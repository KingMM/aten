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

// NOTE
// ddx, ddy
// http://mosapui.blog116.fc2.com/blog-entry-35.html
// https://www.gamedev.net/forums/topic/478820-derivative-instruction-details-ddx-ddy-or-dfdx-dfdy-etc/
// http://d.hatena.ne.jp/umonist/20110616/p1
// http://monsho.blog63.fc2.com/blog-entry-105.html

inline __device__ float ddx(
	int x, int y,
	int w, int h,
	idaten::SVGFPathTracing::AOV* aov)
{
	// NOTE
	// 2x2 pixel‚²‚Æ‚ÉŒvŽZ‚·‚é.

	int leftX = x; 
	int rightX = x + 1;
	if ((x & 0x01) == 1) {
		leftX = x - 1;
		rightX = x;
	}

	rightX = min(rightX, w - 1);

	const int idxL = getIdx(leftX, y, w);
	const int idxR = getIdx(rightX, y, w);

	float left = aov[idxL].depth;
	float right = aov[idxR].depth;

	return right - left;
}

inline __device__ float ddy(
	int x, int y,
	int w, int h,
	idaten::SVGFPathTracing::AOV* aov)
{
	// NOTE
	// 2x2 pixel‚²‚Æ‚ÉŒvŽZ‚·‚é.

	int topY = y;
	int bottomY = y + 1;
	if ((y & 0x01) == 1) {
		topY = y - 1;
		bottomY = y;
	}

	bottomY = min(bottomY, h - 1);

	int idxT = getIdx(x, topY, w);
	int idxB = getIdx(x, bottomY, w);

	float top = aov[idxT].depth;
	float bottom = aov[idxB].depth;

	return bottom - top;
}

template <bool isReferAOV, int Target>
inline __device__ float gaussFilter3x3(
	int ix, int iy,
	int w, int h,
	idaten::SVGFPathTracing::AOV* aov,
	const float2* __restrict__ var)
{
	static const float kernel[] = {
		1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0,
		1.0 / 8.0,  1.0 / 4.0, 1.0 / 8.0,
		1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0,
	};

	float sum = 0;

	int pos = 0;

	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			int xx = clamp(ix + x, 0, w - 1);
			int yy = clamp(iy + y, 0, h - 1);

			int idx = getIdx(xx, yy, w);

			float tmp;
			if (isReferAOV) {
				tmp = aov[idx].var[Target];
			}
			else {
				tmp = Target == 0 ? var[idx].x : var[idx].y;
			}

			sum += kernel[pos] * tmp;

			pos++;
		}
	}

	return sum;
}

