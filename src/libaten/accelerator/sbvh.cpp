#include "accelerator/sbvh.h"

#include <omp.h>
#include <numeric>

//#pragma optimize( "", off)

namespace aten
{
	inline bool checkAABBOverlap(
		const aabb& box0,
		const aabb& box1,
		real& overlap)
	{
		aabb delta = aabb(
			aten::max(box0.minPos(), box1.minPos()),
			aten::min(box0.maxPos(), box1.maxPos()));

		if (delta.isValid()) {
			overlap = delta.computeSurfaceArea();
			return true;
		}

		return false;
	}

	template <class Iter, class Cmp>
	void mergeSort(Iter first, Iter last, Cmp cmp)
	{
		const auto numThreads = ::omp_get_max_threads();

		const auto numItems = last - first;
		const auto numItemPerThread = numItems / numThreads;

		// Sort each blocks.
#pragma omp parallel num_threads(numThreads)
		{
			const auto idx = ::omp_get_thread_num();
		
			const auto startPos = idx * numItemPerThread;
			const auto endPos = (idx + 1 == numThreads ? numItems : startPos + numItemPerThread);

			std::sort(first + startPos, first + endPos, cmp);
		}

		int blockNum = numThreads;

		// Merge blocks.
		while (blockNum >= 2)
		{
			const auto numItemsInBlocks = numItems / blockNum;

			// �Q�̃u���b�N���P�Ƀ}�[�W����.
#pragma omp parallel num_threads(blockNum / 2)
			{
				const auto idx = ::omp_get_thread_num();

				// �Q�̃u���b�N���P�Ƀ}�[�W����̂ŁAnumItemsInBlocks * 2 �ƂȂ�.
				auto startPos = idx * numItemsInBlocks * 2;

				// ���Ԓn�_�Ȃ̂ŁAnumItemsInBlocks * 2 �̔����� numItemsInBlocks �ƂȂ�.
				auto pivot = startPos + numItemsInBlocks;

				// �I�[.
				auto endPos = (idx + 1 == blockNum / 2 ? numItems : pivot + numItemsInBlocks);

				std::inplace_merge(first + startPos, first + pivot, first + endPos);
			}

			// �Q���P�Ƀ}�[�W����̂ŁA�����ɂȂ�.
			blockNum = (blockNum + 1) / 2;
		}
	}

	void sbvh::build(
		hitable** list,
		uint32_t num,
		aabb* bbox/*= nullptr*/)
	{
		if (m_isNested) {
			buildInternal(list, num);

			if (isExporting()) {
				m_threadedNodes.resize(1);

				std::vector<int> indices;
				convert(
					m_threadedNodes[0],
					0,
					indices);
			}
		}
		else {
			// Build top layer bvh.

			m_bvh.disableLayer();
			m_bvh.build(list, num, bbox);

			auto bbox = m_bvh.getBoundingbox();

			const auto& nestedBvh = m_bvh.getNestedAccel();

			// NOTE
			// GPGPU�����p�� threaded bvh(top layer) �� sbvh �𓯂���������ԏ�Ɋi�[���邽�߁A�P�̃��X�g�ŊǗ�����.
			// ���̂��߁A+1����.
			m_threadedNodes.resize(nestedBvh.size() + 1);

			const auto& toplayer = m_bvh.getNodes()[0];
			m_threadedNodes[0].resize(toplayer.size());
			memcpy(&m_threadedNodes[0][0], &toplayer[0], toplayer.size() * sizeof(ThreadedSbvhNode));

			// Convert to threaded.
			for (int i = 0; i < nestedBvh.size(); i++) {
				// TODO
				const auto bvh = (const sbvh*)nestedBvh[i];

				auto box = bvh->getBoundingbox();
				bbox.expand(box);

				std::vector<int> indices;
				bvh->convert(
					m_threadedNodes[i + 1], 
					(int)m_refIndices.size(),
					indices);

				m_refIndices.insert(m_refIndices.end(), indices.begin(), indices.end());
			}

			setBoundingBox(bbox);
		}
	}

