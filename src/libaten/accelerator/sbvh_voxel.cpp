#include "accelerator/sbvh.h"

#include <omp.h>

#include <algorithm>
#include <iterator>
#include <numeric>

//#pragma optimize( "", off)

namespace aten
{
	// TODO
	static const int VoxelDepth = 3;
	static const real VoxelVolumeThreshold = real(0.5);

	inline int computeVoxelLodLevel(int depth, int maxDepth)
	{
		int approximateMaxVoxelDepth = (maxDepth - 1) / VoxelDepth * VoxelDepth;
		int approximateMaxVoxelLodLevel = approximateMaxVoxelDepth / VoxelDepth - 1;	// Zero origin.

		int lod = depth / (VoxelDepth + 1);

		// NOTE
		// �[���ق�LOD���x����������悤�ɂ���.
		int ret = approximateMaxVoxelLodLevel - lod;
		AT_ASSERT(ret >= 0);

		return ret;
	}

	void sbvh::makeTreelet()
	{
		if (m_treelets.size() > 0) {
			// Imported already.
			return;
		}

		int stack[128] = { 0 };

		aabb wholeBox = m_nodes[0].bbox;

		// NOTE
		// root�m�[�h�͑ΏۊO.
		for (size_t i = 1; i < m_nodes.size(); i++) {
			auto* node = &m_nodes[i];

			bool isTreeletRoot = (((node->depth % VoxelDepth) == 0) && !node->isLeaf());

			if (isTreeletRoot) {
				auto lod = computeVoxelLodLevel(node->depth, m_maxDepth);

				auto ratioThreshold = VoxelVolumeThreshold / (lod + 1);

				auto ratio = wholeBox.computeRatio(node->bbox);

				std::map<uint32_t, SBVHNode*> treeletRoots;

				if (ratio < ratioThreshold) {
					if (!node->isTreeletRoot) {
						treeletRoots.insert(std::pair<uint32_t, SBVHNode*>((uint32_t)i, node));
						node->isTreeletRoot = true;
					}
				}
				else {
					stack[0] = node->left;
					stack[1] = node->right;
					int stackpos = 2;

					bool enableTraverseToChild = true;
					
					while (stackpos > 0) {
						auto idx = stack[stackpos - 1];
						stackpos -= 1;

						if (idx < 0) {
							return;
						}

						if (idx == node->left) {
							// ����_�ɖ߂����̂ŁA�q���̒T��������.
							enableTraverseToChild = true;
						}

						auto* n = &m_nodes[idx];

						auto tmpLod = computeVoxelLodLevel(n->depth, m_maxDepth);
						if (tmpLod > lod) {
							// �{�N�Z���̃��x�������̃��x���ɂȂ����̂ŁA�ł��؂�.
							continue;
						}

						if (!n->isLeaf()) {
							ratio = wholeBox.computeRatio(n->bbox);
							if (ratio < ratioThreshold) {
								if (!n->isTreeletRoot) {
									treeletRoots.insert(std::pair<uint32_t, SBVHNode*>(idx, n));
									n->isTreeletRoot = true;

									// Treelet�̃��[�g���͌������̂ŁA����ȏ�͎q����T�����Ȃ�.
									enableTraverseToChild = false;
								}
							}
							else if (enableTraverseToChild) {
								stack[stackpos++] = n->left;
								stack[stackpos++] = n->right;
							}
						}
					}
				}

				for (auto it = treeletRoots.begin(); it != treeletRoots.end(); it++) {
					auto idx = it->first;
					auto node = it->second;

					onMakeTreelet(idx, *node);
				}
			}
		}
	}

	void sbvh::onMakeTreelet(
		uint32_t idx,
		const sbvh::SBVHNode& root)
	{
		m_treelets.push_back(SbvhTreelet());
		auto& treelet = m_treelets[m_treelets.size() - 1];

		treelet.idxInBvhTree = idx;
		treelet.depth = root.depth;

		int stack[128] = { 0 };

		stack[0] = root.left;
		stack[1] = root.right;
		int stackpos = 2;

		while (stackpos > 0) {
			int idx = stack[stackpos - 1];
			stackpos -= 1;

			const auto& sbvhNode = m_nodes[idx];

			if (sbvhNode.isLeaf()) {
				treelet.leafChildren.push_back(idx);

				const auto refid = sbvhNode.refIds[0];
				const auto& ref = m_refs[refid];
				const auto triid = ref.triid + m_offsetTriIdx;
				treelet.tris.push_back(triid);
			}
			else {
				stack[stackpos++] = sbvhNode.right;
				stack[stackpos++] = sbvhNode.left;
			}
		}
	}

