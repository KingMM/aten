#include "svgf/svgf.h"

#include "kernel/context.cuh"
#include "kernel/light.cuh"
#include "kernel/material.cuh"
#include "kernel/intersect.cuh"
#include "kernel/bvh.cuh"
#include "kernel/StreamCompaction.h"
#include "kernel/pt_common.h"
#include "cuda/cudadefs.h"
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
    const float4* __restrict__ aovNormalDepth)
{
    // NOTE
    // 2x2 pixelごとに計算する.

    int leftX = x; 
    int rightX = x + 1;

#if 0
    if ((x & 0x01) == 1) {
        leftX = x - 1;
        rightX = x;
    }
#else
    int offset = (x & 0x01);
    leftX -= offset;
    rightX -= offset;
#endif

    rightX = min(rightX, w - 1);

    const int idxL = getIdx(leftX, y, w);
    const int idxR = getIdx(rightX, y, w);


    float left = aovNormalDepth[idxL].w;
    float right = aovNormalDepth[idxR].w;

    return right - left;
}

inline __device__ float ddy(
    int x, int y,
    int w, int h,
    const float4* __restrict__ aovNormalDepth)
{
    // NOTE
    // 2x2 pixelごとに計算する.

    int topY = y;
    int bottomY = y + 1;

#if 0
    if ((y & 0x01) == 1) {
        topY = y - 1;
        bottomY = y;
    }
#else
    int offset = (y & 0x01);
    topY -= offset;
    bottomY -= offset;
#endif

    bottomY = min(bottomY, h - 1);

    int idxT = getIdx(x, topY, w);
    int idxB = getIdx(x, bottomY, w);

    float top = aovNormalDepth[idxT].w;
    float bottom = aovNormalDepth[idxB].w;

    return bottom - top;
}

inline __device__ float gaussFilter3x3(
    int ix, int iy,
    int w, int h,
    const float4* __restrict__ var)
{
    static const float kernel[] = {
        1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0,
        1.0 / 8.0,  1.0 / 4.0, 1.0 / 8.0,
        1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0,
    };

    static const int offsetx[] = {
        -1, 0, 1,
        -1, 0, 1,
        -1, 0, 1,
    };

    static const int offsety[] = {
        -1, -1, -1,
        0, 0, 0,
        1, 1, 1,
    };

    float sum = 0;

    int pos = 0;

#pragma unroll
    for (int i = 0; i < 9; i++) {
        int xx = clamp(ix + offsetx[i], 0, w - 1);
        int yy = clamp(iy + offsety[i], 0, h - 1);

        int idx = getIdx(xx, yy, w);

        float tmp = var[idx].w;

        sum += kernel[pos] * tmp;

        pos++;
    }

    return sum;
}

// https://software.intel.com/en-us/node/503873
inline __device__ float4 RGB2YCoCg(float4 c)
{
    // Y = R/4 + G/2 + B/4
    // Co = R/2 - B/2
    // Cg = -R/4 + G/2 - B/4
    return make_float4(
        c.x / 4.0 + c.y / 2.0 + c.z / 4.0,
        c.x / 2.0 - c.z / 2.0,
        -c.x / 4.0 + c.y / 2.0 - c.z / 4.0,
        c.w);
}

inline __device__ float4 YCoCg2RGB(float4 c)
{
    // R = Y + Co - Cg
    // G = Y + Cg
    // B = Y - Co - Cg
    return clamp(
        make_float4(
            c.x + c.y - c.z,
            c.x + c.z,
            c.x - c.y - c.z,
            c.w),
        0, 1);
}

// http://graphicrants.blogspot.jp/2013/12/tone-mapping.html
inline __device__ float4 map(float4 clr)
{
    float lum = RGB2YCoCg(clr).x;
    return clr / (1 + lum);
}

inline __device__ float4 unmap(float4 clr)
{
    float lum = RGB2YCoCg(clr).x;
    return clr / (1 - lum);
}

inline __device__ float _C(float3 x1, float3 x2, float sigma)
{
    float a = length(x1 - x2) / sigma;
    a *= a;
    return expf(-0.5f * a);
}

inline __device__ float _C(float x1, float x2, float sigma)
{
    float a = fabs(x1 - x2) / sigma;
    a *= a;
    return expf(-0.5f * a);
}