	void sbvh::buildInternal(
		hitable** list,
		uint32_t num)
	{
		std::vector<face*> tris;
		tris.reserve(num);

		for (uint32_t i = 0; i < num; i++) {
			tris.push_back((face*)list[i]);
		}

		aabb rootBox;

		m_refs.clear();
		m_refs.reserve(2 * tris.size());
		m_refs.resize(tris.size());

		m_offsetTriIdx = UINT32_MAX;

		for (uint32_t i = 0; i < tris.size(); i++) {
			m_refs[i].triid = i;
			m_refs[i].bbox = tris[i]->computeAABB();

			m_offsetTriIdx = std::min<uint32_t>(m_offsetTriIdx, tris[i]->id);

			rootBox.expand(m_refs[i].bbox);
		}

		// Set as bounding box.
		setBoundingBox(rootBox);

		// Reference�̃C���f�b�N�X���X�g���쐬.
		std::vector<uint32_t> refIndices(m_refs.size());
		std::iota(refIndices.begin(), refIndices.end(), 0);

		auto rootSurfaceArea = rootBox.computeSurfaceArea();

		// TODO
		const real areaAlpha = real(1e-5);

		struct SBVHEntry {
			SBVHEntry() {}
			SBVHEntry(uint32_t id) : nodeIdx(id) {}

			uint32_t nodeIdx;
		} stack[128];

		int stackpos = 1;
		stack[0] = SBVHEntry(0);

		m_nodes.reserve(m_refs.size() * 3);
		m_nodes.push_back(SBVHNode());
		m_nodes[0] = SBVHNode(std::move(refIndices), rootBox);

		uint32_t numNodes = 1;

		m_refIndexNum = 0;

		while (stackpos > 0)
		{
			auto top = stack[--stackpos];
			auto& node = m_nodes[top.nodeIdx];

			// enough triangles so far.
			if (node.refIds.size() <= m_maxTriangles) {
				m_refIndexNum += (uint32_t)node.refIds.size();
				continue;
			}

			real objCost = 0.0f;
			aabb objLeftBB;
			aabb objRightBB;
			int sahBin = -1;
			int sahComponent = -1;
			findObjectSplit(node, objCost, objLeftBB, objRightBB, sahBin, sahComponent);

			// check whether the object split produces overlapping nodes
			// if so, check whether we are close enough to the root so that the spatial split makes sense.
			real overlapCost = real(0);
			bool needComputeSpatial = false;

			bool isOverrlap = checkAABBOverlap(objLeftBB, objRightBB, overlapCost);

			if (isOverrlap) {
				needComputeSpatial = (overlapCost / rootSurfaceArea) >= areaAlpha;
			}

			float spatialCost = AT_MATH_INF;
			aabb spatialLeftBB;
			aabb spatialRightBB;
			real spatialSplitPlane = real(0);
			int spatialDimension = -1;
			int leftCnt = 0;
			int rightCnt = 0;

			if (needComputeSpatial) {
				findSpatialSplit(
					node,
					spatialCost,
					leftCnt, rightCnt,
					spatialLeftBB, spatialRightBB,
					spatialDimension,
					spatialSplitPlane);
			}

			std::vector<uint32_t> leftList;
			std::vector<uint32_t> rightList;

			// if we have compute the spatial cost and it is better than the binned sah cost,
			// then do the split.

			int usedAxis = 0;

			if (needComputeSpatial && spatialCost <= objCost) {
				// use spatial split.
				spatialSort(
					node,
					spatialSplitPlane,
					spatialDimension,
					spatialCost,
					leftCnt, rightCnt,
					spatialLeftBB, spatialRightBB,
					leftList, rightList);

				objLeftBB = spatialLeftBB;
				objRightBB = spatialRightBB;

				usedAxis = spatialDimension;
			}
			else {
				// use object split.

				// check whether the binned sah has failed.
				// if so, we have to do the object median split.
				// it happens for some scenes, but it happens very close to the leaves.
				// i.e. when we are left with 8-16 tightly packed references which end up in the same bin.
				if (sahBin == -1) {
					auto axisDelta = node.bbox.size();

					auto maxVal = std::max(std::max(axisDelta.x, axisDelta.y), axisDelta.z);

					int bestAxis = (maxVal == axisDelta.x
						? 0
						: maxVal == axisDelta.y ? 1 : 2);

					usedAxis = bestAxis;

					// bestAxis�Ɋ�Â���bbox�̈ʒu�ɉ����ă\�[�g.
					mergeSort(
						node.refIds.begin(),
						node.refIds.end(),
						[bestAxis, this](const uint32_t a, const uint32_t b) {
						return m_refs[a].bbox.getCenter()[bestAxis] < m_refs[b].bbox.getCenter()[bestAxis];
					});

					// ����AABB�̑傫�������Z�b�g.
					objLeftBB.empty();
					objRightBB.empty();

					// distribute in left and right child evenly.
					// �������E�ƍ��ɋϓ��ɕ���.
					for (int i = 0; i < node.refIds.size(); i++) {
						const auto id = node.refIds[i];
						const auto& ref = m_refs[id];

						if (i < node.refIds.size() / 2) {
							leftList.push_back(node.refIds[i]);
							objLeftBB.expand(ref.bbox);
						}
						else {
							rightList.push_back(node.refIds[i]);
							objRightBB.expand(ref.bbox);
						}
					}
				}
				else {
					objectSort(
						node,
						sahBin,
						sahComponent,
						leftList, rightList);

					usedAxis = sahComponent;
				}
			}

			// push left and right.

			auto leftIdx = numNodes;
			auto rightIdx = leftIdx + 1;

			AT_ASSERT(leftList.size() + rightList.size() >= node.refIds.size());

			// dont with this object, deallocate memory for the current node.
			node.refIds.clear();
			node.setChild(leftIdx, rightIdx);

			// copy node data to left and right children.
			// ������ push_back ���邱�ƂŁAstd::vector �����̃������\�����ς�邱�Ƃ�����̂ŁA�Q�Ƃł��� node �̕ύX�͂��̑O�܂łɏI��点�邱��.
			m_nodes.push_back(SBVHNode());
			m_nodes.push_back(SBVHNode());

			m_nodes[leftIdx] = SBVHNode(std::move(leftList), objLeftBB);
			m_nodes[rightIdx] = SBVHNode(std::move(rightList), objRightBB);

			stack[stackpos++].nodeIdx = leftIdx;
			stack[stackpos++].nodeIdx = rightIdx;

			numNodes += 2;
		}

		AT_ASSERT(m_nodes.size() == numNodes);
	}

