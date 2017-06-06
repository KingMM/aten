#include "accelerator/bvh.h"
#include "shape/tranformable.h"
#include "object/object.h"

#include <random>
#include <vector>

#define BVH_SAH
//#define TEST_NODE_LIST

namespace aten {
	int compareX(const void* a, const void* b)
	{
		const hitable* ah = *(hitable**)a;
		const hitable* bh = *(hitable**)b;

		auto left = ah->getBoundingbox();
		auto right = bh->getBoundingbox();

		if (left.minPos().x < right.minPos().x) {
			return -1;
		}
		else {
			return 1;
		}
	}

	int compareY(const void* a, const void* b)
	{
		hitable* ah = *(hitable**)a;
		hitable* bh = *(hitable**)b;

		auto left = ah->getBoundingbox();
		auto right = bh->getBoundingbox();

		if (left.minPos().y < right.minPos().y) {
			return -1;
		}
		else {
			return 1;
		}
	}

	int compareZ(const void* a, const void* b)
	{
		hitable* ah = *(hitable**)a;
		hitable* bh = *(hitable**)b;

		auto left = ah->getBoundingbox();
		auto right = bh->getBoundingbox();

		if (left.minPos().z < right.minPos().z) {
			return -1;
		}
		else {
			return 1;
		}
	}

	void sortList(bvhnode**& list, uint32_t num, int axis)
	{
		switch (axis) {
		case 0:
			::qsort(list, num, sizeof(bvhnode*), compareX);
			break;
		case 1:
			::qsort(list, num, sizeof(bvhnode*), compareY);
			break;
		default:
			::qsort(list, num, sizeof(bvhnode*), compareZ);
			break;
		}
	}

	void bvhnode::build(
		bvhnode** list,
		uint32_t num)
	{
#ifdef BVH_SAH
		bvh::buildBySAH(this, list, num);
#else
		build(list, num, true);
#endif
	}

	void bvhnode::build(
		bvhnode** list,
		uint32_t num,
		bool needSort)
	{
		if (needSort) {
			// TODO
			//int axis = (int)(::rand() % 3);
			int axis = 0;

			sortList(list, num, axis);
		}

		if (num == 1) {
			m_left = list[0];
		}
		else if (num == 2) {
			m_left = list[0];
			m_right = list[1];
		}
		else {
			m_left = new bvhnode(list, num / 2);
			m_right = new bvhnode(list + num / 2, num - num / 2);
		}

		if (m_left && m_right) {
			auto boxLeft = m_left->getBoundingbox();
			auto boxRight = m_right->getBoundingbox();

			m_aabb = aabb::merge(boxLeft, boxRight);
		}
		else {
			auto boxLeft = m_left->getBoundingbox();

			m_aabb = boxLeft;
		}
	}

	bool bvhnode::hit(
		const ray& r,
		real t_min, real t_max,
		Intersection& isect) const
	{
		auto bbox = getBoundingbox();
		auto isHit = bbox.hit(r, t_min, t_max);

		if (isHit) {
			isHit = bvh::hit(this, r, t_min, t_max, isect);
		}

		return isHit;
	}

	///////////////////////////////////////////////////////
#ifdef TEST_NODE_LIST
	static std::vector<std::vector<BVHNode>> snodes;
	static std::vector<aten::mat4> smtxs;
#endif

