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

inline __device__ idaten::SVGFPathTracing::AOV* sampleAov(
	idaten::SVGFPathTracing::AOV* aovs,
	int ix, int iy,
	int width, int height)
{
	ix = clamp(ix, 0, width - 1);
	iy = clamp(iy, 0, height - 1);

	const int idx = getIdx(ix, iy, width);

	return &aovs[idx];
}

__global__ void varianceEstimation(
	cudaSurfaceObject_t dst,
	idaten::SVGFPathTracing::AOV* aovs,
	int width, int height)
{
	int ix = blockIdx.x * blockDim.x + threadIdx.x;
	int iy = blockIdx.y * blockDim.y + threadIdx.y;

	if (ix >= width && iy >= height) {
		return;
	}

	const int idx = getIdx(ix, iy, width);

	float centerDepth = aovs[idx].depth;
	int centerMeshId = aovs[idx].meshid;

	if (centerMeshId < 0) {
		// �w�i�Ȃ̂ŁA���U�̓[��.
		aovs[idx].moments[0] = aovs[idx].moments[1] = make_float4(0, 0, 0, 1);

		surf2Dwrite(
			make_float4(0),
			dst,
			ix * sizeof(float4), iy,
			cudaBoundaryModeTrap);
	}

	float4 centerMomentDirect = aovs[idx].moments[idaten::SVGFPathTracing::LightType::Direct];
	float4 centerMomentIndirect = aovs[idx].moments[idaten::SVGFPathTracing::LightType::Indirect];

	int frame = (int)centerMomentDirect.w;

	centerMomentDirect /= centerMomentDirect.w;
	centerMomentIndirect /= centerMomentIndirect.w;

	// ���U���v�Z.
	float varDirect = centerMomentDirect.x - centerMomentDirect.y * centerMomentDirect.y;
	float varIndirect = centerMomentIndirect.x - centerMomentIndirect.y * centerMomentIndirect.y;

	if (frame < 4) {
		// �ώZ�t���[�������S���� or Disoccluded����Ă���.
		// 7x7birateral filter�ŋP�x���v�Z.

		static const int radius = 3;
		static const float sigmaN = 0.005f;
		static const float sigmaD = 0.005f;
		static const float sigmaS = 8;

		auto centerNormal = aovs[idx].normal;

		float4 sumDirect = make_float4(0, 0, 0, 0);
		float4 sumIndirect = make_float4(0, 0, 0, 0);

		float weight = 0.0f;

		for (int v = -radius; v <= radius; v++)
		{
			for (int u = -radius; u <= radius; u++)
			{
				auto sampleaov = sampleAov(aovs, ix + u, iy + v, width, height);

				auto sampleNml = sampleaov->normal;

				float sampleDepth = sampleaov->depth;
				int sampleMeshId = sampleaov->meshid;

				float n = 1 - dot(sampleNml, centerNormal);
				float Wn = exp(-0.5f * n * n / (sigmaN * sigmaN));

				float d = 1 - min(centerDepth, sampleDepth) / max(centerDepth, sampleDepth);
				float Wd = exp(-0.5f * d * d / (sigmaD * sigmaD));

				float Ws = exp(-0.5f * (u * u + v * v) / (sigmaS * sigmaS));

				float Wm = centerMeshId == sampleMeshId ? 1.0f : 0.0f;

				float W = Ws * Wn * Wd * Wm;

				auto momentDirect = sampleaov->moments[idaten::SVGFPathTracing::LightType::Direct];
				momentDirect /= momentDirect.w;

				auto momentIndirect = sampleaov->moments[idaten::SVGFPathTracing::LightType::Indirect];
				momentIndirect /= momentIndirect.w;

				sumDirect += momentDirect * W;
				sumIndirect += momentIndirect * W;

				weight += W;
			}
		}

		if (weight > 0.0f) {
			sumDirect /= weight;
			sumIndirect /= weight;
		}

		varDirect = sumDirect.x - sumDirect.y * sumDirect.y;
		varIndirect = sumIndirect.x - sumIndirect.y * sumIndirect.y;
	}

	// TODO
	// ���U�̓}�C�i�X�ɂȂ�Ȃ����E�E�E�E
	varDirect = abs(varDirect);
	varIndirect = abs(varIndirect);

	aovs[idx].var[idaten::SVGFPathTracing::LightType::Direct] = varDirect;
	aovs[idx].var[idaten::SVGFPathTracing::LightType::Indirect] = varIndirect;

	surf2Dwrite(
		make_float4(varDirect, varIndirect, 0, 1),
		dst,
		ix * sizeof(float4), iy,
		cudaBoundaryModeTrap);
}

namespace idaten
{
	void SVGFPathTracing::onVarianceEstimation(
		cudaSurfaceObject_t outputSurf,
		int width, int height)
	{
		dim3 block(BLOCK_SIZE, BLOCK_SIZE);
		dim3 grid(
			(width + block.x - 1) / block.x,
			(height + block.y - 1) / block.y);

		auto& curaov = getCurAovs();

		varianceEstimation << <grid, block >> > (
		//varianceEstimation << <1, 1 >> > (
			outputSurf,
			curaov.ptr(),
			width, height);

		checkCudaKernel(varianceEstimation);
	}
}