	inline real evalPreSplitCost(
		real leftBoxArea, int numLeft,
		real rightBoxArea, int numRight)
	{
		return leftBoxArea * numLeft + rightBoxArea * numRight;
	}

	void sbvh::findObjectSplit(
		SBVHNode& node,
		real& cost,
		aabb& leftBB,
		aabb& rightBB,
		int& splitBinPos,
		int& axis)
	{
		std::vector<Bin> bins(m_numBins);

		aabb bbCentroid = aabb(node.bbox.maxPos(), node.bbox.minPos());

		uint32_t refNum = (uint32_t)node.refIds.size();

		// compute the aabb of all centroids.
		for (uint32_t i = 0; i < refNum; i++) {
			auto id = node.refIds[i];
			const auto& ref = m_refs[id];
			auto center = ref.bbox.getCenter();
			bbCentroid.expand(center);
		}

		cost = AT_MATH_INF;
		splitBinPos = -1;
		axis = -1;

		auto centroidMin = bbCentroid.minPos();
		auto centroidMax = bbCentroid.maxPos();

		// for each dimension check the best splits.
		for (int dim = 0; dim < 3; ++dim)
		{
			// Skip empty axis.
			if ((centroidMax[dim] - centroidMin[dim]) == 0.0f) {
				continue;
			}

			const real invLen = real(1) / (centroidMax[dim] - centroidMin[dim]);

			// clear bins;
			for (uint32_t i = 0; i < m_numBins; i++) {
				bins[i] = Bin();
			}

			// distribute references in the bins based on the centroids.
			for (uint32_t i = 0; i < refNum; i++) {
				// �m�[�h�Ɋ܂܂��O�p�`��AABB�ɂ��Čv�Z.

				auto id = node.refIds[i];
				const auto& ref = m_refs[id];

				auto center = ref.bbox.getCenter();

				// �������(bins)�ւ̃C���f�b�N�X.
				// �ŏ��[�_����O�p�`AABB�̒��S�ւ̋��������̒����Ő��K�����邱�ƂŌv�Z.
				int binIdx = (int)(m_numBins * ((center[dim] - centroidMin[dim]) * invLen));

				binIdx = std::min<int>(binIdx, m_numBins - 1);
				AT_ASSERT(binIdx >= 0);

				bins[binIdx].start += 1;	// Bin�Ɋ܂܂��O�p�`�̐��𑝂₷.
				bins[binIdx].bbox.expand(ref.bbox);
			}

			// ��납�番������~�ς���.
			bins[m_numBins - 1].accum = bins[m_numBins - 1].bbox;
			bins[m_numBins - 1].end = bins[m_numBins - 1].start;

			for (int i = m_numBins - 2; i >= 0; i--) {
				// �����܂ł�AABB�S�̃T�C�Y.
				bins[i].accum = bins[i + 1].accum;

				// �������g��AABB���܂߂�.
				bins[i].accum.expand(bins[i].bbox);

				// �����܂ł̎O�p�`��.
				bins[i].end = bins[i].start + bins[i + 1].end;
			}

			// keep only one variable for forward accumulation
			auto leftBox = bins[0].bbox;

			// find split.
			for (uint32_t i = 0; i < m_numBins - 1; i++) {
				// ����͈͂��L���Ă���.
				leftBox.expand(bins[i].bbox);

				auto leftArea = leftBox.computeSurfaceArea();

				// i �����̑S�͈̔͂��E���ɂȂ�.
				auto rightArea = bins[i + 1].accum.computeSurfaceArea();

				int rightCount = bins[i + 1].end;
				int leftCount = refNum - rightCount;

				if (leftCount == 0) {
					continue;
				}

				auto splitCost = evalPreSplitCost(leftArea, leftCount, rightArea, rightCount);

				if (splitCost < cost)
				{
					cost = splitCost;
					splitBinPos = i;
					axis = dim;

					leftBB = leftBox;

					// i �����̑S�͈̔͂��E���ɂȂ�.
					rightBB = bins[i + 1].accum;
				}
			}
		}
	}