	void bvh::build(
		bvhnode** list,
		uint32_t num)
	{
		// TODO
		//int axis = (int)(::rand() % 3);
		int axis = 0;

		sortList(list, num, axis);

		m_root = new bvhnode();
#ifdef BVH_SAH
		buildBySAH(m_root, list, num);
#else
		m_root->build(&list[0], num, false);
#endif

#ifdef TEST_NODE_LIST
		if (snodes.empty()) {
			collectNodes(snodes, smtxs);
			//bvh::dumpCollectedNodes(snodes, "_nodes.txt");
		}
#endif
	}

#ifdef TEST_NODE_LIST
	static bool _hit(
		BVHNode* nodes,
		const ray& r,
		real t_min, real t_max,
		Intersection& isect)
	{
		static const uint32_t stacksize = 64;
		BVHNode* stackbuf[stacksize] = { nullptr };

		stackbuf[0] = &nodes[0];

		int stackpos = 1;

		auto& shapes = transformable::getShapes();
		auto& prims = face::faces();

		real hitt = AT_MATH_INF;
		int exid = -1;

		int shapeid = -1;
		int tmpShapeid = -1;

		while (stackpos > 0) {
			auto node = stackbuf[stackpos - 1];

			stackpos -= 1;

			if (node->isLeaf()) {
				if (node->nestid >= 0) {
					if (aten::aabb::hit(r, node->boxmin, node->boxmax, t_min, t_max)) {
						stackbuf[stackpos++] = &nodes[(int)node->nestid];
						tmpShapeid = (int)node->shapeid;
					}
				}
				else {
					Intersection isectTmp;

					bool isHit = false;

					auto s = shapes[(int)node->shapeid];

					int tmpexid = -1;

					if (node->exid >= 0) {
						real t = AT_MATH_INF;
						isHit = aten::aabb::hit(r, node->boxmin, node->boxmax, t_min, t_max, &t);
						isectTmp.t = t;
						tmpexid = node->exid;
					}
					else if (node->primid >= 0) {
						auto prim = (hitable*)prims[(int)node->primid];
						isHit = prim->hit(r, t_min, t_max, isectTmp);
						if (isHit) {
							isectTmp.objid = s->id();
						}
					}
					else {
						isHit = s->hit(r, t_min, t_max, isectTmp);
						tmpexid = -1;
					}

					if (isHit) {
						if (isectTmp.t < hitt) {
							hitt = isectTmp.t;
							exid = tmpexid;
							shapeid = tmpShapeid;
						}

						if (tmpexid < 0) {
							if (isectTmp.t < isect.t) {
								isect = isectTmp;
							}
						}
					}
				}
			}
			else {
				if (aten::aabb::hit(r, node->boxmin, node->boxmax, t_min, t_max)) {
					if (node->left >= 0) {
						stackbuf[stackpos++] = &nodes[(int)node->left];
					}
					if (node->right >= 0) {
						stackbuf[stackpos++] = &nodes[(int)node->right];
					}

					if (stackpos > stacksize) {
						AT_ASSERT(false);
						return false;
					}
				}
			}
		}

		if (exid >= 0) {
			const auto s = shapes[shapeid];
			const auto& param = s->getParam();

			int mtxid = param.mtxid;

			const auto& mtxW2L = smtxs[mtxid * 2 + 1];

			auto transformedRay = mtxW2L.applyRay(r);

			Intersection isectTmp;

			if (_hit(&snodes[exid][0], transformedRay, t_min, t_max, isectTmp)) {
				if (isectTmp.t < isect.t) {
					isect = isectTmp;
				}
			}
		}

		return (isect.objid >= 0);
	}
#endif

	bool bvh::hit(
		const ray& r,
		real t_min, real t_max,
		Intersection& isect) const
	{
#ifdef TEST_NODE_LIST
		bool isHit = _hit(&snodes[0][0], r, t_min, t_max, isect);
		return isHit;
#else
		bool isHit = hit(m_root, r, t_min, t_max, isect);
		return isHit;
#endif
	}