__global__ void atrousFilter(
    idaten::TileDomain tileDomain,
    bool isFirstIter, bool isFinalIter,
    cudaSurfaceObject_t dst,
    float4* tmpBuffer,
    const float4* __restrict__ aovNormalDepth,
    const float4* __restrict__ aovTexclrMeshid,
    const float4* __restrict__ aovColorVariance,
    const float4* __restrict__ aovMomentTemporalWeight,
    const float4* __restrict__ clrVarBuffer,
    float4* nextClrVarBuffer,
    int stepScale,
    int width, int height)
{
    int ix = blockIdx.x * blockDim.x + threadIdx.x;
    int iy = blockIdx.y * blockDim.y + threadIdx.y;

    if (ix >= tileDomain.w || iy >= tileDomain.h) {
        return;
    }

    ix += tileDomain.x;
    iy += tileDomain.y;

    const int idx = getIdx(ix, iy, width);

    auto normalDepth = aovNormalDepth[idx];
    auto texclrMeshid = aovTexclrMeshid[idx];

    float3 centerNormal = make_float3(normalDepth.x, normalDepth.y, normalDepth.z);
    float centerDepth = normalDepth.w;
    int centerMeshId = (int)texclrMeshid.w;

#if 0
    float tmpDdzX = ddx(ix, iy, width, height, aovNormalDepth);
    float tmpDdzY = ddy(ix, iy, width, height, aovNormalDepth);
    float2 ddZ = make_float2(tmpDdzX, tmpDdzY);
#endif

    float4 centerColor;

    if (isFirstIter) {
        centerColor = aovColorVariance[idx];
    }
    else {
        centerColor = clrVarBuffer[idx];
    }

    if (centerMeshId < 0) {
        // 背景なので、そのまま出力して終了.
        nextClrVarBuffer[idx] = make_float4(centerColor.x, centerColor.y, centerColor.z, 0.0f);

        if (isFinalIter) {
            centerColor *= texclrMeshid;

            surf2Dwrite(
                centerColor,
                dst,
                ix * sizeof(float4), iy,
                cudaBoundaryModeTrap);
        }

        return;
    }

    if (isFirstIter) {
        centerColor = map(centerColor);
        centerColor = RGB2YCoCg(centerColor);
    }

    float centerLum = AT_NAME::color::luminance(centerColor.x, centerColor.y, centerColor.z);

    // ガウスフィルタ3x3
    float gaussedVarLum;
    
    if (isFirstIter) {
        gaussedVarLum = gaussFilter3x3(ix, iy, width, height, aovColorVariance);
    }
    else {
        gaussedVarLum = gaussFilter3x3(ix, iy, width, height, clrVarBuffer);
    }

    float sqrGaussedVarLum = sqrt(gaussedVarLum);

    static const float sigmaZ = 1.0f;
    static const float sigmaN = 128.0f;
    static const float sigmaL = 4.0f;

    float2 p = make_float2(ix, iy);

    // NOTE
    // 5x5

    float4 sumC = make_float4(0, 0, 0, 0);
    float weightC = 0;

    float sumV = 0;
    float weightV = 0;

    int pos = 0;

    static const float h[] = {
        1.0 / 256.0, 1.0 / 64.0, 3.0 / 128.0, 1.0 / 64.0, 1.0 / 256.0,
        1.0 / 64.0,  1.0 / 16.0, 3.0 / 32.0,  1.0 / 16.0, 1.0 / 64.0,
        3.0 / 128.0, 3.0 / 32.0, 9.0 / 64.0,  3.0 / 32.0, 3.0 / 128.0,
        1.0 / 64.0,  1.0 / 16.0, 3.0 / 32.0,  1.0 / 16.0, 1.0 / 64.0,
        1.0 / 256.0, 1.0 / 64.0, 3.0 / 128.0, 1.0 / 64.0, 1.0 / 256.0,
    };

#if 0
    int R = 2;

    for (int y = -R; y <= R; y++) {
        for (int x = -R; x <= R; x++) {
            int xx = clamp(ix + x * stepScale, 0, width - 1);
            int yy = clamp(iy + y * stepScale, 0, height - 1);
#else
    static const int offsetx[] = {
        -2, -1, 0, 1, 2,
        -2, -1, 0, 1, 2,
        -2, -1, 0, 1, 2,
        -2, -1, 0, 1, 2,
        -2, -1, 0, 1, 2,
    };
    static const int offsety[] = {
        -2, -2, -2, -2, -2,
        -1, -1, -1, -1, -1,
         0,  0,  0,  0,  0,
         1,  1,  1,  1,  1,
         2,  2,  2,  2,  2,
    };

#pragma unroll
    for (int i = 0; i < 25; i++) {
    {
            int xx = clamp(ix + offsetx[i] * stepScale, 0, width - 1);
            int yy = clamp(iy + offsety[i] * stepScale, 0, height - 1);
#endif

            float2 q = make_float2(xx, yy);

            const int qidx = getIdx(xx, yy, width);

            normalDepth = aovNormalDepth[qidx];
            texclrMeshid = aovTexclrMeshid[qidx];

            float3 normal = make_float3(normalDepth.x, normalDepth.y, normalDepth.z);

            float depth = normalDepth.w;
            int meshid = (int)texclrMeshid.w;

            float4 color;
            float variance;

            if (isFirstIter) {
                color = aovColorVariance[qidx];
                variance = color.w;

                color = map(color);
                color = RGB2YCoCg(color);
            }
            else {
                color = clrVarBuffer[qidx];
                variance = color.w;
            }

            int _idx = getIdx(xx, iy, width);
            float depthX = aovNormalDepth[_idx].w;

            _idx = getIdx(ix, yy, width);
            float depthY = aovNormalDepth[_idx].w;

            float2 ddZ = make_float2(depthX - centerDepth, depthY - centerDepth);

            float lum = AT_NAME::color::luminance(color.x, color.y, color.z);

            float Wz = min(expf(-fabs(centerDepth - depth) / (sigmaZ * fabs(dot(ddZ, p - q)) + 0.000001f)), 1.0f);

            float Wn = powf(max(0.0f, dot(centerNormal, normal)), sigmaN);

            float Wl = min(expf(-fabs(centerLum - lum) / (sigmaL * sqrGaussedVarLum + 0.000001f)), 1.0f);

            float Wm = meshid == centerMeshId ? 1.0f : 0.0f;

            float W = Wz * Wn * Wl * Wm;
            
            sumC += h[pos] * W * color;
            weightC += h[pos] * W;

#if 1
            Wz = _C(depth, centerDepth, 0.1f);
            Wn = _C(normal, centerNormal, 0.1f);
            Wl = _C(lum, centerLum, 0.1f);
            W = Wz * Wn * Wl * Wm;
#endif

            sumV += (h[pos] * h[pos]) * (W * W) * variance;
            weightV += h[pos] * W;

            pos++;
        }
    }

    if (weightC > 0.0) {
        sumC /= weightC;
    }
    if (weightV > 0.0) {
        sumV /= (weightV * weightV);
    }

    nextClrVarBuffer[idx] = make_float4(sumC.x, sumC.y, sumC.z, sumV);

    if (isFirstIter) {
        // Store color temporary.
        sumC = YCoCg2RGB(sumC);
        tmpBuffer[idx] = unmap(sumC);
    }
    
    if (isFinalIter) {
        sumC = isFirstIter ? sumC : YCoCg2RGB(sumC);
        sumC = unmap(sumC);

        texclrMeshid = aovTexclrMeshid[idx];
        sumC *= texclrMeshid;

        surf2Dwrite(
            sumC,
            dst,
            ix * sizeof(float4), iy,
            cudaBoundaryModeTrap);
    }
}

__global__ void copyFromBufferToAov(
    const float4* __restrict__ src,
    float4* aovColorVariance,
    int width, int height)
{
    int ix = blockIdx.x * blockDim.x + threadIdx.x;
    int iy = blockIdx.y * blockDim.y + threadIdx.y;

    if (ix >= width || iy >= height) {
        return;
    }

    const int idx = getIdx(ix, iy, width);

    float4 s = src[idx];

    aovColorVariance[idx].x = s.x;
    aovColorVariance[idx].y = s.y;
    aovColorVariance[idx].z = s.z;
}

namespace idaten
{
    void SVGFPathTracing::onAtrousFilter(
        cudaSurfaceObject_t outputSurf,
        int width, int height)
    {
        m_atrousMaxIterCnt = aten::clamp(m_atrousMaxIterCnt, 0U, 5U);

        for (int i = 0; i < m_atrousMaxIterCnt; i++) {
            onAtrousFilterIter(
                i, m_atrousMaxIterCnt,
                outputSurf,
                width, height);
        }
    }

    void SVGFPathTracing::onAtrousFilterIter(
        uint32_t iterCnt,
        uint32_t maxIterCnt,
        cudaSurfaceObject_t outputSurf,
        int width, int height)
    {
        dim3 block(BLOCK_SIZE, BLOCK_SIZE);
        dim3 grid(
            (m_tileDomain.w + block.x - 1) / block.x,
            (m_tileDomain.h + block.y - 1) / block.y);

        int curaov = getCurAovs();

        int cur = iterCnt & 0x01;
        int next = 1 - cur;

        bool isFirstIter = iterCnt == 0 ? true : false;
        bool isFinalIter = iterCnt == maxIterCnt - 1 ? true : false;

        int stepScale = 1 << iterCnt;

        atrousFilter << <grid, block, 0, m_stream >> > (
            m_tileDomain,
            isFirstIter, isFinalIter,
            outputSurf,
            m_tmpBuf.ptr(),
            m_aovNormalDepth[curaov].ptr(),
            m_aovTexclrMeshid[curaov].ptr(),
            m_aovColorVariance[curaov].ptr(),
            m_aovMomentTemporalWeight[curaov].ptr(),
            m_atrousClrVar[cur].ptr(), m_atrousClrVar[next].ptr(),
            stepScale,
            width, height);
        checkCudaKernel(atrousFilter);
    }

    void SVGFPathTracing::onCopyFromTmpBufferToAov(int width, int height)
    {
        dim3 block(BLOCK_SIZE, BLOCK_SIZE);
        dim3 grid(
            (width + block.x - 1) / block.x,
            (height + block.y - 1) / block.y);

        int curaov = getCurAovs();

        // Copy color from temporary buffer to AOV buffer for next temporal reprojection.
        copyFromBufferToAov << <grid, block, 0, m_stream >> > (
            m_tmpBuf.ptr(),
            m_aovColorVariance[curaov].ptr(),
            width, height);
        checkCudaKernel(copyFromBufferToAov);
    }
}
