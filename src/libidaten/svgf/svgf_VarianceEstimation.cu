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

inline __device__ float3 computeViewSpace(
    int ix, int iy,
    float centerDepth,
    int width, int height,
    const aten::mat4* mtxC2V)
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
    uv /= make_float2(width - 1, height - 1);    // [0, 1]
    uv = uv * 2.0f - 1.0f;    // [0, 1] -> [-1, 1]

    aten::vec4 pos(uv.x, uv.y, 0, 0);

    // Screen-space -> Clip-space.
    pos.x *= centerDepth;
    pos.y *= centerDepth;

    // Clip-space -> View-space
    pos = mtxC2V->apply(pos);
    pos.z = -centerDepth;
    pos.w = 1.0;

    return make_float3(pos.x, pos.y, pos.z);
}

inline __device__ float C(float3 x1, float3 x2, float sigma)
{
    float a = length(x1 - x2) / sigma;
    a *= a;
    return expf(-0.5f * a);
}

inline __device__ float C(float x1, float x2, float sigma)
{
    float a = fabs(x1 - x2) / sigma;
    a *= a;
    return expf(-0.5f * a);
}

#define IS_IN_BOUND(x, a, b)    ((a) <= (x) && (x) < (b))

__global__ void varianceEstimation(
    idaten::TileDomain tileDomain,
    cudaSurfaceObject_t dst,
    const float4* __restrict__ aovNormalDepth,
    float4* aovMomentTemporalWeight,
    float4* aovColorVariance,
    float4* aovTexclrMeshid,
    aten::mat4 mtxC2V,
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
    auto momentTemporalWeight = aovMomentTemporalWeight[idx];

    float centerDepth = aovNormalDepth[idx].w;
    int centerMeshId = (int)texclrMeshid.w;

    if (centerMeshId < 0) {
        // 背景なので、分散はゼロ.
        aovMomentTemporalWeight[idx].x = 0;
        aovMomentTemporalWeight[idx].y = 0;
        aovMomentTemporalWeight[idx].z = 1;

        surf2Dwrite(
            make_float4(0),
            dst,
            ix * sizeof(float4), iy,
            cudaBoundaryModeTrap);
    }

    float3 centerViewPos = computeViewSpace(ix, iy, centerDepth, width, height, &mtxC2V);

    float3 centerMoment = make_float3(momentTemporalWeight.x, momentTemporalWeight.y, momentTemporalWeight.z);

    int frame = (int)centerMoment.z;

    centerMoment /= centerMoment.z;

    // 分散を計算.
    float var = centerMoment.x - centerMoment.y * centerMoment.y;

    if (frame < 4) {
        // 積算フレーム数が４未満 or Disoccludedされている.
        // 7x7birateral filterで輝度を計算.

        static const int radius = 3;
        static const float sigmaN = 0.005f;
        static const float sigmaD = 0.005f;
        static const float sigmaS = 0.965f;

        float3 centerNormal = make_float3(normalDepth.x, normalDepth.y, normalDepth.z);

        float3 sum = make_float3(0);
        float weight = 0.0f;

#if 0
        for (int v = -radius; v <= radius; v++)
        {
            for (int u = -radius; u <= radius; u++)
            {
#else
        static const int offsetx[] = {
            -3, -2, -1, 0, 1, 2, 3,
            -3, -2, -1, 0, 1, 2, 3,
            -3, -2, -1, 0, 1, 2, 3,
            -3, -2, -1, 0, 1, 2, 3,
            -3, -2, -1, 0, 1, 2, 3,
            -3, -2, -1, 0, 1, 2, 3,
            -3, -2, -1, 0, 1, 2, 3,
        };

        static const int offsety[] = {
            -3, -3, -3, -3, -3, -3, -3,
            -2,    -2,    -2,    -2,    -2,    -2,    -2,
            -1,    -1,    -1,    -1,    -1,    -1,    -1,
             0,     0,     0,     0,     0,     0,     0,
             1,     1,     1,     1,     1,     1,     1,
             2,     2,     2,     2,     2,     2,     2,
             3,     3,     3,     3,     3,     3,     3,
        };

#pragma unroll
        for (int i = 0; i < 49; i++)
        {
            {
                int u = offsetx[i];
                int v = offsety[i];
#endif
                int xx = clamp(ix + u, 0, width - 1);
                int yy = clamp(iy + v, 0, height - 1);

                int pidx = getIdx(xx, yy, width);
                normalDepth = aovNormalDepth[pidx];
                texclrMeshid = aovTexclrMeshid[pidx];
                momentTemporalWeight = aovMomentTemporalWeight[pidx];

                float3 sampleNml = make_float3(normalDepth.x, normalDepth.y, normalDepth.z);
                float sampleDepth = normalDepth.w;
                int sampleMeshId = (int)texclrMeshid.w;

                float3 moment = make_float3(momentTemporalWeight.x, momentTemporalWeight.y, momentTemporalWeight.z);
                moment /= moment.z;

#if 0
                float n = 1 - dot(sampleNml, centerNormal);
                float Wn = exp(-0.5f * n * n / (sigmaN * sigmaN));

                float d = 1 - min(centerDepth, sampleDepth) / max(centerDepth, sampleDepth);
                float Wd = exp(-0.5f * d * d / (sigmaD * sigmaD));

                float Ws = exp(-0.5f * (u * u + v * v) / (sigmaS * sigmaS));
#elif 0
                float Wn = 1.0f;
                {
                    float normalCloseness = dot(sampleNml, centerNormal);
                    normalCloseness = normalCloseness * normalCloseness;
                    normalCloseness = normalCloseness * normalCloseness;
                    float normalError = (1.0f - normalCloseness);
                    Wn = max((1.0f - normalError), 0.0f);
                }

                float Wd = max(0.0f, 1.0f - fabs(centerDepth - sampleDepth));

                float Ws = 1.0f;
                {
                    auto sampleViewPos = computeViewSpace(ix + u, iy + v, sampleDepth, width, height, &mtxC2V);

                    // Change in position in camera space.
                    auto dq = centerViewPos - sampleViewPos;

                    // How far away is this point from the original sample in camera space? (Max value is unbounded).
                    auto dist2 = dot(dq, dq);

                    // How far off the expected plane (on the perpendicular) is this point?  Max value is unbounded.
                    float err = max(fabs(dot(dq, sampleNml)), abs(dot(dq, centerNormal)));

                    Ws = (dist2 < 0.001f)
                        ? 1.0
                        : pow(max(0.0, 1.0 - 2.0 * err / sqrt(dist2)), 2.0);
                }
#else
                float3 sampleViewPos = computeViewSpace(ix + u, iy + v, sampleDepth, width, height, &mtxC2V);

                float Wn = C(centerNormal, sampleNml, 0.1f);
                float Ws = C(centerViewPos, sampleViewPos, 0.1f);
                float Wd = C(centerDepth, sampleDepth, 0.1f);
#endif

                float Wm = centerMeshId == sampleMeshId ? 1.0f : 0.0f;

                float W = Ws * Wn * Wd * Wm;
                sum += moment * W;
                weight += W;
            }
        }

        if (weight > 0.0f) {
            sum /= weight;
        }

        var = sum.x - sum.y * sum.y;
    }

    // TODO
    // 分散はマイナスにならないが・・・・
    var = fabs(var);

    aovColorVariance[idx].w = var;

    surf2Dwrite(
        make_float4(var, var, var, 1),
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
            (m_tileDomain.w + block.x - 1) / block.x,
            (m_tileDomain.h + block.y - 1) / block.y);

        int curaov = getCurAovs();

        varianceEstimation << <grid, block, 0, m_stream >> > (
        //varianceEstimation << <1, 1 >> > (
            m_tileDomain,
            outputSurf,
            m_aovNormalDepth[curaov].ptr(),
            m_aovMomentTemporalWeight[curaov].ptr(),
            m_aovColorVariance[curaov].ptr(),
            m_aovTexclrMeshid[curaov].ptr(),
            m_mtxC2V,
            width, height);

        checkCudaKernel(varianceEstimation);
    }
}