	bool bvh::hit(
		const bvhnode* root,
		const ray& r,
		real t_min, real t_max,
		Intersection& isect)
	{
		// NOTE
		// https://devblogs.nvidia.com/parallelforall/thinking-parallel-part-ii-tree-traversal-gpu/

		// TODO
		// stack size.
		static const uint32_t stacksize = 64;
		const bvhnode* stackbuf[stacksize];

		stackbuf[0] = root;
		int stackpos = 1;

		while (stackpos > 0) {
			auto node = stackbuf[stackpos - 1];

			stackpos -= 1;

			if (node->isLeaf()) {
				Intersection isectTmp;
				if (node->hit(r, t_min, t_max, isectTmp)) {
					if (isectTmp.t < isect.t) {
						isect = isectTmp;
					}
				}
			}
			else {
				if (node->getBoundingbox().hit(r, t_min, t_max)) {
					if (node->m_left) {
						stackbuf[stackpos++] = node->m_left;
					}
					if (node->m_right) {
						stackbuf[stackpos++] = node->m_right;
					}

					if (stackpos > stacksize) {
						AT_ASSERT(false);
						return false;
					}
				}
			}
		}

		return (isect.objid >= 0);
	}

	template<typename T>
	static void pop_front(std::vector<T>& vec)
	{
		AT_ASSERT(!vec.empty());
		vec.erase(vec.begin());
	}

