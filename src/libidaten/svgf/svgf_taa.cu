#include "svgf/svgf_pt.h"
#include "kernel/pt_common.h"

#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include "cuda/helper_math.h"
#include "cuda/cudautil.h"
#include "cuda/cudamemory.h"

#include "aten4idaten.h"

inline __device__ float3 clipAABB(
	float3 aabb_min,
	float3 aabb_max,
	float3 q)
{
	float3 center = 0.5 * (aabb_max + aabb_min);

	float3 halfsize = 0.5 * (aabb_max - aabb_min) + 0.00000001f;

	// ���S����̑��Έʒu.
	float3 clip = q - center;

	// ���Έʒu�̐��K��.
	float3 unit = clip / halfsize;

	float3 abs_unit = make_float3(fabsf(unit.x), fabsf(unit.y), fabsf(unit.z));

	float ma_unit = max(abs_unit.x, max(abs_unit.y, abs_unit.z));

	if (ma_unit > 1.0) {
		// �N���b�v�ʒu.
		return center + clip / ma_unit;
	}
	else {
		// point inside aabb
		return q;
	}
}

inline __device__ float3 sampleColor(
	const idaten::SVGFPathTracing::AOV* __restrict__ aovs,
	int width, int height,
	int2 p)
{
	int ix = clamp(p.x, 0, width);
	int iy = clamp(p.y, 0, height);

	auto idx = getIdx(ix, iy, width);

	auto c = aovs[idx].color;

	return make_float3(c.x, c.y, c.z);
}

inline __device__ float3 sampleColor(
	const idaten::SVGFPathTracing::AOV* __restrict__ aovs,
	int width, int height,
	int2 p,
	float2 jitter)
{
	auto pclr = sampleColor(aovs, width, height, p);
	auto jclr = sampleColor(aovs, width, height, make_int2(p.x - 1, p.y - 1));

	auto f = sqrtf(dot(jitter, jitter));

	auto ret = lerp(jclr, pclr, f);

	return ret;
}

inline __device__ float3 min(float3 a, float3 b)
{
	return make_float3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
}

inline __device__ float3 max(float3 a, float3 b)
{
	return make_float3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
}

