#pragma once

#include "accelerator/threaded_bvh.h"

#define SBVH_TRIANGLE_NUM	(1)

#define VOXEL_TEST

namespace aten
{
	struct ThreadedSbvhNode {
		aten::vec3 boxmin;		///< AABB min position.
		float hit{ -1 };		///< Link index if ray hit.

		aten::vec3 boxmax;		///< AABB max position.
		float miss{ -1 };		///< Link index if ray miss.

#if (SBVH_TRIANGLE_NUM == 1)
		// NOTE
		// triid�̈ʒu��ThreadedBvhNode�ƍ��킹��.

		// NOTE
		// ThreadedBvhNode �ł� isleaf �̈ʒu�� shapeid ������GPU�ł� shapeid �����ă��[�t�m�[�h���ǂ������肵�Ă���.
		// ���̂��߁A�ŏ���float�Ń��[�t�m�[�h���ǂ����𔻒肷��悤�ɂ���.
		// padding �̕����� ThreadedBvhNode �ł� exid �Ȃ̂ŁA�����͏�� -1 �ɂȂ�悤�ɂ���.

		float isleaf{ -1 };
		float triid{ -1 };
		float padding{ -1 };

#ifdef VOXEL_TEST
		float voxel{ -1 };
#else
		float parent{ -1 };
#endif


		bool isLeaf() const
		{
			return (isleaf >= 0);
		}
#else
		float refIdListStart{ -1.0f };
		float refIdListEnd{ -1.0f };
		float parent{ -1.0f };
		float padding;

		bool isLeaf() const
		{
			return refIdListStart >= 0;
		}
#endif
	};

	struct SbvhTreelet {
		uint32_t idxInBvhTree;
		uint32_t depth;
		std::vector<uint32_t> leafChildren;
		std::vector<uint32_t> tris;
	};

	struct BvhVoxel {
		float nmlX;
		float nmlY;
		struct {
			uint32_t signNmlZ : 1;
			uint32_t radius : 31;
		};
		uint32_t nodeid{ 0 };

		float clrR;
		struct {
			uint32_t clrG : 31;
			uint32_t padding : 1;
		};
		float clrB;
		struct {
			uint32_t exid : 16;
			uint32_t lod : 16;
		};
	};

	// NOTE
	// GPGPU�����p�ɗ����𓯂���������ԏ�Ɋi�[���邽�߁A�����T�C�Y�łȂ��Ƃ����Ȃ�.
	AT_STATICASSERT(sizeof(ThreadedSbvhNode) == sizeof(ThreadedBvhNode));

	class sbvh : public accelerator {
	public:
		sbvh() : accelerator(AccelType::Sbvh) {}
		virtual ~sbvh() {}

	public:
		virtual void build(
			hitable** list,
			uint32_t num,
			aabb* bbox = nullptr) override final;

		virtual void buildVoxel(
			uint32_t exid,
			uint32_t offset) override final;

		virtual bool hit(
			const ray& r,
			real t_min, real t_max,
			Intersection& isect) const override;

		virtual bool hit(
			const ray& r,
			real t_min, real t_max,
			Intersection& isect,
			bool enableLod) const override;

		virtual bool exportTree(const char* path) override final;
		virtual bool importTree(const char* path, int offsetTriIdx) override final;

		ThreadedBVH& getTopLayer()
		{
			return m_bvh;
		}

		virtual void drawAABB(
			aten::hitable::FuncDrawAABB func,
			const aten::mat4& mtxL2W) override final;


		virtual void update() override final;

		const std::vector<std::vector<ThreadedSbvhNode>>& getNodes() const
		{
			return m_threadedNodes;
		}
		const std::vector<aten::mat4>& getMatrices() const
		{
			return m_bvh.getMatrices();
		}
		const std::vector<BvhVoxel>& getVoxels() const
		{
			return m_voxels;
		}

		void prepareToComputeVoxelLodError(uint32_t height, real verticalFov);

	private:
		void buildInternal(
			hitable** list,
			uint32_t num);

		void convert(
			std::vector<ThreadedSbvhNode>& nodes,
			int offset,
			std::vector<int>& indices) const;

		bool hit(
			int exid,
			const ray& r,
			real t_min, real t_max,
			Intersection& isect) const;

		struct SBVHNode {
			SBVHNode() {}

			SBVHNode(const std::vector<uint32_t>&& indices, const aabb& box)
				: refIds(indices), bbox(box)
			{}

			bool isLeaf() const
			{
				return leaf;
			}

			void setChild(int leftId, int rightId)
			{
				leaf = false;
				left = leftId;
				right = rightId;
			}

			aabb bbox;

			// Indices for triangls which this node has.
			std::vector<uint32_t> refIds;

			// Child left;
			int left{ -1 };

			// Child right;
			int right{ -1 };

			int parent{ -1 };
			uint32_t depth{ 0 };

			int voxelIdx{ -1 };

			bool leaf{ true };
			bool isTreeletRoot{ false };
		};

		// �������.
		struct Bin {
			Bin() {}

			// bbox of bin.
			aabb bbox;

			// accumulated bbox of bin left/right.
			aabb accum;

			// references starting here.
			int start{ 0 };

			// references ending here.
			int end{ 0 };
		};

		// �����O�p�`���.
		struct Reference {
			Reference() {}

			Reference(int id) : triid(id) {}

			// �������̎O�p�`�C���f�b�N�X.
			int triid;

			// �����������AABB.
			aabb bbox;
		};

		void findObjectSplit(
			SBVHNode& node,
			real& cost,
			aabb& leftBB,
			aabb& rightBB,
			int& splitBinPos,
			int& axis);

		void findSpatialSplit(
			SBVHNode& node,
			real& cost,
			int& leftCount,
			int& rightCount,
			aabb& leftBB,
			aabb& rightBB,
			int& bestAxis,
			real& splitPlane);

		void spatialSort(
			SBVHNode& node,
			real splitPlane,
			int axis,
			real splitCost,
			int leftCnt,
			int rightCnt,
			aabb& leftBB,
			aabb& rightBB,
			std::vector<uint32_t>& leftList,
			std::vector<uint32_t>& rightList);

		void objectSort(
			SBVHNode& node,
			int splitBin,
			int axis,
			std::vector<uint32_t>& leftList,
			std::vector<uint32_t>& rightList);

		void getOrderIndex(std::vector<int>& indices) const;

		void makeTreelet();
		void onMakeTreelet(
			uint32_t idx,
			const sbvh::SBVHNode& root);

	private:
		ThreadedBVH m_bvh;

		// �����ő吔.
		uint32_t m_numBins{ 16 };

		// �m�[�h������̍ő�O�p�`��.
		uint32_t m_maxTriangles{ SBVH_TRIANGLE_NUM };

		uint32_t m_refIndexNum{ 0 };

		int m_offsetTriIdx{ 0 };

		std::vector<SBVHNode> m_nodes;

		// �O�p�`��񃊃X�g.
		// �����ł����O�p�`���Ƃ͕������ꂽ or ����Ă��Ȃ��O�p�`�̏��.
		std::vector<Reference> m_refs;

		// For layer.
		std::vector<std::vector<ThreadedSbvhNode>> m_threadedNodes;
		std::vector<int> m_refIndices;

		uint32_t m_maxDepth{ 0 };

		// For voxelize.
		float m_maxVoxelRadius{ 0.0f };
		std::vector<SbvhTreelet> m_treelets;
		std::vector<BvhVoxel> m_voxels;

		// For voxel hit test.
		real m_voxelLodErrorC{ real(1) };
	};
}