	void bvh::buildBySAH(
		bvhnode* root,
		bvhnode** list,
		uint32_t num)
	{
		// NOTE
		// http://qiita.com/omochi64/items/9336f57118ba918f82ec

#if 0
		// ある？
		AT_ASSERT(num > 0);

		// 全体を覆うAABBを計算.
		root->m_aabb = list[0]->getBoundingbox();
		for (uint32_t i = 1; i < num; i++) {
			auto bbox = list[i]->getBoundingbox();
			root->m_aabb = aabb::merge(root->m_aabb, bbox);
		}

		if (num == 1) {
			// １個しかないので、これだけで終了.
			root->m_left = list[0];
			return;
		}
		else if (num == 2) {
			// ２個だけのときは適当にソートして、終了.
			int axis = (int)(::rand() % 3);

			sortList(list, num, axis);

			root->m_left = list[0];
			root->m_right = list[1];

			return;
		}

		// Triangleとrayのヒットにかかる処理時間の見積もり.
		static const real T_tri = 1;  // 適当.

		// AABBとrayのヒットにかかる処理時間の見積もり.
		static const real T_aabb = 1;  // 適当.

		// 領域分割をせず、polygons を含む葉ノードを構築する場合を暫定の bestCost にする.
		//auto bestCost = T_tri * num;
		uint32_t bestCost = UINT32_MAX;	// 限界まで分割したいので、適当に大きい値にしておく.

		// 分割に最も良い軸 (0:x, 1:y, 2:z)
		int bestAxis = -1;

		// 最も良い分割場所
		int bestSplitIndex = -1;

		// ノード全体のAABBの表面積
		auto rootSurfaceArea = root->m_aabb.computeSurfaceArea();

		for (int axis = 0; axis < 3; axis++) {
			// ポリゴンリストを、それぞれのAABBの中心座標を使い、axis でソートする.
			sortList(list, num, axis);

			// AABBの表面積リスト。s1SA[i], s2SA[i] は、
			// 「S1側にi個、S2側に(polygons.size()-i)個ポリゴンがあるように分割」したときの表面積
			std::vector<real> s1SurfaceArea(num + 1, AT_MATH_INF);
			std::vector<real> s2SurfaceArea(num + 1, AT_MATH_INF);

			// 分割された2つの領域.
			std::vector<bvhnode*> s1;					// 右側.
			std::vector<bvhnode*> s2(list, list + num);	// 左側.

			// NOTE
			// s2側から取り出して、s1に格納するため、s2にリストを全部入れる.

			aabb s1bbox;

			// 可能な分割方法について、s1側の AABB の表面積を計算.
			for (uint32_t i = 0; i <= num; i++) {
				s1SurfaceArea[i] = s1bbox.computeSurfaceArea();

				if (s2.size() > 0) {
					// s2側で、axis について最左 (最小位置) にいるポリゴンをs1の最右 (最大位置) に移す
					auto p = s2.front();
					s1.push_back(p);
					pop_front(s2);

					// 移したポリゴンのAABBをマージしてs1のAABBとする.
					auto bbox = p->getBoundingbox();
					s1bbox = aabb::merge(s1bbox, bbox);
				}
			}

			// 逆にs2側のAABBの表面積を計算しつつ、SAH を計算.
			aabb s2bbox;

			for (int i = num; i >= 0; i--) {
				s2SurfaceArea[i] = s2bbox.computeSurfaceArea();

				if (s1.size() > 0 && s2.size() > 0) {
					// SAH-based cost の計算.
					auto cost =	2 * T_aabb
						+ (s1SurfaceArea[i] * s1.size() + s2SurfaceArea[i] * s2.size()) * T_tri / rootSurfaceArea;

					// 最良コストが更新されたか.
					if (cost < bestCost) {
						bestCost = cost;
						bestAxis = axis;
						bestSplitIndex = i;
					}
				}

				if (s1.size() > 0) {
					// s1側で、axis について最右にいるポリゴンをs2の最左に移す.
					auto p = s1.back();
					
					// 先頭に挿入.
					s2.insert(s2.begin(), p); 

					s1.pop_back();

					// 移したポリゴンのAABBをマージしてS2のAABBとする.
					auto bbox = p->getBoundingbox();
					s2bbox = aabb::merge(s2bbox, bbox);
				}
			}
		}

		if (bestAxis >= 0) {
			// bestAxis に基づき、左右に分割.
			// bestAxis でソート.
			sortList(list, num, bestAxis);

			// 左右の子ノードを作成.
			root->m_left = new bvhnode();
			root->m_right = new bvhnode();

			// リストを分割.
			int leftListNum = bestSplitIndex;
			int rightListNum = num - leftListNum;

			AT_ASSERT(rightListNum > 0);

			// 再帰処理
			buildBySAH(root->m_left, list, leftListNum);
			buildBySAH(root->m_right, list + leftListNum, rightListNum);
		}
#else
		struct BuildInfo {
			bvhnode* node{ nullptr };
			bvhnode** list{ nullptr };
			uint32_t num{ 0 };

			BuildInfo() {}
			BuildInfo(bvhnode* _node, bvhnode** _list, uint32_t _num)
			{
				node = _node;
				list = _list;
				num = _num;
			}
		};

		std::vector<BuildInfo> stacks;
		stacks.push_back(BuildInfo());	// Add terminator.

		BuildInfo info(root, list, num);

		while (info.node != nullptr)
		{
			// 全体を覆うAABBを計算.
			info.node->m_aabb = info.list[0]->getBoundingbox();
			for (uint32_t i = 1; i < info.num; i++) {
				auto bbox = info.list[i]->getBoundingbox();
				info.node->m_aabb = aabb::merge(info.node->m_aabb, bbox);
			}

			if (info.num == 1) {
				// １個しかないので、これだけで終了.
				info.node->m_left = info.list[0];

				info = stacks.back();
				stacks.pop_back();
				continue;
			}
			else if (info.num == 2) {
				// ２個だけのときは適当にソートして、終了.

				// TODO
				//int axis = (int)(::rand() % 3);
				int axis = 0;

				sortList(info.list, info.num, axis);

				info.node->m_left = info.list[0];
				info.node->m_right = info.list[1];

				info = stacks.back();
				stacks.pop_back();
				continue;
			}

			// Triangleとrayのヒットにかかる処理時間の見積もり.
			static const real T_tri = 1;  // 適当.

			// AABBとrayのヒットにかかる処理時間の見積もり.
			static const real T_aabb = 1;  // 適当.

			// 領域分割をせず、polygons を含む葉ノードを構築する場合を暫定の bestCost にする.
			//auto bestCost = T_tri * num;
			uint32_t bestCost = UINT32_MAX;	// 限界まで分割したいので、適当に大きい値にしておく.

			// 分割に最も良い軸 (0:x, 1:y, 2:z)
			int bestAxis = -1;

			// 最も良い分割場所
			int bestSplitIndex = -1;

			// ノード全体のAABBの表面積
			auto rootSurfaceArea = info.node->m_aabb.computeSurfaceArea();

			for (int axis = 0; axis < 3; axis++) {
				// ポリゴンリストを、それぞれのAABBの中心座標を使い、axis でソートする.
				sortList(info.list, info.num, axis);

				// AABBの表面積リスト。s1SA[i], s2SA[i] は、
				// 「S1側にi個、S2側に(polygons.size()-i)個ポリゴンがあるように分割」したときの表面積
				std::vector<real> s1SurfaceArea(info.num + 1, AT_MATH_INF);
				std::vector<real> s2SurfaceArea(info.num + 1, AT_MATH_INF);

				// 分割された2つの領域.
				std::vector<bvhnode*> s1;					// 右側.
				std::vector<bvhnode*> s2(info.list, info.list + info.num);	// 左側.

				// NOTE
				// s2側から取り出して、s1に格納するため、s2にリストを全部入れる.

				aabb s1bbox;

				// 可能な分割方法について、s1側の AABB の表面積を計算.
				for (uint32_t i = 0; i <= info.num; i++) {
					s1SurfaceArea[i] = s1bbox.computeSurfaceArea();

					if (s2.size() > 0) {
						// s2側で、axis について最左 (最小位置) にいるポリゴンをs1の最右 (最大位置) に移す
						auto p = s2.front();
						s1.push_back(p);
						pop_front(s2);

						// 移したポリゴンのAABBをマージしてs1のAABBとする.
						auto bbox = p->getBoundingbox();
						s1bbox = aabb::merge(s1bbox, bbox);
					}
				}

				// 逆にs2側のAABBの表面積を計算しつつ、SAH を計算.
				aabb s2bbox;

				for (int i = info.num; i >= 0; i--) {
					s2SurfaceArea[i] = s2bbox.computeSurfaceArea();

					if (s1.size() > 0 && s2.size() > 0) {
						// SAH-based cost の計算.
						auto cost = 2 * T_aabb
							+ (s1SurfaceArea[i] * s1.size() + s2SurfaceArea[i] * s2.size()) * T_tri / rootSurfaceArea;

						// 最良コストが更新されたか.
						if (cost < bestCost) {
							bestCost = cost;
							bestAxis = axis;
							bestSplitIndex = i;
						}
					}

					if (s1.size() > 0) {
						// s1側で、axis について最右にいるポリゴンをs2の最左に移す.
						auto p = s1.back();

						// 先頭に挿入.
						s2.insert(s2.begin(), p);

						s1.pop_back();

						// 移したポリゴンのAABBをマージしてS2のAABBとする.
						auto bbox = p->getBoundingbox();
						s2bbox = aabb::merge(s2bbox, bbox);
					}
				}
			}

			if (bestAxis >= 0) {
				// bestAxis に基づき、左右に分割.
				// bestAxis でソート.
				sortList(info.list, info.num, bestAxis);

				// 左右の子ノードを作成.
				info.node->m_left = new bvhnode();
				info.node->m_right = new bvhnode();

				// リストを分割.
				int leftListNum = bestSplitIndex;
				int rightListNum = info.num - leftListNum;

				AT_ASSERT(rightListNum > 0);

				auto _list = info.list;

				auto _left = info.node->m_left;
				auto _right = info.node->m_right;

				info = BuildInfo(_left, _list, leftListNum);
				stacks.push_back(BuildInfo(_right, _list + leftListNum, rightListNum));
			}
			else {
				// TODO
				info = stacks.back();
				stacks.pop_back();
			}
		}
#endif
	}

