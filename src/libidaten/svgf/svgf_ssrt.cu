#include "svgf/svgf_pt.h"

#include "kernel/context.cuh"
#include "kernel/light.cuh"
#include "kernel/material.cuh"
#include "kernel/intersect.cuh"
#include "kernel/accelerator.cuh"
#include "kernel/compaction.h"
#include "kernel/pt_common.h"

#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include "cuda/helper_math.h"
#include "cuda/cudautil.h"
#include "cuda/cudamemory.h"

#include "aten4idaten.h"

__global__ void hitTestPrimaryRayInScreenSpace(
	cudaSurfaceObject_t gbuffer,
	idaten::SVGFPathTracing::Path* paths,
	aten::Intersection* isects,
	int* hitbools,
	int width, int height,
	const aten::vec4 camPos,
	const aten::GeomParameter* __restrict__ geoms,
	const aten::PrimitiveParamter* __restrict__ prims,
	const aten::mat4* __restrict__ matrices,
	cudaTextureObject_t vtxPos)
{
	const auto ix = blockIdx.x * blockDim.x + threadIdx.x;
	const auto iy = blockIdx.y * blockDim.y + threadIdx.y;

	if (ix >= width || iy >= height) {
		return;
	}

	const auto idx = getIdx(ix, iy, width);

	auto& path = paths[idx];
	path.isHit = false;

	hitbools[idx] = 0;

	if (path.isTerminate) {
		return;
	}

	// Sample data from texture.
	float4 data;
	surf2Dread(&data, gbuffer, ix * sizeof(float4), iy);

	// NOTE
	// x : objid
	// y : primid
	// zw : bary centroid

	int objid = __float_as_int(data.x);
	int primid = __float_as_int(data.y);

	isects[idx].objid = objid;
	isects[idx].primid = primid;

	// bary centroid.
	isects[idx].a = data.z;
	isects[idx].b = data.w;

	if (objid >= 0) {
		aten::PrimitiveParamter prim;
		prim.v0 = ((aten::vec4*)prims)[primid * aten::PrimitiveParamter_float4_size + 0];
		prim.v1 = ((aten::vec4*)prims)[primid * aten::PrimitiveParamter_float4_size + 1];

		isects[idx].mtrlid = prim.mtrlid;
		isects[idx].meshid = prim.gemoid;

		const auto* obj = &geoms[objid];

		float4 p0 = tex1Dfetch<float4>(vtxPos, prim.idx[0]);
		float4 p1 = tex1Dfetch<float4>(vtxPos, prim.idx[1]);
		float4 p2 = tex1Dfetch<float4>(vtxPos, prim.idx[2]);

		real a = data.z;
		real b = data.w;
		real c = 1 - a - b;

		// �d�S���W�n(barycentric coordinates).
		// v0�.
		// p = (1 - a - b)*v0 + a*v1 + b*v2
		auto p = c * p0 + a * p1 + b * p2;
		aten::vec4 vp(p.x, p.y, p.z, 1.0f);

		if (obj->mtxid >= 0) {
			auto mtxL2W = matrices[obj->mtxid * 2 + 0];
			vp = mtxL2W.apply(vp);
		}

		isects[idx].t = (camPos - vp).length();

		path.isHit = true;
		hitbools[idx] = 1;
	}
	else {
		path.isHit = false;
		hitbools[idx] = 0;
	}
}

namespace idaten
{
	void SVGFPathTracing::onScreenSpaceHitTest(
		int width, int height,
		int bounce,
		cudaTextureObject_t texVtxPos)
	{
		dim3 block(BLOCK_SIZE, BLOCK_SIZE);
		dim3 grid(
			(width + block.x - 1) / block.x,
			(height + block.y - 1) / block.y);

		aten::vec4 campos = aten::vec4(m_camParam.origin, 1.0f);

		CudaGLResourceMap rscmap(&m_gbuffer);
		auto gbuffer = m_gbuffer.bind();

		hitTestPrimaryRayInScreenSpace << <grid, block >> > (
			gbuffer,
			m_paths[Resolution::Hi].ptr(),
			m_isects.ptr(),
			m_hitbools.ptr(),
			width, height,
			campos,
			m_shapeparam.ptr(),
			m_primparams.ptr(),
			m_mtxparams.ptr(),
			texVtxPos);

		checkCudaKernel(hitTestPrimaryRayInScreenSpace);
	}
}
