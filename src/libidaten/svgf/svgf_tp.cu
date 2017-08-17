#include "svgf/svgf_pt.h"

#include "kernel/context.cuh"
#include "kernel/light.cuh"
#include "kernel/material.cuh"
#include "kernel/intersect.cuh"
#include "kernel/bvh.cuh"
#include "kernel/compaction.h"
#include "kernel/pt_common.h"

#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include "cuda/helper_math.h"
#include "cuda/cudautil.h"
#include "cuda/cudamemory.h"

#include "aten4idaten.h"

inline __device__ void computePrevScreenPos(
	int ix, int iy,
	float centerDepth,
	int width, int height,
	aten::vec4* prevPos,
	const aten::mat4* __restrict__ mtxs)
{
	// NOTE
	// Pview = (Xview, Yview, Zview, 1)
	// mtxV2C = W 0 0  0
	//          0 H 0  0
	//          0 0 A  B
	//          0 0 -1 0
	// mtxV2C * Pview = (Xclip, Yclip, Zclip, Wclip) = (Xclip, Yclip, Zclip, Zview)
	//  Wclip = Zview = depth
	// Xscr = Xclip / Wclip = Xclip / Zview = Xclip / depth
	// Yscr = Yclip / Wclip = Yclip / Zview = Yclip / depth
	//
	// Xscr * depth = Xclip
	// Xview = mtxC2V * Xclip

	const aten::mat4 mtxC2V = mtxs[0];
	const aten::mat4 mtxPrevV2C = mtxs[1];

	float2 uv = make_float2(ix + 0.5, iy + 0.5);
	uv /= make_float2(width - 1, height - 1);	// [0, 1]
	uv = uv * 2.0f - 1.0f;	// [0, 1] -> [-1, 1]

	aten::vec4 pos(uv.x, uv.y, 0, 0);

	// Screen-space -> Clip-space.
	pos.x *= centerDepth;
	pos.y *= centerDepth;

	// Clip-space -> View-space
	pos = mtxC2V.apply(pos);
	pos.z = -centerDepth;
	pos.w = 1.0;

	// Reproject previous screen position.
	*prevPos = mtxPrevV2C.apply(pos);
	*prevPos /= prevPos->w;

	*prevPos = *prevPos * 0.5 + 0.5;	// [-1, 1] -> [0, 1]
}

