#include "kernel/bvh.cuh"
#include "kernel/intersect.cuh"

#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include "cuda/helper_math.h"

#define STACK_SIZE	(64)

__device__ bool intersectBVH(
	cudaTextureObject_t nodes,
	int* exid,
	aten::ray* keepTransformedRay,
	const Context* ctxt,
	const aten::ray r,
	float t_min, float t_max,
	aten::hitrecord* rec)
{
	int stackbuf[STACK_SIZE];

	stackbuf[0] = 0;

	int stackpos = 1;
	int nestedStackPos = -1;

	aten::ray transformedRay = r;
	*keepTransformedRay = transformedRay;

	bool isNested = false;
	aten::hitrecord recTmp;
	bool isHit = false;

	real hitt = AT_MATH_INF;
	*exid = -1;
	int tmpexid = -1;

	int nodeid = -1;
	float4 node;	// x:left, y:right
	float4 attrib;	// x:shapeid, y:primid, z:nestid

	float4 _boxmin;
	float4 _boxmax;

	aten::vec3 boxmin;
	aten::vec3 boxmax;

	while (stackpos > 0) {
		if (stackpos == nestedStackPos) {
			nestedStackPos = -1;
			isNested = false;
			transformedRay = r;
		}

		nodeid = stackbuf[stackpos - 1];
		stackpos--;

		if (nodeid >= 0) {
			node = tex1Dfetch<float4>(nodes, 4 * nodeid + 0);	// x : left, y: right
			attrib = tex1Dfetch<float4>(nodes, 4 * nodeid + 1);	// x : shapeid, y : primgid, z : nestid, w : exid
			_boxmin = tex1Dfetch<float4>(nodes, 4 * nodeid + 2);
			_boxmax = tex1Dfetch<float4>(nodes, 4 * nodeid + 3);

			boxmin = aten::make_float3(_boxmin.x, _boxmin.y, _boxmin.z);
			boxmax = aten::make_float3(_boxmax.x, _boxmax.y, _boxmax.z);

			if (node.x < 0 && node.y < 0) {
				if (attrib.z >= 0) {
					if (aten::aabb::hit(transformedRay, boxmin, boxmax, t_min, t_max)) {
						nestedStackPos = isNested ? nestedStackPos : stackpos;
						stackbuf[stackpos++] = (int)attrib.z;

						if (!isNested) {
							const auto& param = ctxt->shapes[(int)attrib.x];
							transformedRay.org = param.mtxW2L.apply(r.org);
							transformedRay.dir = param.mtxW2L.applyXYZ(r.dir);
							transformedRay.dir = normalize(transformedRay.dir);
							isNested = true;
						}
					}
				}
				else {
					isHit = false;
					tmpexid = -1;

					const auto* s = &ctxt->shapes[(int)attrib.x];

					if (attrib.w >= 0) {	// exid
						real t = AT_MATH_INF;
						isHit = aten::aabb::hit(transformedRay, boxmin, boxmax, t_min, t_max, &t);
						isHit = (t < t_min || t > t_max ? false : isHit);
						recTmp.t = t;
						tmpexid = attrib.w;
					}
					else if (attrib.y >= 0) {	// primid
						const auto& prim = ctxt->prims[(int)attrib.y];
						isHit = intersectShape(s, &prim, ctxt, transformedRay, t_min, t_max, &recTmp);
						recTmp.mtrlid = prim.mtrlid;
					}
					else {
						isHit = intersectShape(s, nullptr, ctxt, transformedRay, t_min, t_max, &recTmp);
						recTmp.mtrlid = s->mtrl.idx;
						tmpexid = -1;
					}

					if (isHit) {
						if (recTmp.t < hitt) {
							hitt = recTmp.t;
							*exid = tmpexid;
							*keepTransformedRay = transformedRay;
						}
						if (tmpexid < 0) {
							if (recTmp.t < rec->t) {
								*rec = recTmp;
								rec->obj = (void*)s;
							}
						}
					}
				}
			}
			else {
				if (aten::aabb::hit(transformedRay, boxmin, boxmax, t_min, t_max)) {
					stackbuf[stackpos++] = (int)node.x;
					stackbuf[stackpos++] = (int)node.y;

					if (stackpos > STACK_SIZE) {
						//AT_ASSERT(false);
						return false;
					}
				}
			}
		}
	}

	return (rec->obj != nullptr);
}

__device__ bool intersectBVH(
	const Context* ctxt,
	const aten::ray& r,
	float t_min, float t_max,
	aten::hitrecord* rec)
{
	int exid = -1;
	aten::ray keepTransformedRay = r;

	bool isHit = intersectBVH(
		ctxt->nodes[0],
		&exid,
		&keepTransformedRay,
		ctxt,
		r,
		t_min, t_max,
		rec);

	if (exid >= 0) {
		aten::hitrecord recTmp;

		if (intersectBVH(ctxt->nodes[exid], &exid, &keepTransformedRay, ctxt, keepTransformedRay, t_min, t_max, &recTmp)) {
			if (recTmp.t < rec->t) {
				*rec = recTmp;
				isHit = true;
			}
		}
	}

	if (isHit) {
		evalHitResult(ctxt, (aten::ShapeParameter*)rec->obj, r, rec);
	}

	return isHit;
}