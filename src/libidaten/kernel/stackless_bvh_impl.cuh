#include "kernel/idatendefs.cuh"

template <idaten::IntersectType Type>
AT_CUDA_INLINE __device__ bool intersectStacklessBVHTriangles(
    cudaTextureObject_t nodes,
    const Context* ctxt,
    const aten::ray r,
    float t_min, float t_max,
    aten::Intersection* isect)
{
    aten::Intersection isectTmp;

    int nodeid = 0;
    uint32_t bitstack = 0;
    
    float4 node0;    // xyz: boxmin_0, w: parent
    float4 node1;    // xyz: boxmax_0, w: sibling
    float4 node2;    // xyz: boxmax_1, w: child_0
    float4 node3;    // xyz: boxmax_1, w: child_1

    float4 attrib;    // x:shapeid, y:primid, z:exid,    w:meshid

    float4 boxmin_0;
    float4 boxmax_0;
    float4 boxmin_1;
    float4 boxmax_1;

    isect->t = t_max;

    while (nodeid >= 0) {
        node0 = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 0);    // xyz : boxmin_0, w: parent
        node1 = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 1);    // xyz : boxmax_0, w: sibling
        node2 = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 2);    // xyz : boxmin_1, w: child_0
        node3 = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 3);    // xyz : boxmax_1, w: child_1
        attrib = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 4);    // x : shapeid, y : primid, z : exid, w : meshid

        boxmin_0 = make_float4(node0.x, node0.y, node0.z, 1.0f);
        boxmax_0 = make_float4(node1.x, node1.y, node1.z, 1.0f);

        boxmin_1 = make_float4(node2.x, node2.y, node2.z, 1.0f);
        boxmax_1 = make_float4(node3.x, node3.y, node3.z, 1.0f);

        bool isHit = false;

        if (attrib.y >= 0) {
            int primidx = (int)attrib.y;
            aten::PrimitiveParamter prim;
            prim.v0 = ((aten::vec4*)ctxt->prims)[primidx * aten::PrimitiveParamter_float4_size + 0];
            prim.v1 = ((aten::vec4*)ctxt->prims)[primidx * aten::PrimitiveParamter_float4_size + 1];

            isectTmp.t = AT_MATH_INF;
            isHit = hitTriangle(&prim, ctxt, r, &isectTmp);

            bool isIntersect = (Type == idaten::IntersectType::Any
                ? isHit
                : isectTmp.t < isect->t);

            if (isIntersect) {
                *isect = isectTmp;
                isect->objid = (int)attrib.x;
                isect->primid = (int)attrib.y;
                isect->mtrlid = prim.mtrlid;
                
                //isect->meshid = (int)attrib.w;
                isect->meshid = prim.gemoid;

                if (Type == idaten::IntersectType::Closer
                    || Type == idaten::IntersectType::Any)
                {
                    return true;
                }
            }
        }
        else {
            float t[2];
            bool hit[2];

            hit[0] = hitAABB(r.org, r.dir, boxmin_0, boxmax_0, t_min, t_max, &t[0]);
            hit[1] = hitAABB(r.org, r.dir, boxmin_1, boxmax_1, t_min, t_max, &t[1]);

            if (hit[0] || hit[1]) {
                bitstack = bitstack << 1;

                if (hit[0] && hit[1]) {
                    nodeid = (int)(t[0] < t[1] ? node2.w : node3.w);
                    bitstack = bitstack | 1;
                }
                else if (hit[0]) {
                    nodeid = (int)node2.w;
                }
                else if (hit[1]) {
                    nodeid = (int)node3.w;
                }

                continue;
            }
        }

        while ((bitstack & 1) == 0) {
            if (bitstack == 0) {
                return (isect->objid >= 0);
            }

            nodeid = (int)node0.w;    // parent
            bitstack = bitstack >> 1;

            node0 = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 0);    // xyz : boxmin_0, w: parent
        }

        node1 = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 1);    // xyz : boxmax_0, w: sibling

        nodeid = (int)node1.w;    // sibling
        bitstack = bitstack ^ 1;
    }

    return (isect->objid >= 0);
}