inline __device__ float3 clipColor(
	float3 clr,
	int ix, int iy,
	int width, int height,
	const idaten::SVGFPathTracing::AOV* __restrict__ aovs)
{
	int2 du = make_int2(1, 0);
	int2 dv = make_int2(0, 1);

	int2 uv = make_int2(ix, iy);

	float3 ctl = sampleColor(aovs, width, height, uv - dv - du);
	float3 ctc = sampleColor(aovs, width, height, uv - dv);
	float3 ctr = sampleColor(aovs, width, height, uv - dv + du);
	float3 cml = sampleColor(aovs, width, height, uv - du);
	float3 cmc = sampleColor(aovs, width, height, uv);
	float3 cmr = sampleColor(aovs, width, height, uv + du);
	float3 cbl = sampleColor(aovs, width, height, uv + dv - du);
	float3 cbc = sampleColor(aovs, width, height, uv + dv);
	float3 cbr = sampleColor(aovs, width, height, uv + dv + du);

	float3 cmin = min(ctl, min(ctc, min(ctr, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
	float3 cmax = max(ctl, max(ctc, max(ctr, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));

	float3 cavg = (ctl + ctc + ctr + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;

	float3 cmin5 = min(ctc, min(cml, min(cmc, min(cmr, cbc))));
	float3 cmax5 = max(ctc, max(cml, max(cmc, max(cmr, cbc))));
	float3 cavg5 = (ctc + cml + cmc + cmr + cbc) / 5.0;
	cmin = 0.5 * (cmin + cmin5);
	cmax = 0.5 * (cmax + cmax5);
	cavg = 0.5 * (cavg + cavg5);

	auto ret = clipAABB(cmin, cmax, clr);

	return ret;
}

inline __device__ float4 PDnrand4(float2 n)
{
	return fracf(sin(dot(n, make_float2(12.9898f, 78.233f))) * make_float4(43758.5453f, 28001.8384f, 50849.4141f, 12996.89f));
}

inline __device__ float4 PDsrand4(float2 n)
{
	return PDnrand4(n) * 2 - 1;
}

// TODO
// temporal reporjection�̂��̂Ɠ��ꂷ��ׂ�.
inline __device__ void computePrevScreenPos(
	int ix, int iy,
	float centerDepth,
	int width, int height,
	aten::vec4* prevPos,
	const aten::mat4 mtxC2V,
	const aten::mat4 mtxPrevV2C)
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

template <bool showDiff>
__global__ void temporalAA(
	cudaSurfaceObject_t dst,
	idaten::SVGFPathTracing::Path* paths,
	const idaten::SVGFPathTracing::AOV* __restrict__ curAovs,
	const idaten::SVGFPathTracing::AOV* __restrict__ prevAovs,
	const aten::mat4 mtxC2V,
	const aten::mat4 mtxPrevV2C,
	float sinTime,
	int width, int height)
{
	int ix = blockIdx.x * blockDim.x + threadIdx.x;
	int iy = blockIdx.y * blockDim.y + threadIdx.y;

	if (ix >= width && iy >= height) {
		return;
	}

	const int idx = getIdx(ix, iy, width);

	auto p = make_int2(ix, iy);

	auto* sampler = &paths[idx].sampler;

	auto r1 = sampler->nextSample();
	auto r2 = sampler->nextSample();
	float2 jitter = make_float2(r1, r2);

	auto tmp = sampleColor(curAovs, width, height, p);

	auto clr0 = sampleColor(curAovs, width, height, p, jitter);

	auto centerDepth = curAovs[idx].depth;

	aten::vec4 prevPos;
	computePrevScreenPos(
		ix, iy,
		centerDepth,
		width, height,
		&prevPos,
		mtxC2V, mtxPrevV2C);

	// [0, 1]�͈͓̔��ɓ����Ă��邩.
	bool isInsideX = (0.0 <= prevPos.x) && (prevPos.x <= 1.0);
	bool isInsideY = (0.0 <= prevPos.y) && (prevPos.y <= 1.0);

	if (isInsideX && isInsideY) {
		// �O�̃t���[���̃X�N���[�����W.
		int px = (int)(prevPos.x * width - 0.5f);
		int py = (int)(prevPos.y * height - 0.5f);

		auto clr1 = sampleColor(prevAovs, width, height, make_int2(px, py));

		clr1 = clipColor(clr1, ix, iy, width, height, curAovs);

		float lum0 = AT_NAME::color::luminance(clr0.x, clr0.y, clr0.z);
		float lum1 = AT_NAME::color::luminance(clr1.x, clr1.y, clr1.z);

		float unbiased_diff = abs(lum0 - lum1) / max(lum0, max(lum1, 0.2f));
		float unbiased_weight = 1.0 - unbiased_diff;
		float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
		float k_feedback = lerp(0.8, 0.2, unbiased_weight_sqr);

		auto c = lerp(clr0, clr1, k_feedback);

		auto noise4 = PDsrand4(make_float2(ix, iy) + sinTime + 0.6959174f) / 510.0f;
		noise4.w = 0;

		auto f = make_float4(c.x, c.y, c.z, 1) + noise4;

		if (showDiff) {
			f = make_float4(abs(f.x - tmp.x), abs(f.y - tmp.y), abs(f.z - tmp.z), 1);
		}

		surf2Dwrite(
			f,
			dst,
			ix * sizeof(float4), iy,
			cudaBoundaryModeTrap);
	}
	else if (showDiff) {
		surf2Dwrite(
			make_float4(0, 0, 0, 1),
			dst,
			ix * sizeof(float4), iy,
			cudaBoundaryModeTrap);
	}
}

namespace idaten
{
	void SVGFPathTracing::onTAA(
		cudaSurfaceObject_t outputSurf,
		int width, int height)
	{
		if (isFirstFrame()) {
			return;
		}

		if (isEnableTAA()) {
			auto& curaov = getCurAovs();
			auto& prevaov = getPrevAovs();

			dim3 block(BLOCK_SIZE, BLOCK_SIZE);
			dim3 grid(
				(width + block.x - 1) / block.x,
				(height + block.y - 1) / block.y);

			float sintime = aten::abs(aten::sin((m_frame & 0xf) * AT_MATH_PI));

			if (canShowTAADiff()) {
				temporalAA<true> << <grid, block >> > (
					outputSurf,
					m_paths.ptr(),
					curaov.ptr(),
					prevaov.ptr(),
					m_mtxC2V,
					m_mtxPrevV2C,
					sintime,
					width, height);
			}
			else {
				temporalAA<false> << <grid, block >> > (
					outputSurf,
					m_paths.ptr(),
					curaov.ptr(),
					prevaov.ptr(),
					m_mtxC2V,
					m_mtxPrevV2C,
					sintime,
					width, height);
			}

			checkCudaKernel(temporalAA);
		}
	}
}