__global__ void temporalReprojection(
	const idaten::SVGFPathTracing::Path* __restrict__ paths,
	const aten::CameraParameter* __restrict__ camera,
	cudaSurfaceObject_t* curAovs,
	cudaSurfaceObject_t* prevAovs,
	const aten::mat4* __restrict__ mtxs,
	cudaSurfaceObject_t dst,
	int width, int height)
{
	const int ix = blockIdx.x * blockDim.x + threadIdx.x;
	const int iy = blockIdx.y * blockDim.y + threadIdx.y;

	if (ix >= width && iy >= height) {
		return;
	}

	const auto idx = getIdx(ix, iy, width);

	const auto path = paths[idx];

	float4 depth_meshid;
	surf2Dread(
		&depth_meshid,
		curAovs[idaten::SVGFPathTracing::AOVType::depth_meshid],
		ix * sizeof(float4), iy);

	const float centerDepth = aten::clamp(depth_meshid.x, camera->znear, camera->zfar);
	const int centerMeshId = (int)depth_meshid.y;

	// ����̃t���[���̃s�N�Z���J���[.
	float4 curColor = make_float4(path.contrib.x, path.contrib.y, path.contrib.z, 0) / path.samples;
	curColor.w = 1;

	if (centerMeshId < 0) {
		// �w�i�Ȃ̂ŁA���̂܂܏o�͂��ďI���.
		surf2Dwrite(
			curColor,
			dst,
			ix * sizeof(float4), iy,
			cudaBoundaryModeTrap);

		return;
	}

	float4 centerNormal;
	surf2Dread(
		&centerNormal,
		curAovs[idaten::SVGFPathTracing::AOVType::normal],
		ix * sizeof(float4), iy);

	// [0, 1] -> [-1, 1]
	centerNormal = 2 * centerNormal - 1;
	centerNormal.w = 0;

	float4 sum = make_float4(0, 0, 0, 0);
	float weight = 0.0f;

	float4 prevDepthMeshId;
	float4 prevNormal;

	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			int xx = clamp(ix + x, 0, width - 1);
			int yy = clamp(iy + y, 0, height - 1);

			// �O�̃t���[���̃N���b�v��ԍ��W���v�Z.
			aten::vec4 prevPos;
			computePrevScreenPos(
				xx, yy,
				centerDepth,
				width, height,
				&prevPos,
				mtxs);

			// [0, 1]�͈͓̔��ɓ����Ă��邩.
			bool isInsideX = (0.0 <= prevPos.x) && (prevPos.x <= 1.0);
			bool isInsideY = (0.0 <= prevPos.y) && (prevPos.y <= 1.0);

			if (isInsideX && isInsideY) {
				// �O�̃t���[���̃X�N���[�����W.
				int px = (int)(prevPos.x * width - 0.5f);
				int py = (int)(prevPos.y * height - 0.5f);

				px = clamp(px, 0, width - 1);
				py = clamp(py, 0, height - 1);

				surf2Dread(
					&prevDepthMeshId,
					prevAovs[idaten::SVGFPathTracing::AOVType::depth_meshid],
					px * sizeof(float4), py);

				const float prevDepth = aten::clamp(depth_meshid.x, camera->znear, camera->zfar);
				const int prevMeshId = (int)depth_meshid.y;

				surf2Dread(
					&prevNormal,
					prevAovs[idaten::SVGFPathTracing::AOVType::normal],
					px * sizeof(float4), py);

				// [0, 1] -> [-1, 1]
				prevNormal = 2 * prevNormal - 1;
				prevNormal.w = 0;

				// TODO
				// �������b�V����ł����C�g�̂��΂̖��邭�Ȃ����s�N�Z�����E���Ă��܂��ꍇ�̑΍􂪕K�v.

				static const float zThreshold = 0.05f;
				static const float nThreshold = 0.98f;

				float Wz = clamp((zThreshold - abs(1 - centerDepth / prevDepth)) / zThreshold, 0.0f, 1.0f);
				float Wn = clamp((dot(centerNormal, prevNormal) - nThreshold) / (1.0f - nThreshold), 0.0f, 1.0f);
				float Wm = centerMeshId == prevMeshId ? 1.0f : 0.0f;

				// �O�̃t���[���̃s�N�Z���J���[���擾.
				float4 prev;
				surf2Dread(
					&prev, 
					prevAovs[idaten::SVGFPathTracing::AOVType::clr_history],
					px * sizeof(float4), py);

				float W = Wz * Wn * Wm;
				sum += prev * W;
				weight += W;
			}
		}
	}

	if (weight > 0.0f) {
		sum /= weight;
		curColor = 0.2 * curColor + 0.8 * sum;
	}

	surf2Dwrite(
		curColor,
		curAovs[idaten::SVGFPathTracing::AOVType::clr_history],
		ix * sizeof(float4), iy,
		cudaBoundaryModeTrap);

	surf2Dwrite(
		curColor,
		dst,
		ix * sizeof(float4), iy,
		cudaBoundaryModeTrap);
}

namespace idaten
{
	void SVGFPathTracing::onTemporalReprojection(
		cudaSurfaceObject_t outputSurf,
		int width, int height)
	{
		dim3 block(BLOCK_SIZE, BLOCK_SIZE);
		dim3 grid(
			(width + block.x - 1) / block.x,
			(height + block.y - 1) / block.y);

		auto& curaov = getCurAovs();
		auto& prevaov = getPrevAovs();

		aten::mat4 mtxs[2] = {
			m_mtxC2V,
			m_mtxPrevV2C,
		};

		m_mtxs.init(sizeof(aten::mat4) * AT_COUNTOF(mtxs));
		m_mtxs.writeByNum(mtxs, AT_COUNTOF(mtxs));

		temporalReprojection << <grid, block >> > (
			m_paths.ptr(),
			m_cam.ptr(),
			curaov.ptr(),
			prevaov.ptr(),
			m_mtxs.ptr(),
			outputSurf,
			width, height);

		m_mtxs.reset();
	}
}