	void sbvh::findSpatialSplit(
		SBVHNode& node,
		real& cost,
		int& retLeftCount,
		int& retRightCount,
		aabb& leftBB,
		aabb& rightBB,
		int& bestAxis,
		real& splitPlane)
	{
		cost = AT_MATH_INF;
		splitPlane = -1;
		bestAxis = -1;

		std::vector<Bin> bins(m_numBins);

		uint32_t refNum = (uint32_t)node.refIds.size();

		const auto& box = node.bbox;
		const auto boxMin = box.minPos();
		const auto boxMax = box.maxPos();

		// check along each dimension
		for (int dim = 0; dim < 3; ++dim)
		{
			const auto segmentLength = boxMax[dim] - boxMin[dim];

			if (segmentLength == real(0)) {
				continue;
			}

			const auto invLen = real(1) / segmentLength;

			// ������񓖂���̎��̒���.
			const auto lenghthPerBin = segmentLength / (float)m_numBins;

			// clear bins;
			for (uint32_t i = 0; i < m_numBins; i++) {
				bins[i] = Bin();
			}

			for (uint32_t i = 0; i < refNum; i++) {
				const auto id = node.refIds[i];
				const auto& ref = m_refs[id];

				const auto triMin = ref.bbox.minPos()[dim];
				const auto triMax = ref.bbox.maxPos()[dim];

				// split each triangle into references.
				// each triangle will be recorded into multiple bins.
				// �O�p�`�����镪�����̃C���f�b�N�X�͈͂��v�Z.
				int binStartIdx = (int)(m_numBins * ((triMin - boxMin[dim]) * invLen));
				int binEndIdx = (int)(m_numBins * ((triMax - boxMin[dim]) * invLen));

				binStartIdx = aten::clamp<int>(binStartIdx, 0, m_numBins - 1);
				binEndIdx = aten::clamp<int>(binEndIdx, 0, m_numBins - 1);

				//AT_ASSERT(binStartIdx <= binEndIdx);

				for (int n = binStartIdx; n <= binEndIdx; n++) {
					const auto binMin = boxMin[dim] + n * lenghthPerBin;
					const auto binMax = boxMin[dim] + (n + 1) * lenghthPerBin;
					AT_ASSERT(binMin <= binMax);

					bins[n].bbox.expand(ref.bbox);

					if (bins[n].bbox.minPos()[dim] < binMin) {
						bins[n].bbox.minPos()[dim] = binMin;
					}

					if (bins[n].bbox.maxPos()[dim] > binMax) {
						bins[n].bbox.maxPos()[dim] = binMax;
					}
				}

				// ������񂪎�舵���O�p�`�����X�V.
				bins[binStartIdx].start++;
				bins[binEndIdx].end++;
			}

			// augment the bins from right to left.

			bins[m_numBins - 1].accum = bins[m_numBins - 1].bbox;

			for (int n = m_numBins - 2; n >= 0; n--) {
				// �����܂ł�AABB�S�̃T�C�Y.
				bins[n].accum = bins[n + 1].accum;

				// �������g��AABB���܂߂�.
				bins[n].accum.expand(bins[n].bbox);

				// �����܂ł̎O�p�`��.
				bins[n].end += bins[n + 1].end;
			}

			int leftCount = 0;
			auto leftBox = bins[0].bbox;

			// find split.
			for (uint32_t n = 0; n < m_numBins - 1; n++) {
				const int rightCount = bins[n + 1].end;
				leftCount += bins[n].start;

				leftBox.expand(bins[n].bbox);

				AT_ASSERT(leftBox.maxPos()[dim] <= bins[n + 1].bbox.minPos()[dim]);

				real leftArea = leftBox.computeSurfaceArea();
				real rightArea = bins[n + 1].accum.computeSurfaceArea();

				const real splitCost = evalPreSplitCost(leftArea, leftCount, rightArea, rightCount);

				if (splitCost < cost) {
					cost = splitCost;

					leftBB = leftBox;
					rightBB = bins[n + 1].accum;

					bestAxis = dim;
					splitPlane = bins[n + 1].accum.minPos()[dim];

					retLeftCount = leftCount;
					retRightCount = rightCount;
				}
			}
		}
	}