template <bool isFirstIter, bool isFinalIter>
__global__ void atrousFilter(
	cudaSurfaceObject_t dst,
	idaten::SVGFPathTracing::Store* tmpBuffer,
	idaten::SVGFPathTracing::AOV* aovs,
	const idaten::SVGFPathTracing::Store* __restrict__ clrBuffer,
	idaten::SVGFPathTracing::Store* nextClrBuffer,
	const float2* __restrict__ varBuffer,
	float2* nextVarBuffer,
	int stepScale,
	float thresholdTemporalWeight,
	int radiusScale,
	int width, int height)
{
	int ix = blockIdx.x * blockDim.x + threadIdx.x;
	int iy = blockIdx.y * blockDim.y + threadIdx.y;

	if (ix >= width && iy >= height) {
		return;
	}

	const int idx = getIdx(ix, iy, width);

	auto centerNormal = aovs[idx].normal;

	float centerDepth = aovs[idx].depth;
	int centerMeshId = aovs[idx].meshid;

	float tmpDdzX = ddx(ix, iy, width, height, aovs);
	float tmpDdzY = ddy(ix, iy, width, height, aovs);
	float2 ddZ = make_float2(tmpDdzX, tmpDdzY);

	float4 centerColorDirect;
	float4 centerColorIndirect;

	if (isFirstIter) {
		centerColorDirect = aovs[idx].color[idaten::SVGFPathTracing::LightType::Direct];
		centerColorIndirect = aovs[idx].color[idaten::SVGFPathTracing::LightType::Indirect];
	}
	else {
		centerColorDirect = clrBuffer[idx].f[idaten::SVGFPathTracing::LightType::Direct];
		centerColorIndirect = clrBuffer[idx].f[idaten::SVGFPathTracing::LightType::Indirect];
	}

	if (centerMeshId < 0) {
		// ”wŒi‚È‚Ì‚ÅA‚»‚Ì‚Ü‚Üo—Í‚µ‚ÄI—¹.
		nextClrBuffer[idx].f[idaten::SVGFPathTracing::LightType::Direct] = centerColorDirect;

		if (isFinalIter) {
			centerColorDirect *= aovs[idx].texclr;

			surf2Dwrite(
				centerColorDirect,
				dst,
				ix * sizeof(float4), iy,
				cudaBoundaryModeTrap);
		}

		return;
	}

	float centerLumDirect = AT_NAME::color::luminance(centerColorDirect.x, centerColorDirect.y, centerColorDirect.z);
	float centerLumIndirect = AT_NAME::color::luminance(centerColorIndirect.x, centerColorIndirect.y, centerColorIndirect.z);

	// ƒKƒEƒXƒtƒBƒ‹ƒ^3x3
	float gaussedVarLumDirect;
	float gaussedVarLumIndirect;
	
	if (isFirstIter) {
		gaussedVarLumDirect = gaussFilter3x3<true, idaten::SVGFPathTracing::LightType::Direct>(ix, iy, width, height, aovs, varBuffer);
		gaussedVarLumIndirect = gaussFilter3x3<true, idaten::SVGFPathTracing::LightType::Indirect>(ix, iy, width, height, aovs, varBuffer);
	}
	else {
		gaussedVarLumDirect = gaussFilter3x3<false, idaten::SVGFPathTracing::LightType::Direct>(ix, iy, width, height, aovs, varBuffer);
		gaussedVarLumIndirect = gaussFilter3x3<false, idaten::SVGFPathTracing::LightType::Indirect>(ix, iy, width, height, aovs, varBuffer);
	}

	float sqrGaussedVarLumDirect = sqrt(gaussedVarLumDirect);
	float sqrGaussedVarLumIndirect = sqrt(gaussedVarLumIndirect);

	static const float sigmaZ = 1.0f;
	static const float sigmaN = 128.0f;
	static const float sigmaL = 4.0f;

	float2 p = make_float2(ix, iy);

	// NOTE
	// 5x5

	float4 sumClrDirect = make_float4(0, 0, 0, 0);
	float weightClrDirect = 0;

	float4 sumClrIndirect = make_float4(0, 0, 0, 0);
	float weightClrIndirect = 0;

	float sumVarDirect = 0;
	float weightVarDirect = 0;

	float sumVarIndirect = 0;
	float weightVarIndirect = 0;

	int pos = 0;

	static const float h[] = {
		1.0 / 256.0, 1.0 / 64.0, 3.0 / 128.0, 1.0 / 64.0, 1.0 / 256.0,
		1.0 / 64.0,  1.0 / 16.0, 3.0 / 32.0,  1.0 / 16.0, 1.0 / 64.0,
		3.0 / 128.0, 3.0 / 32.0, 9.0 / 64.0,  3.0 / 32.0, 3.0 / 128.0,
		1.0 / 64.0,  1.0 / 16.0, 3.0 / 32.0,  1.0 / 16.0, 1.0 / 64.0,
		1.0 / 256.0, 1.0 / 64.0, 3.0 / 128.0, 1.0 / 64.0, 1.0 / 256.0,
	};

	int R = 2;

	if (isFirstIter) {
		if (aovs[idx].temporalWeight < thresholdTemporalWeight) {
			R *= radiusScale;
		}
	}

	for (int y = -R; y <= R; y++) {
		for (int x = -R; x <= R; x++) {
			int xx = clamp(ix + x * stepScale, 0, width - 1);
			int yy = clamp(iy + y * stepScale, 0, height - 1);

			float2 q = make_float2(xx, yy);

			const int qidx = getIdx(xx, yy, width);

			float depth = aovs[qidx].depth;
			int meshid = aovs[qidx].meshid;

			if (meshid != centerMeshId) {
				continue;
			}

			auto normal = aovs[qidx].normal;

			float4 colorDirect;
			float4 colorIndirect;

			float varDirect;
			float varIndirect;

			if (isFirstIter) {
				colorDirect = aovs[qidx].color[idaten::SVGFPathTracing::LightType::Direct];
				varDirect = aovs[qidx].var[idaten::SVGFPathTracing::LightType::Direct];

				colorIndirect = aovs[qidx].color[idaten::SVGFPathTracing::LightType::Indirect];
				varIndirect = aovs[qidx].var[idaten::SVGFPathTracing::LightType::Indirect];
			}
			else {
				colorDirect = clrBuffer[qidx].f[idaten::SVGFPathTracing::LightType::Direct];
				varDirect = varBuffer[qidx].x;

				colorIndirect = clrBuffer[qidx].f[idaten::SVGFPathTracing::LightType::Indirect];
				varIndirect = varBuffer[qidx].y;
			}

			float lumDirect = AT_NAME::color::luminance(colorDirect.x, colorDirect.y, colorDirect.z);
			float lumIndirect = AT_NAME::color::luminance(colorIndirect.x, colorIndirect.y, colorIndirect.z);

			float Wz = min(exp(-abs(centerDepth - depth) / (sigmaZ * abs(dot(ddZ, p - q)) + 0.000001f)), 1.0f);

			float Wn = pow(max(0.0f, dot(centerNormal, normal)), sigmaN);

			float Wl_Direct = min(exp(-abs(centerLumDirect - lumDirect) / (sigmaL * sqrGaussedVarLumDirect + 0.000001f)), 1.0f);
			float Wl_Indirect = min(exp(-abs(centerLumIndirect - lumIndirect) / (sigmaL * sqrGaussedVarLumIndirect + 0.000001f)), 1.0f);

			float Wm = meshid == centerMeshId ? 1.0f : 0.0f;

			float W_Direct = Wz * Wn * Wm * Wl_Direct;
			float W_Indirect = Wz * Wn * Wm * Wl_Indirect;
			
			sumClrDirect += h[pos] * W_Direct * colorDirect;
			weightClrDirect += h[pos] * W_Direct;

			sumVarDirect += (h[pos] * h[pos]) * (W_Direct * W_Direct) * varDirect;
			weightVarDirect += h[pos] * W_Direct;

			sumClrIndirect += h[pos] * W_Indirect * colorIndirect;
			weightClrIndirect += h[pos] * W_Indirect;

			sumVarIndirect += (h[pos] * h[pos]) * (W_Indirect * W_Indirect) * varIndirect;
			weightVarIndirect += h[pos] * W_Indirect;

			pos++;
		}
	}

	if (weightClrDirect > 0.0) {
		sumClrDirect /= weightClrDirect;
	}
	if (weightVarDirect > 0.0) {
		sumVarDirect /= (weightVarDirect * weightVarDirect);
	}

	if (weightClrIndirect > 0.0) {
		sumClrIndirect /= weightClrIndirect;
	}
	if (weightVarIndirect > 0.0) {
		sumVarIndirect /= (weightVarIndirect * weightVarIndirect);
	}

	nextClrBuffer[idx].f[idaten::SVGFPathTracing::LightType::Direct] = sumClrDirect;
	nextVarBuffer[idx].x = sumVarDirect;

	nextClrBuffer[idx].f[idaten::SVGFPathTracing::LightType::Indirect] = sumClrIndirect;
	nextVarBuffer[idx].y = sumVarIndirect;

	if (isFirstIter) {
		// Store color temporary.
		tmpBuffer[idx].f[idaten::SVGFPathTracing::LightType::Direct] = sumClrDirect;
		tmpBuffer[idx].f[idaten::SVGFPathTracing::LightType::Indirect] = sumClrIndirect;
	}	
}