	void bvh::collectNodes(
		std::vector<std::vector<BVHNode>>& nodes,
		std::vector<aten::mat4>& mtxs) const
	{
		nodes.push_back(std::vector<BVHNode>());
		auto& rootnode = nodes[0];

		int order = setTraverseOrder(m_root, 0);
		collectNodes(m_root, rootnode, nullptr);

		std::map<hitable*, int> cache;

		auto& shapes = const_cast<std::vector<transformable*>&>(transformable::getShapes());

		for (auto s : shapes) {
			const auto idx = s->m_traverseOrder;
			if (idx >= 0) {
				auto& param = const_cast<aten::ShapeParameter&>(s->getParam());

				if (param.type == ShapeType::Instance) {
					auto& node = rootnode[idx];

					// Search if object which instance has is collected.
					auto obj = s->getHasObject();
					auto found = cache.find(const_cast<hitable*>(obj));

					if (found != cache.end()) {
						// Collected.
						node.nestid = found->second;
					}
					else {
						// Not collected yet.
						node.nestid = rootnode.size();

						order = s->collectInternalNodes(nodes, order, nullptr);
					}

					param.mtxid = mtxs.size() / 2;

					aten::mat4 mtxL2W, mtxW2L;
					s->getMatrices(mtxL2W, mtxW2L);

					mtxs.push_back(mtxL2W);
					mtxs.push_back(mtxW2L);
				}
			}
		}
	}