	void sbvh::spatialSort(
		SBVHNode& node,
		real splitPlane,
		int axis,
		real splitCost,
		int leftCnt,
		int rightCnt,
		aabb& leftBB,
		aabb& rightBB,
		std::vector<uint32_t>& leftList,
		std::vector<uint32_t>& rightList)
	{
		std::vector<Bin> bins(m_numBins);

		real rightSurfaceArea = rightBB.computeSurfaceArea();
		real leftSurfaceArea = leftBB.computeSurfaceArea();

		uint32_t refNum = (uint32_t)node.refIds.size();

		// distribute the refenreces to left, right or both children.
		for (uint32_t i = 0; i < refNum; i++) {
			const auto refIdx = node.refIds[i];
			const auto& ref = m_refs[refIdx];

			const auto refMin = ref.bbox.minPos()[axis];
			const auto refMax = ref.bbox.maxPos()[axis];

			if (refMax <= splitPlane) {
				// �����ʒu��荶.
				leftList.push_back(refIdx);
			}
			else if (refMin >= splitPlane) {
				// �����ʒu���E.
				rightList.push_back(refIdx);
			}
			else {
				// split the reference.

				// check possible unsplit.

				// NOTE
				// �_�����.
				// Csplit = SA(B1) * N1 + SA(B2) * N2
				//     C1 = SA(B1 �� B��) * N1 + SA(B2) * (N2 - 1)
				//     C2 = SA(B1) * (N1 - 1) + SA(B2 �� B��) * N2

				aabb leftUnsplitBB = leftBB;
				leftUnsplitBB.expand(ref.bbox);
				const real leftUnsplitBBArea = leftUnsplitBB.computeSurfaceArea();
				const real unsplitLeftCost = leftUnsplitBBArea * leftCnt + rightSurfaceArea * (rightCnt - 1);

				aabb rightUnsplitBB = rightBB;
				rightUnsplitBB.expand(ref.bbox);
				const real rightUnsplitBBArea = rightUnsplitBB.computeSurfaceArea();
				const real unsplitRightCost = leftSurfaceArea * (leftCnt - 1) + rightUnsplitBBArea * rightCnt;

				if (unsplitLeftCost < splitCost && unsplitLeftCost <= unsplitRightCost) {
					// put only into left only.
					leftList.push_back(refIdx);

					// update params.
					leftSurfaceArea = leftUnsplitBBArea;
					leftBB = leftUnsplitBB;
					rightCnt -= 1;
					splitCost = unsplitLeftCost;
				}
				else if (unsplitRightCost <= unsplitLeftCost && unsplitRightCost < splitCost) {
					// put only into right only.
					rightList.push_back(refIdx);

					rightSurfaceArea = leftUnsplitBBArea;
					rightBB = rightUnsplitBB;
					leftCnt -= 1;
					splitCost = unsplitRightCost;
				}
				else {
					// push left and right.
					// ��ɕ���.

					Reference leftRef(m_refs[refIdx]);

					// �o�E���f�B���O�{�b�N�X���X�V.
					if (leftRef.bbox.maxPos()[axis] > splitPlane) {
						leftRef.bbox.maxPos()[axis] = splitPlane;
					}

					Reference rightRef(m_refs[refIdx]);

					// �o�E���f�B���O�{�b�N�X���X�V.
					if (rightRef.bbox.minPos()[axis] < splitPlane) {
						rightRef.bbox.minPos()[axis] = splitPlane;
					}

					m_refs[refIdx] = leftRef;
					m_refs.push_back(rightRef);

					leftList.push_back(refIdx);
					rightList.push_back((uint32_t)m_refs.size() - 1);
				}
			}
		}
	}

