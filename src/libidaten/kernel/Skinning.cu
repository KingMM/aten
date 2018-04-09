#include "kernel/Skinning.h"

#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include "cuda/helper_math.h"
#include "cuda/cudautil.h"

//#pragma optimize( "", off)

__global__ void computeSkinning(
	uint32_t indexNum,
	const idaten::Skinning::Vertex* __restrict__ vertices,
	const uint32_t* __restrict__ indices,
	const aten::mat4* __restrict__ matrices,
	aten::vertex* dst)
{
	const auto idx = blockIdx.x * blockDim.x + threadIdx.x;

	if (idx >= indexNum) {
		return;
	}

	const auto vtxIdx = indices[idx];
	const auto* vtx = &vertices[vtxIdx];

	aten::vec4 srcPos = vtx->position;
	aten::vec4 srcNml = aten::vec4(vtx->normal, 0);

	aten::vec4 dstPos(0);
	aten::vec4 dstNml(0);

	for (int i = 0; i < 4; i++) {
		int idx = int(vtx->blendIndex[i]);
		float weight = vtx->blendWeight[i];

		aten::mat4 mtx = matrices[idx];

		dstPos += weight * mtx * vtx->position;
		dstNml += weight * mtx * srcNml;
	}

	dstNml = normalize(dstNml);

	dst[idx].pos = aten::vec4(dstPos.x, dstPos.y, dstPos.z, 1);
	dst[idx].nml = dstNml;
	dst[idx].uv = aten::vec3(vtx->uv[0], vtx->uv[1], 0);
}

// NOTE
// http://www.cuvilib.com/Reduction.pdf
// https://github.com/AJcodes/cuda_minmax/blob/master/cuda_minmax/kernel.cu

//#define MINMAX_TEST

__global__ void getMinMax(
	bool isFinalIter,
	uint32_t num,
#ifdef MINMAX_TEST
	const uint32_t* __restrict__ src,
	uint32_t* dstMin,
	uint32_t* dstMax)
#else
	const aten::vertex* __restrict__ src,
	aten::vec3* dstMin,
	aten::vec3* dstMax)
#endif
{
	const auto idx = blockIdx.x * blockDim.x + threadIdx.x;

	if (idx >= num) {
		return;
	}

	const auto tid = threadIdx.x;

	// NOTE
	// http://yusuke-ujitoko.hatenablog.com/entry/2016/02/05/012618
	// �J�[�l���Ăяo���̂Ƃ��Ɏw��ł���̂�1�̐�����.
	// ������shared memory���g�������Ƃ��́Ashared memory�̃T�C�Y�̍��v���w�肵�āA�J�[�l�������Ő؂蕪����K�v������.

#ifdef MINMAX_TEST
	extern __shared__ uint32_t minPos[];
	__shared__ uint32_t* maxPos;

	if (tid == 0) {
		maxPos = minPos + blockDim.x;
	}

	if (isFinalIter) {
		minPos[tid] = dstMin[idx];
		maxPos[tid] = dstMax[idx];
	}
	else {
		minPos[tid] = src[idx];
		maxPos[tid] = src[idx];
	}
#else
	auto pos = src[idx].pos;

	extern __shared__ aten::vec3 minPos[];
	__shared__ aten::vec3* maxPos;

	if (tid == 0) {
		maxPos = minPos + blockDim.x;
	}

	if (isFinalIter) {
		minPos[tid] = dstMin[idx];
		maxPos[tid] = dstMax[idx];
	}
	else {
		minPos[tid] = pos;
		maxPos[tid] = pos;
	}
#endif
	__syncthreads();

	for (unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
		if (tid < s && tid + s < num) {
#ifdef MINMAX_TEST
			auto _min = min(minPos[tid], minPos[tid + s]);
			auto _max = max(maxPos[tid], maxPos[tid + s]);

			printf("[tid]%d [s]%d [tid + s]%d min(%d, %d)>%d, max(%d, %d)>%d\n",
				tid, s, tid + s,
				minPos[tid], minPos[tid + s], _min,
				maxPos[tid], maxPos[tid + s], _max);

			minPos[tid] = _min;
			maxPos[tid] = _max;

			printf("   [%d] (%d(%d), %d(%d))\n", tid, minPos[tid], _min, maxPos[tid], _max);
#else
			minPos[tid] = aten::min(minPos[tid], minPos[tid + s]);
			maxPos[tid] = aten::max(maxPos[tid], maxPos[tid + s]);
#endif
		}
		__syncthreads();
	}

	if (tid == 0) {
#ifdef MINMAX_TEST
		printf("[tid]%d [min]%d max[%d]\n", tid, minPos[0], maxPos[0]);
#endif
		dstMin[blockIdx.x] = minPos[0];
		dstMax[blockIdx.x] = maxPos[0];
	}
}

namespace idaten
{
	void Skinning::init(
		Skinning::Vertex* vertices,
		uint32_t vtxNum,
		uint32_t* indices,
		uint32_t idxNum)
	{
		m_vertices.init(vtxNum);
		m_vertices.writeByNum(vertices, vtxNum);

		m_indices.init(idxNum);
		m_indices.writeByNum(indices, idxNum);
	}

	void Skinning::update(
		aten::mat4* matrices,
		uint32_t mtxNum)
	{
		if (m_matrices.bytes() == 0) {
			m_matrices.init(mtxNum);
		}

		AT_ASSERT(m_matrices.maxNum() >= mtxNum);

		m_matrices.writeByNum(matrices, mtxNum);
	}

	void Skinning::compute()
	{
		const auto idxNum = m_indices.maxNum();
	}

	void Skinning::runMinMaxTest()
	{
#ifdef MINMAX_TEST
		uint32_t data[] = {
			2, 4, 7, 9, 10, 3, 4, 1,
		};

		auto num = AT_COUNTOF(data);

		TypedCudaMemory<uint32_t> buf;
		buf.init(AT_COUNTOF(data));
		buf.writeByNum(data, AT_COUNTOF(data));

		dim3 block(256, 1, 1);
		dim3 grid((num + block.x - 1) / block.x, 1, 1);

		TypedCudaMemory<uint32_t> _min;
		TypedCudaMemory<uint32_t> _max;
		_min.init(grid.x);
		_max.init(grid.x);

		auto sharedMemSize = block.x * sizeof(uint32_t) * 2;

		getMinMax << <grid, block, sharedMemSize >> > (
			false,
			num,
			buf.ptr(),
			_min.ptr(),
			_max.ptr());

		checkCudaKernel(getMinMax);

		num = grid.x;

		getMinMax << <1, block, sharedMemSize >> > (
			true,
			num,
			buf.ptr(),
			_min.ptr(),
			_max.ptr());

		checkCudaKernel(getMinMaxFinal);

		std::vector<uint32_t> tmpMin(_min.maxNum());
		std::vector<uint32_t> tmpMax(_max.maxNum());

		_min.read(&tmpMin[0], sizeof(uint32_t) * tmpMin.size());
		_max.read(&tmpMax[0], sizeof(uint32_t) * tmpMax.size());
#endif
	}
}