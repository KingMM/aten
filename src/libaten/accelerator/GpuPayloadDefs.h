#pragma once

#include "accelerator/threaded_bvh.h"
#include "accelerator/sbvh.h"

//#define GPGPU_TRAVERSE_THREADED_BVH
#define GPGPU_TRAVERSE_SBVH

namespace aten {
#if defined(GPGPU_TRAVERSE_THREADED_BVH)
	using GPUBvhNode = ThreadedBvhNode;
	using GPUBvh = ThreadedBVH;
#elif defined(GPGPU_TRAVERSE_SBVH)
	using GPUBvhNode = ThreadedSbvhNode;
	using GPUBvh = sbvh;
#else
	AT_STATICASSERT(false);
#endif

	AT_STATICASSERT((sizeof(GPUBvhNode) % (sizeof(float) * 4)) == 0);
	static const int GPUBvhNodeSize = sizeof(GPUBvhNode) / (sizeof(float) * 4);
}