	void sbvh::objectSort(
		SBVHNode& node,
		int splitBin,
		int axis,
		std::vector<uint32_t>& leftList,
		std::vector<uint32_t>& rightList)
	{
		std::vector<Bin> bins(m_numBins);

		aabb bbCentroid = aabb(node.bbox.maxPos(), node.bbox.minPos());

		const auto refNum = node.refIds.size();

		for (int i = 0; i < refNum; i++) {
			const auto id = node.refIds[i];
			const auto& ref = m_refs[id];
			auto centroid = ref.bbox.getCenter();
			bbCentroid.expand(centroid);
		}

		const auto invLen = real(1) / (bbCentroid.maxPos()[axis] - bbCentroid.minPos()[axis]);

		// distribute to left and right based on the provided split bin
		for (int i = 0; i < refNum; i++) {
			const auto id = node.refIds[i];
			const auto& ref = m_refs[id];

			auto center = ref.bbox.getCenter();

			// compute the bin index of the current reference and put to EITHER left OR right
			int binIdx = (int)(m_numBins * ((center[axis] - bbCentroid.minPos()[axis]) * invLen));

			binIdx = aten::clamp<int>(binIdx, 0, m_numBins - 1);

			if (binIdx <= splitBin) {
				leftList.push_back(id);
			}
			else {
				rightList.push_back(id);
			}
		}
	}