	bool barycentric(
		const aten::vec3& v1,
		const aten::vec3& v2,
		const aten::vec3& v3,
		const aten::vec3& p, float &lambda1, float &lambda2)
	{
#if 1
		// NOTE
		// https://blogs.msdn.microsoft.com/rezanour/2011/08/07/barycentric-coordinates-and-point-in-triangle-tests/

		// Prepare our barycentric variables
		auto u = v2 - v1;
		auto v = v3 - v1;
		auto w = p - v1;

		auto vCrossW = cross(v, w);
		auto vCrossU = cross(v, u);

		auto uCrossW = cross(u, w);
		auto uCrossV = cross(u, v);

		// At this point, we know that r and t and both > 0.
		// Therefore, as long as their sum is <= 1, each must be less <= 1
		float denom = length(uCrossV);
		lambda1 = length(vCrossW) / denom;
		lambda2 = length(uCrossW) / denom;
#else
		auto f1 = v1 - p;
		auto f2 = v2 - p;
		auto f3 = v3 - p;
		auto c = cross(v2 - v1, v3 - v1);
		float area = length(c);
		lambda1 = length(cross(f2, f3));
		lambda2 = length(cross(f3, f1));

		lambda1 /= area;
		lambda2 /= area;
#endif

		return lambda1 >= 0.0f && lambda2 >= 0.0f && lambda1 + lambda2 <= 1.0f;
	}

	void sbvh::buildVoxel(
		uint32_t exid,
		uint32_t offset)
	{
		if (m_voxels.size() > 0 || m_nodes.empty()) {
			// Imported already.

			AT_ASSERT(!m_threadedNodes.empty());
			AT_ASSERT(!m_threadedNodes[0].empty());

			// Add voxel index offset.
			for (auto& threadedNode : m_threadedNodes[0])
			{
				if (threadedNode.voxel >= 0) {
					threadedNode.voxel += offset;
				}
			}

			// Set correct external node index.
			for (auto& voxel : m_voxels) {
				voxel.exid += exid;
			}

			return;
		}

		m_maxVoxelRadius = 0.0f;

		const auto& faces = aten::face::faces();
		const auto& vertices = aten::VertexManager::getVertices();
		const auto& mtrls = aten::material::getMaterials();

		for (size_t i = 0; i < m_treelets.size(); i++) {
			const auto& treelet = m_treelets[i];

			auto& sbvhNode = m_nodes[treelet.idxInBvhTree];

			// Speficfy having voxel.
			sbvhNode.voxelIdx = (int)i + offset;

			auto center = sbvhNode.bbox.getCenter();

			aten::vec3 avgNormal(0);
			aten::vec3 avgColor(0);

			BvhVoxel voxel;
			voxel.nodeid = treelet.idxInBvhTree;
			voxel.exid = exid;
			voxel.lod = computeVoxelLodLevel(sbvhNode.depth, m_maxDepth);

			for (const auto tid : treelet.tris) {
				const auto triparam = faces[tid]->param;

				const auto& v0 = vertices[triparam.idx[0]];
				const auto& v1 = vertices[triparam.idx[1]];
				const auto& v2 = vertices[triparam.idx[2]];

				float lambda1, lambda2;

				if (!barycentric(v0.pos, v1.pos, v2.pos, center, lambda1, lambda2)) {
					lambda1 = std::min<float>(std::max<float>(lambda1, 0.0f), 1.0f);
					lambda2 = std::min<float>(std::max<float>(lambda2, 0.0f), 1.0f);
					float tau = lambda1 + lambda2;
					if (tau > 1.0f) {
						lambda1 /= tau;
						lambda2 /= tau;
					}
				}

				float lambda3 = 1.0f - lambda1 - lambda2;

				auto normal = v0.nml * lambda1 + v1.nml * lambda2 + v2.nml * lambda3;
				if (triparam.needNormal > 0) {
					auto e01 = v1.pos - v0.pos;
					auto e02 = v2.pos - v0.pos;

					e01.w = e02.w = real(0);

					normal = normalize(cross(e01, e02));
				}

				auto uv = v0.uv * lambda1 + v1.uv * lambda2 + v2.uv * lambda3;

				const auto mtrl = mtrls[triparam.mtrlid];
				auto color = mtrl->sampleAlbedoMap(uv.x, uv.y);
				color *= mtrl->color();

				avgNormal += normal;
				avgColor += color;
			}

			int cnt = (int)treelet.tris.size();

			avgNormal /= cnt;
			avgColor /= cnt;

			voxel.clrR = avgColor.r;
			voxel.clrG = collapseTo31bitInteger(avgColor.g);
			voxel.clrB = avgColor.b;

			avgNormal = normalize(avgNormal);

			voxel.nmlX = avgNormal.x;
			voxel.nmlY = avgNormal.y;
			voxel.signNmlZ = avgNormal.z < real(0) ? 1 : 0;

			float radius = sbvhNode.bbox.getDiagonalLenght() * 0.5f;
			voxel.radius = collapseTo31bitInteger(radius);

			m_maxVoxelRadius = std::max(m_maxVoxelRadius, radius);

			m_voxels.push_back(voxel);
		}
	}

	void sbvh::prepareToComputeVoxelLodError(uint32_t height, real verticalFov)
	{
		// NOTE
		// http://sirkan.iit.bme.hu/~szirmay/voxlod_cgf_final.pdf
		// 6.2. LOD error metric

		real theta = Deg2Rad(verticalFov);
		real lambda = (height * real(0.5)) / (aten::tan(theta * real(0.5)));

		m_voxelLodErrorC = m_maxVoxelRadius / lambda;
	}
}
