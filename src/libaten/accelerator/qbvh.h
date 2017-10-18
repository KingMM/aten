#pragma once

#include "accelerator/bvh.h"

namespace aten {
	struct QbvhNode {
		float leftChildrenIdx;
		float isLeaf{ false };
		float numChildren{ 0 };
		float padding;

		float shapeid{ -1 };	///< Object index.
		float primid{ -1 };		///< Triangle index.
		float exid{ -1 };		///< External bvh index.
		float meshid{ -1 };		///< Mesh id.

		aten::vec4 bminx;
		aten::vec4 bmaxx;

		aten::vec4 bminy;
		aten::vec4 bmaxy;

		aten::vec4 bminz;
		aten::vec4 bmaxz;
	};

	class transformable;

	class qbvh : public accelerator {
	public:
		qbvh() {}
		virtual ~qbvh() {}

	public:
		virtual void build(
			hitable** list,
			uint32_t num) override final;

		virtual bool hit(
			const ray& r,
			real t_min, real t_max,
			Intersection& isect) const override;

		std::vector<std::vector<QbvhNode>>& getNodes()
		{
			return m_listQbvhNode;
		}
		std::vector<aten::mat4>& getMatrices()
		{
			return m_mtxs;
		}

	private:
		struct BvhNode {
			bvhnode* node;
			hitable* nestParent;
			aten::mat4 mtxL2W;

			BvhNode(bvhnode* n, hitable* p, const aten::mat4& m)
				: node(n), nestParent(p), mtxL2W(m)
			{}
		};

		void registerBvhNodeToLinearList(
			bvhnode* root,
			bvhnode* parentNode,
			hitable* nestParent,
			const aten::mat4& mtxL2W,
			std::vector<BvhNode>& listBvhNode,
			std::vector<accelerator*>& listBvh,
			std::map<hitable*, std::vector<accelerator*>>& nestedBvhMap);

		uint32_t convertFromBvh(
			bool isPrimitiveLeaf,
			std::vector<BvhNode>& listBvhNode,
			std::vector<QbvhNode>& listQbvhNode);

		void setQbvhNodeLeafParams(
			bool isPrimitiveLeaf,
			const BvhNode& bvhNode,
			QbvhNode& qbvhNode);

		void fillQbvhNode(
			QbvhNode& qbvhNode,
			std::vector<BvhNode>& listBvhNode,
			int children[4],
			int numChildren);

		int getChildren(
			std::vector<BvhNode>& listBvhNode,
			int bvhNodeIdx,
			int children[4]);

		bool hit(
			int exid,
			const std::vector<std::vector<QbvhNode>>& listQbvhNode,
			const ray& r,
			real t_min, real t_max,
			Intersection& isect) const;

	private:
		bvh m_bvh;

		int m_exid{ 1 };

		std::vector<std::vector<QbvhNode>> m_listQbvhNode;
		std::vector<aten::mat4> m_mtxs;
	};
}