	void sbvh::convert(
		std::vector<ThreadedSbvhNode>& nodes,
		int offset,
		std::vector<int>& indices) const
	{
		indices.resize(m_refIndexNum);
		nodes.resize(m_nodes.size());

		// in order traversal to index nodes
		std::vector<int> inOrderIndices;
		getOrderIndex(inOrderIndices);

		struct ThreadedEntry {
			ThreadedEntry() {}

			ThreadedEntry(int idx, int _parentSiblind)
				: nodeIdx(idx), parentSibling(_parentSiblind)
			{}

			int nodeIdx{ -1 };
			int parentSibling{ -1 };
		} stack[128];

		int stackpos = 1;

		int refIndicesCount = 0;
		int nodeCount = 0;

		stack[0] = ThreadedEntry(0, -1);

		nodes[0].parent = -1;

		while (stackpos > 0) {
			auto entry = stack[stackpos - 1];
			stackpos -= 1;

			const auto& sbvhNode = m_nodes[entry.nodeIdx];
			auto& thrededNode = nodes[entry.nodeIdx];

			thrededNode.boxmin = sbvhNode.bbox.minPos();
			thrededNode.boxmax = sbvhNode.bbox.maxPos();

			if (nodeCount + 1 == nodes.size()) {
				thrededNode.hit = -1.0f;
			}
			else {
				thrededNode.hit = (float)inOrderIndices[nodeCount + 1];
			}

			if (sbvhNode.isLeaf()) {
				if (nodeCount + 1 == nodes.size()) {
					thrededNode.miss = -1.0f;
				}
				else {
					thrededNode.miss = (float)inOrderIndices[nodeCount + 1];
				}

#if (SBVH_TRIANGLE_NUM == 1)
				const auto refid = sbvhNode.refIds[0];
				const auto& ref = m_refs[refid];
				thrededNode.triid = ref.triid + m_offsetTriIdx;
				thrededNode.isleaf = 1;
#else
				thrededNode.refIdListStart = (float)refIndicesCount + offset;
				thrededNode.refIdListEnd = thrededNode.refIdListStart + (float)sbvhNode.refIds.size();
#endif

				// �Q�Ƃ���O�p�`�C���f�b�N�X��z��Ɋi�[.
				// �������Ă���̂ŁA�d������ꍇ������̂ŁA�ʔz��Ɋi�[���Ă���.
				for (int i = 0; i < sbvhNode.refIds.size(); i++) {
					const auto refId = sbvhNode.refIds[i];
					const auto& ref = m_refs[refId];

					indices[refIndicesCount++] = ref.triid + m_offsetTriIdx;
				}
			}
			else {
#if (SBVH_TRIANGLE_NUM == 1)
				thrededNode.triid = -1;
				thrededNode.isleaf = -1;
#else
				thrededNode.refIdListStart = -1;
				thrededNode.refIdListEnd = -1;
#endif

				thrededNode.miss = (float)entry.parentSibling;

				stack[stackpos++] = ThreadedEntry(sbvhNode.right, entry.parentSibling);
				stack[stackpos++] = ThreadedEntry(sbvhNode.left, sbvhNode.right);

				nodes[sbvhNode.right].parent = (float)entry.nodeIdx;
				nodes[sbvhNode.left].parent = (float)entry.nodeIdx;
			}

			nodeCount++;
		}

	}

	void sbvh::getOrderIndex(std::vector<int>& indices) const
	{
		indices.reserve(m_nodes.size());

		int stack[128] = { 0 };

		int stackpos = 1;

		while (stackpos > 0) {
			int idx = stack[stackpos - 1];
			stackpos -= 1;

			const auto& sbvhNode = m_nodes[idx];

			indices.push_back(idx);

			if (!sbvhNode.isLeaf()) {
				stack[stackpos++] = sbvhNode.right;
				stack[stackpos++] = sbvhNode.left;
			}
		}
	}

	bool sbvh::hit(
		const ray& r,
		real t_min, real t_max,
		Intersection& isect) const
	{
		auto& shapes = transformable::getShapes();
		auto& prims = face::faces();

		auto& mtxs = m_bvh.getMatrices();

		const auto& topLayerBvhNode = m_bvh.getNodes()[0];

		real hitt = AT_MATH_INF;

		int nodeid = 0;

		for (;;) {
			const ThreadedBvhNode* node = nullptr;

			if (nodeid >= 0) {
				//node = &topLayerBvhNode[nodeid];
				node = (const ThreadedBvhNode*)&m_threadedNodes[0][nodeid];
			}

			if (!node) {
				break;
			}

			bool isHit = false;

			if (node->isLeaf()) {
				Intersection isectTmp;

				auto s = shapes[(int)node->shapeid];

				if (node->exid >= 0) {
					// Traverse external linear bvh list.
					const auto& param = s->getParam();

					int mtxid = param.mtxid;

					aten::ray transformedRay;

					if (mtxid >= 0) {
						const auto& mtxW2L = mtxs[mtxid * 2 + 1];

						transformedRay = mtxW2L.applyRay(r);
					}
					else {
						transformedRay = r;
					}

					isHit = hit(
						(int)node->exid,
						transformedRay,
						t_min, t_max,
						isectTmp);
				}
				else if (node->primid >= 0) {
					// Hit test for a primitive.
					auto prim = (hitable*)prims[(int)node->primid];
					isHit = prim->hit(r, t_min, t_max, isectTmp);
					if (isHit) {
						isectTmp.objid = s->id();
					}
				}
				else {
					// Hit test for a shape.
					isHit = s->hit(r, t_min, t_max, isectTmp);
				}

				if (isHit) {
					if (isectTmp.t < isect.t) {
						isect = isectTmp;
						isect.objid = s->id();
						t_max = isect.t;
					}
				}
			}
			else {
				isHit = aten::aabb::hit(r, node->boxmin, node->boxmax, t_min, t_max);
			}

			if (isHit) {
				nodeid = (int)node->hit;
			}
			else {
				nodeid = (int)node->miss;
			}
		}

		return (isect.objid >= 0);
	}