	int bvh::setTraverseOrder(bvhnode* root, int curOrder)
	{
		static const uint32_t stacksize = 64;
		bvhnode* stackbuf[stacksize] = { nullptr };
		bvhnode** stack = &stackbuf[0];

		// push terminator.
		*stack++ = nullptr;

		int stackpos = 1;

		bvhnode* pnode = root;

		int order = curOrder;

		while (pnode != nullptr) {
			bvhnode* pleft = pnode->m_left;
			bvhnode* pright = pnode->m_right;

			pnode->m_traverseOrder = order++;

			if (pnode->isLeaf()) {
				pnode = *(--stack);
				stackpos -= 1;
			}
			else {
				pnode = pleft;

				if (pright) {
					*(stack++) = pright;
					stackpos += 1;
				}
			}
		}

		return order;
	}

	void bvh::collectNodes(bvhnode* root, std::vector<BVHNode>& nodes, bvhnode* parent)
	{
		static const uint32_t stacksize = 64;
		bvhnode* stackbuf[stacksize] = { nullptr };
		bvhnode** stack = &stackbuf[0];

		// push terminator.
		*stack++ = nullptr;

		int stackpos = 1;

		bvhnode* pnode = root;

		while (pnode != nullptr) {
			bvhnode* pleft = pnode->m_left;
			bvhnode* pright = pnode->m_right;

			BVHNode node;

			node.boxmin = aten::vec4(pnode->getBoundingbox().minPos(), 0);
			node.boxmax = aten::vec4(pnode->getBoundingbox().maxPos(), 0);

			if (pnode->isLeaf()) {
				node.shapeid = transformable::findShapeIdxAsHitable(pnode);

				if (node.shapeid < 0) {
					// Not find shape.
					pnode->setBVHNodeParamInCollectNodes(node);
				}
			}
			else {
				node.left = (pleft ? pleft->m_traverseOrder : -1);
				node.right = (pright ? pright->m_traverseOrder : -1);
			}

			if (parent) {
				AT_ASSERT(node.shapeid < 0);
				node.shapeid = transformable::findShapeIdxAsHitable(parent);
			}

			nodes.push_back(node);

			if (pnode->isLeaf()) {
				pnode = *(--stack);
				stackpos -= 1;
			}
			else {
				pnode = pleft;

				if (pright) {
					*(stack++) = pright;
					stackpos += 1;
				}
			}
		}
	}

	void bvh::dumpCollectedNodes(std::vector<BVHNode>& nodes, const char* path)
	{
		FILE* fp = nullptr;
		fopen_s(&fp, path, "wt");
		if (!fp) {
			AT_ASSERT(false);
			return;
		}

		for (const auto& n : nodes) {
			fprintf(fp, "%d %d %d %d %d\n", (int)n.left, (int)n.right, (int)n.nestid, (int)n.shapeid, (int)n.primid);
		}

		fclose(fp);
	} 
}