__global__ void copyFromBufferToAov(
	const idaten::SVGFPathTracing::Store* __restrict__ src,
	idaten::SVGFPathTracing::AOV* aovs,
	int width, int height)
{
	int ix = blockIdx.x * blockDim.x + threadIdx.x;
	int iy = blockIdx.y * blockDim.y + threadIdx.y;

	if (ix >= width && iy >= height) {
		return;
	}

	const int idx = getIdx(ix, iy, width);

	aovs[idx].color[0] = src[idx].f[0];
	aovs[idx].color[1] = src[idx].f[1];
}

__global__ void modulateTexColor(
	cudaSurfaceObject_t dst,
	const idaten::SVGFPathTracing::Store* __restrict__ buffer,
	const idaten::SVGFPathTracing::AOV* __restrict__ aovs,
	int width, int height)
{
	int ix = blockIdx.x * blockDim.x + threadIdx.x;
	int iy = blockIdx.y * blockDim.y + threadIdx.y;

	if (ix >= width && iy >= height) {
		return;
	}

	const int idx = getIdx(ix, iy, width);

	auto direct = buffer[idx].f[idaten::SVGFPathTracing::LightType::Direct];
	auto indirect = buffer[idx].f[idaten::SVGFPathTracing::LightType::Indirect];

	auto clr = (direct + indirect) * aovs[idx].texclr;

	surf2Dwrite(
		clr,
		dst,
		ix * sizeof(float4), iy,
		cudaBoundaryModeTrap);
}