template <idaten::IntersectType Type>
AT_CUDA_INLINE __device__ bool intersectStacklessBVH(
    cudaTextureObject_t nodes,
    const Context* ctxt,
    const aten::ray r,
    float t_min, float t_max,
    aten::Intersection* isect)
{
    aten::Intersection isectTmp;

    isect->t = t_max;

    int nodeid = 0;
    uint32_t bitstack = 0;

    float4 node0;    // xyz: boxmin_0, w: parent
    float4 node1;    // xyz: boxmax_0, w: sibling
    float4 node2;    // xyz: boxmax_1, w: child_0
    float4 node3;    // xyz: boxmax_1, w: child_1

    float4 attrib;    // x:shapeid, y:primid, z:exid,    w:meshid

    float4 boxmin_0;
    float4 boxmax_0;
    float4 boxmin_1;
    float4 boxmax_1;

    while (nodeid >= 0) {
        node0 = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 0);    // xyz : boxmin_0, w: parent
        node1 = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 1);    // xyz : boxmax_0, w: sibling
        node2 = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 2);    // xyz : boxmin_1, w: child_0
        node3 = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 3);    // xyz : boxmax_1, w: child_1
        attrib = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 4);    // x : shapeid, y : primid, z : exid, w : meshid

        boxmin_0 = make_float4(node0.x, node0.y, node0.z, 1.0f);
        boxmax_0 = make_float4(node1.x, node1.y, node1.z, 1.0f);

        boxmin_1 = make_float4(node2.x, node2.y, node2.z, 1.0f);
        boxmax_1 = make_float4(node3.x, node3.y, node3.z, 1.0f);

        bool isHit = false;

        if (attrib.x >= 0) {
            // Leaf.
            const auto* s = &ctxt->shapes[(int)attrib.x];

            if (attrib.z >= 0) {    // exid
                aten::ray transformedRay;

                if (s->mtxid >= 0) {
                    auto mtxW2L = ctxt->matrices[s->mtxid * 2 + 1];
                    transformedRay.dir = mtxW2L.applyXYZ(r.dir);
                    transformedRay.dir = normalize(transformedRay.dir);
                    transformedRay.org = mtxW2L.apply(r.org) + AT_MATH_EPSILON * transformedRay.dir;
                }
                else {
                    transformedRay = r;
                }

                isHit = intersectStacklessBVHTriangles<Type>(ctxt->nodes[(int)attrib.z], ctxt, transformedRay, t_min, t_max, &isectTmp);
            }
            else {
                // TODO
                // Only sphere...
                //isHit = intersectShape(s, nullptr, ctxt, r, t_min, t_max, &recTmp, &recOptTmp);
                isectTmp.t = AT_MATH_INF;
                isHit = hitSphere(s, r, t_min, t_max, &isectTmp);
                isectTmp.mtrlid = s->mtrl.idx;
            }

            bool isIntersect = (Type == idaten::IntersectType::Any
                ? isHit
                : isectTmp.t < isect->t);

            if (isIntersect) {
                *isect = isectTmp;
                isect->objid = (int)attrib.x;
                isect->meshid = (isect->meshid < 0 ? (int)attrib.w : isect->meshid);

                if (Type == idaten::IntersectType::Closer
                    || Type == idaten::IntersectType::Any)
                {
                    return true;
                }
            }
        }
        else {
            float t[2];
            bool hit[2];

            hit[0] = hitAABB(r.org, r.dir, boxmin_0, boxmax_0, t_min, t_max, &t[0]);
            hit[1] = hitAABB(r.org, r.dir, boxmin_1, boxmax_1, t_min, t_max, &t[1]);

            if (hit[0] || hit[1]) {
                bitstack = bitstack << 1;

                if (hit[0] && hit[1]) {
                    nodeid = (int)(t[0] < t[1] ? node2.w : node3.w);
                    bitstack = bitstack | 1;
                }
                else if (hit[0]) {
                    nodeid = (int)node2.w;
                }
                else if (hit[1]) {
                    nodeid = (int)node3.w;
                }

                continue;
            }
        }

        while ((bitstack & 1) == 0) {
            if (bitstack == 0) {
                return (isect->objid >= 0);
            }

            nodeid = (int)node0.w;    // parent
            bitstack = bitstack >> 1;

            node0 = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 0);    // xyz : boxmin_0, w: parent
        }

        node1 = tex1Dfetch<float4>(nodes, aten::GPUBvhNodeSize * nodeid + 1);    // xyz : boxmax_0, w: sibling

        nodeid = (int)node1.w;    // sibling
        bitstack = bitstack ^ 1;
    }

    return (isect->objid >= 0);
}

AT_CUDA_INLINE __device__ bool intersectClosestStacklessBVH(
    const Context* ctxt,
    const aten::ray& r,
    aten::Intersection* isect,
    const float t_max/*= AT_MATH_INF*/)
{
    float t_min = AT_MATH_EPSILON;

    bool isHit = intersectStacklessBVH<idaten::IntersectType::Closest>(
        ctxt->nodes[0],
        ctxt,
        r,
        t_min, t_max,
        isect);

    return isHit;
}

AT_CUDA_INLINE __device__ bool intersectCloserStacklessBVH(
    const Context* ctxt,
    const aten::ray& r,
    aten::Intersection* isect,
    const float t_max)
{
    float t_min = AT_MATH_EPSILON;

    bool isHit = intersectStacklessBVH<idaten::IntersectType::Closer>(
        ctxt->nodes[0],
        ctxt,
        r,
        t_min, t_max,
        isect);

    return isHit;
}

AT_CUDA_INLINE __device__ bool intersectAnyStacklessBVH(
    const Context* ctxt,
    const aten::ray& r,
    aten::Intersection* isect)
{
    float t_min = AT_MATH_EPSILON;
    float t_max = AT_MATH_INF;

    bool isHit = intersectStacklessBVH<idaten::IntersectType::Any>(
        ctxt->nodes[0],
        ctxt,
        r,
        t_min, t_max,
        isect);

    return isHit;
}