	bool sbvh::hit(
		int exid,
		const ray& r,
		real t_min, real t_max,
		Intersection& isect) const
	{
		auto& shapes = transformable::getShapes();
		auto& prims = face::faces();

		real hitt = AT_MATH_INF;

		int nodeid = 0;

		for (;;) {
			const ThreadedSbvhNode* node = nullptr;

			if (nodeid >= 0) {
				node = &m_threadedNodes[exid][nodeid];
			}

			if (!node) {
				break;
			}

			bool isHit = false;

			if (node->isLeaf()) {
				Intersection isectTmp;

#if (SBVH_TRIANGLE_NUM == 1)
				auto prim = prims[(int)node->triid];
				isHit = prim->hit(r, t_min, t_max, isectTmp);

				if (isHit) {
					isectTmp.meshid = prim->param.gemoid;
				}
#else
				int start = (int)node->refIdListStart;
				int end = (int)node->refIdListEnd;

				auto tmpTmax = t_max;

				for (int i = start; i < end; i++) {
					int triid = m_refIndices[i];

					auto prim = prims[triid];
					auto hit = prim->hit(r, t_min, tmpTmax, isectTmp);

					if (hit) {
						isHit = true;
						tmpTmax = isectTmp.t;
						isectTmp.meshid = prim->param.gemoid;
					}
				}
#endif

				if (isHit) {
					if (isectTmp.t < isect.t) {
						isect = isectTmp;
						t_max = isect.t;
					}
				}
			}
			else {
				isHit = aten::aabb::hit(r, node->boxmin, node->boxmax, t_min, t_max);
			}

			if (isHit) {
				nodeid = (int)node->hit;
			}
			else {
				nodeid = (int)node->miss;
			}
		}

		return (isect.objid >= 0);
	}

	bool sbvh::exportTree(const char* path)
	{
		if (m_threadedNodes.size() == 0) {
			// TODO
			// through exception...
			AT_ASSERT(false);
			return false;
		}

		FILE* fp = nullptr;
		auto err = fopen_s(&fp, path, "wb");
		if (err != 0) {
			// TODO
			// through exception...
			AT_ASSERT(false);
			return false;
		}

		SbvhFileHeader header;
		{
			header.magic[0] = 'S';
			header.magic[1] = 'B';
			header.magic[2] = 'V';
			header.magic[3] = 'H';

			header.version[0] = 0;
			header.version[1] = 0;
			header.version[2] = 0;
			header.version[3] = 1;

			header.nodeNum = m_threadedNodes[0].size();

			auto bbox = getBoundingbox();
			auto boxmin = bbox.minPos();
			auto boxmax = bbox.maxPos();

			header.boxmin[0] = boxmin.x;
			header.boxmin[1] = boxmin.y;
			header.boxmin[2] = boxmin.z;

			header.boxmax[0] = boxmax.x;
			header.boxmax[1] = boxmax.y;
			header.boxmax[2] = boxmax.z;
		}

		fwrite(&header, sizeof(header), 1, fp);

		fwrite(&m_threadedNodes[0][0], sizeof(ThreadedSbvhNode), header.nodeNum, fp);

		fclose(fp);

		return true;
	}
}