namespace idaten
{
	void SVGFPathTracing::onAtrousFilter(
		cudaSurfaceObject_t outputSurf,
		int width, int height)
	{
		static const int ITER = 5;

		dim3 block(BLOCK_SIZE, BLOCK_SIZE);
		dim3 grid(
			(width + block.x - 1) / block.x,
			(height + block.y - 1) / block.y);

		auto& curaov = getCurAovs();

		int cur = 0;
		int next = 1;

		for (int i = 0; i < ITER; i++) {
			int stepScale = 1 << i;

			if (i == 0) {
				// First.
				atrousFilter<true, false> << <grid, block >> > (
					outputSurf,
					m_tmpBuf.ptr(),
					curaov.ptr(),
					m_atrousClr[cur].ptr(), m_atrousClr[next].ptr(),
					m_atrousVar[cur].ptr(), m_atrousVar[next].ptr(),
					stepScale,
					m_thresholdTemporalWeight, m_atrousTapRadiusScale,
					width, height);
				checkCudaKernel(atrousFilter);
			}
#if 1
			else if (i == ITER - 1) {
				// Final.
				atrousFilter<false, true> << <grid, block >> > (
					outputSurf,
					m_tmpBuf.ptr(),
					curaov.ptr(),
					m_atrousClr[cur].ptr(), m_atrousClr[next].ptr(),
					m_atrousVar[cur].ptr(), m_atrousVar[next].ptr(),
					stepScale,
					m_thresholdTemporalWeight, m_atrousTapRadiusScale,
					width, height);
				checkCudaKernel(atrousFilter);
			}
			else {
				atrousFilter<false, false> << <grid, block >> > (
					outputSurf,
					m_tmpBuf.ptr(),
					curaov.ptr(),
					m_atrousClr[cur].ptr(), m_atrousClr[next].ptr(),
					m_atrousVar[cur].ptr(), m_atrousVar[next].ptr(),
					stepScale,
					m_thresholdTemporalWeight, m_atrousTapRadiusScale,
					width, height);
				checkCudaKernel(atrousFilter);
			}
#endif

			cur = next;
			next = 1 - cur;
		}

		modulateTexColor << < grid, block >> > (
			outputSurf,
			m_atrousClr[cur].ptr(),
			curaov.ptr(),
			width, height);
	}

	void SVGFPathTracing::copyFromTmpBufferToAov(int width, int height)
	{
		dim3 block(BLOCK_SIZE, BLOCK_SIZE);
		dim3 grid(
			(width + block.x - 1) / block.x,
			(height + block.y - 1) / block.y);

		auto& curaov = getCurAovs();

		// Copy color from temporary buffer to AOV buffer for next temporal reprojection.
		copyFromBufferToAov << <grid, block >> > (
			m_tmpBuf.ptr(),
			curaov.ptr(),
			width, height);
		checkCudaKernel(copyFromBufferToAov);
	}
}
