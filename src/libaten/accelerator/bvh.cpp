#include "accelerator/bvh.h"
#include "geometry/transformable.h"
#include "geometry/object.h"

#include <random>
#include <vector>

//#define TEST_NODE_LIST
//#pragma optimize( "", off)

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

	void sortList(hitable**& list, uint32_t num, int axis)
	{
		switch (axis) {
		case 0:
			::qsort(list, num, sizeof(hitable*), compareX);
			break;
		case 1:
			::qsort(list, num, sizeof(hitable*), compareY);
			break;
		default:
			::qsort(list, num, sizeof(hitable*), compareZ);
			break;
		}
	}

	bvhnode::bvhnode(bvhnode* parent, hitable* item, bvh* bvh)
		: m_parent(parent)
	{
		m_children[0] = m_children[1] = m_children[2] = m_children[3] = nullptr;
		m_item = item;

		if (m_item) {
			m_item->setFuncNotifyChanged(std::bind(&bvhnode::itemChanged, this, std::placeholders::_1));
		}

		m_bvh = bvh;
	}

	bool bvhnode::hit(
		const ray& r,
		real t_min, real t_max,
		Intersection& isect) const
	{
#if 0
		if (m_item) {
			return m_item->hit(r, t_min, t_max, isect);
		}
#else
		if (m_childrenNum > 0) {
			bool isHit = false;

			for (int i = 0; i < m_childrenNum; i++) {
				Intersection isectTmp;
				isectTmp.t = AT_MATH_INF;
				auto res = m_children[i]->hit(r, t_min, t_max, isectTmp);
				
				if (res) {
					if (isectTmp.t < isect.t) {
						isect = isectTmp;
						t_max = isect.t;

						isHit = true;
					}
				}
			}

			return isHit;
		}
		else if (m_item) {
			return m_item->hit(r, t_min, t_max, isect);
		}
#endif
		else {
			auto bbox = getBoundingbox();
			auto isHit = bbox.hit(r, t_min, t_max);

			if (isHit) {
				isHit = bvh::onHit(this, r, t_min, t_max, isect);
			}

			return isHit;
		}
	}

	void bvhnode::drawAABB(
		aten::hitable::FuncDrawAABB func,
		const aten::mat4& mtxL2W) const
	{
		if (m_item && m_item->getHasObject()) {
			m_item->drawAABB(func, mtxL2W);
		}
		else {
			auto transofrmedBox = aten::aabb::transform(m_aabb, mtxL2W);

			aten::mat4 mtxScale;
			mtxScale.asScale(transofrmedBox.size());

			aten::mat4 mtxTrans;
			mtxTrans.asTrans(transofrmedBox.minPos());

			aten::mat4 mtx = mtxTrans * mtxScale;

			func(mtx);
		}
	}

	///////////////////////////////////////////////////////

	inline int findLongestAxis(const aabb& bbox)
	{
		const auto& minP = bbox.minPos();
		const auto& maxP = bbox.maxPos();

		auto x = maxP.x - minP.x;
		auto y = maxP.y - minP.y;
		auto z = maxP.z - minP.z;

		auto maxAxis = std::max(std::max(x, y), z);

		auto axis = (maxAxis == x
			? 0
			: maxAxis == y ? 1 : 2);

		return axis;
	}

	void bvh::build(
		hitable** list,
		uint32_t num,
		aabb* bbox/*= nullptr*/)
	{
		int axis = 0;

		if (bbox) {
			axis = findLongestAxis(*bbox);
		}

		sortList(list, num, axis);

		m_root = new bvhnode(nullptr, nullptr, this);
		buildBySAH(m_root, list, num, 0, m_root);
	}

	bool bvh::hit(
		const ray& r,
		real t_min, real t_max,
		Intersection& isect) const
	{
		bool isHit = onHit(m_root, r, t_min, t_max, isect);
		return isHit;
	}

	bool bvh::onHit(
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
						t_max = isect.t;
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
		hitable** list,
		uint32_t num,
		int depth/*= 0*/,
		bvhnode* parent/*= nullptr*/)
	{
		// NOTE
		// http://qiita.com/omochi64/items/9336f57118ba918f82ec

#if 1
		// ����H
		AT_ASSERT(num > 0);

		// �S�̂𕢂�AABB���v�Z.
		root->m_aabb = list[0]->getBoundingbox();
		for (uint32_t i = 1; i < num; i++) {
			auto bbox = list[i]->getBoundingbox();
			root->m_aabb = aabb::merge(root->m_aabb, bbox);
		}

		if (num == 1) {
			// �P�����Ȃ��̂ŁA���ꂾ���ŏI��.
			root->m_left = new bvhnode(parent, list[0], this);

			root->m_left->setBoundingBox(list[0]->getBoundingbox());
			root->m_left->setDepth(depth + 1);

			return;
		}
		else if (num == 2) {
			// �Q�����̂Ƃ��͓K���Ƀ\�[�g���āA�I��.
			auto bbox = list[0]->getBoundingbox();
			bbox = aabb::merge(bbox, list[1]->getBoundingbox());

			int axis = findLongestAxis(bbox);

			sortList(list, num, axis);

			root->m_left = new bvhnode(parent, list[0], this);
			root->m_right = new bvhnode(parent, list[1], this);

			root->m_left->setBoundingBox(list[0]->getBoundingbox());
			root->m_right->setBoundingBox(list[1]->getBoundingbox());

			root->m_left->setDepth(depth + 1);
			root->m_right->setDepth(depth + 1);

			return;
		}

		// Triangle��ray�̃q�b�g�ɂ����鏈�����Ԃ̌��ς���.
		static const real T_tri = 1;  // �K��.

		// AABB��ray�̃q�b�g�ɂ����鏈�����Ԃ̌��ς���.
		static const real T_aabb = 1;  // �K��.

		// �̈敪���������Apolygons ���܂ޗt�m�[�h���\�z����ꍇ���b��� bestCost �ɂ���.
		//auto bestCost = T_tri * num;
		uint32_t bestCost = UINT32_MAX;	// ���E�܂ŕ����������̂ŁA�K���ɑ傫���l�ɂ��Ă���.

		// �����ɍł��ǂ��� (0:x, 1:y, 2:z)
		int bestAxis = -1;

		// �ł��ǂ������ꏊ
		int bestSplitIndex = -1;

		// �m�[�h�S�̂�AABB�̕\�ʐ�
		auto rootSurfaceArea = root->m_aabb.computeSurfaceArea();

		for (int axis = 0; axis < 3; axis++) {
			// �|���S�����X�g���A���ꂼ���AABB�̒��S���W���g���Aaxis �Ń\�[�g����.
			sortList(list, num, axis);

			// AABB�̕\�ʐσ��X�g�Bs1SA[i], s2SA[i] �́A
			// �uS1����i�AS2����(polygons.size()-i)�|���S��������悤�ɕ����v�����Ƃ��̕\�ʐ�
			std::vector<real> s1SurfaceArea(num + 1, AT_MATH_INF);
			std::vector<real> s2SurfaceArea(num + 1, AT_MATH_INF);

			// �������ꂽ2�̗̈�.
			std::vector<hitable*> s1;					// �E��.
			std::vector<hitable*> s2(list, list + num);	// ����.

			// NOTE
			// s2��������o���āAs1�Ɋi�[���邽�߁As2�Ƀ��X�g��S�������.

			aabb s1bbox;

			// �\�ȕ������@�ɂ��āAs1���� AABB �̕\�ʐς��v�Z.
			for (uint32_t i = 0; i <= num; i++) {
				s1SurfaceArea[i] = s1bbox.computeSurfaceArea();

				if (s2.size() > 0) {
					// s2���ŁAaxis �ɂ��čō� (�ŏ��ʒu) �ɂ���|���S����s1�̍ŉE (�ő�ʒu) �Ɉڂ�
					auto p = s2.front();
					s1.push_back(p);
					pop_front(s2);

					// �ڂ����|���S����AABB���}�[�W����s1��AABB�Ƃ���.
					auto bbox = p->getBoundingbox();
					s1bbox = aabb::merge(s1bbox, bbox);
				}
			}

			// �t��s2����AABB�̕\�ʐς��v�Z���ASAH ���v�Z.
			aabb s2bbox;

			for (int i = num; i >= 0; i--) {
				s2SurfaceArea[i] = s2bbox.computeSurfaceArea();

				if (s1.size() > 0 && s2.size() > 0) {
					// SAH-based cost �̌v�Z.
					auto cost =	2 * T_aabb
						+ (s1SurfaceArea[i] * s1.size() + s2SurfaceArea[i] * s2.size()) * T_tri / rootSurfaceArea;

					// �ŗǃR�X�g���X�V���ꂽ��.
					if (cost < bestCost) {
						bestCost = cost;
						bestAxis = axis;
						bestSplitIndex = i;
					}
				}

				if (s1.size() > 0) {
					// s1���ŁAaxis �ɂ��čŉE�ɂ���|���S����s2�̍ō��Ɉڂ�.
					auto p = s1.back();
					
					// �擪�ɑ}��.
					s2.insert(s2.begin(), p); 

					s1.pop_back();

					// �ڂ����|���S����AABB���}�[�W����S2��AABB�Ƃ���.
					auto bbox = p->getBoundingbox();
					s2bbox = aabb::merge(s2bbox, bbox);
				}
			}
		}

		if (bestAxis >= 0) {
			// bestAxis �Ɋ�Â��A���E�ɕ���.
			// bestAxis �Ń\�[�g.
			sortList(list, num, bestAxis);

			// ���E�̎q�m�[�h���쐬.
			root->m_left = new bvhnode(root, nullptr, this);
			root->m_right = new bvhnode(root, nullptr, this);

			root->m_left->setDepth(depth + 1);
			root->m_right->setDepth(depth + 1);

			// ���X�g�𕪊�.
			int leftListNum = bestSplitIndex;
			int rightListNum = num - leftListNum;

			AT_ASSERT(rightListNum > 0);

			// �ċA����
			buildBySAH(root->m_left, list, leftListNum, depth + 1, root->m_left);
			buildBySAH(root->m_right, list + leftListNum, rightListNum, depth + 1, root->m_right);
		}
#else
		struct BuildInfo {
			bvhnode* node{ nullptr };
			hitable** list{ nullptr };
			uint32_t num{ 0 };

			BuildInfo() {}
			BuildInfo(bvhnode* _node, hitable** _list, uint32_t _num)
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
			// �S�̂𕢂�AABB���v�Z.
			info.node->setBoundingBox(info.list[0]->getBoundingbox());
			for (uint32_t i = 1; i < info.num; i++) {
				auto bbox = info.list[i]->getBoundingbox();
				info.node->setBoundingBox(
					aabb::merge(info.node->getBoundingbox(), bbox));
			}

#ifdef ENABLE_BVH_MULTI_TRIANGLES
			if (info.num <= 4) {
				bool canRegisterAsChild = true;

				if (info.num > 1) {
					for (int i = 0; i < info.num; i++) {
						auto internalObj = info.list[i]->getHasObject();
						if (internalObj) {
							canRegisterAsChild = false;
							break;
						}
					}
				}

				if (canRegisterAsChild) {
					// TODO
					int axis = 0;

					sortList(info.list, info.num, axis);

					for (int i = 0; i < info.num; i++) {
						info.node->registerChild(info.list[i], i);
					}

					info.node->setChildrenNum(info.num);

					info = stacks.back();
					stacks.pop_back();
					continue;
		}
			}
#else
			if (info.num == 1) {
				// �P�����Ȃ��̂ŁA���ꂾ���ŏI��.
				info.node->m_left = new bvhnode(info.node, info.list[0]);

				info.node->m_left->setBoundingBox(info.list[0]->getBoundingbox());

				info = stacks.back();
				stacks.pop_back();
				continue;
			}
			else if (info.num == 2) {
				// �Q�����̂Ƃ��͓K���Ƀ\�[�g���āA�I��.

				auto bbox = info.list[0]->getBoundingbox();
				bbox = aabb::merge(bbox, info.list[1]->getBoundingbox());

				int axis = findLongestAxis(bbox);

				sortList(info.list, info.num, axis);

				info.node->m_left = new bvhnode(info.node, info.list[0]);
				info.node->m_right = new bvhnode(info.node, info.list[1]);

				info.node->m_left->setBoundingBox(info.list[0]->getBoundingbox());
				info.node->m_right->setBoundingBox(info.list[1]->getBoundingbox());

				info = stacks.back();
				stacks.pop_back();
				continue;
			}			
#endif

			// Triangle��ray�̃q�b�g�ɂ����鏈�����Ԃ̌��ς���.
			static const real T_tri = 1;  // �K��.

			// AABB��ray�̃q�b�g�ɂ����鏈�����Ԃ̌��ς���.
			static const real T_aabb = 1;  // �K��.

			// �̈敪���������Apolygons ���܂ޗt�m�[�h���\�z����ꍇ���b��� bestCost �ɂ���.
			//auto bestCost = T_tri * num;
			uint32_t bestCost = UINT32_MAX;	// ���E�܂ŕ����������̂ŁA�K���ɑ傫���l�ɂ��Ă���.

			// �����ɍł��ǂ��� (0:x, 1:y, 2:z)
			int bestAxis = -1;

			// �ł��ǂ������ꏊ
			int bestSplitIndex = -1;

			// �m�[�h�S�̂�AABB�̕\�ʐ�
			auto rootSurfaceArea = info.node->getBoundingbox().computeSurfaceArea();

			for (int axis = 0; axis < 3; axis++) {
				// �|���S�����X�g���A���ꂼ���AABB�̒��S���W���g���Aaxis �Ń\�[�g����.
				sortList(info.list, info.num, axis);

				// AABB�̕\�ʐσ��X�g�Bs1SA[i], s2SA[i] �́A
				// �uS1����i�AS2����(polygons.size()-i)�|���S��������悤�ɕ����v�����Ƃ��̕\�ʐ�
				std::vector<real> s1SurfaceArea(info.num + 1, AT_MATH_INF);
				std::vector<real> s2SurfaceArea(info.num + 1, AT_MATH_INF);

				// �������ꂽ2�̗̈�.
				std::vector<hitable*> s1;					// �E��.
				std::vector<hitable*> s2(info.list, info.list + info.num);	// ����.

				// NOTE
				// s2��������o���āAs1�Ɋi�[���邽�߁As2�Ƀ��X�g��S�������.

				aabb s1bbox;

				// �\�ȕ������@�ɂ��āAs1���� AABB �̕\�ʐς��v�Z.
				for (uint32_t i = 0; i <= info.num; i++) {
					s1SurfaceArea[i] = s1bbox.computeSurfaceArea();

					if (s2.size() > 0) {
						// s2���ŁAaxis �ɂ��čō� (�ŏ��ʒu) �ɂ���|���S����s1�̍ŉE (�ő�ʒu) �Ɉڂ�
						auto p = s2.front();
						s1.push_back(p);
						pop_front(s2);

						// �ڂ����|���S����AABB���}�[�W����s1��AABB�Ƃ���.
						auto bbox = p->getBoundingbox();
						s1bbox = aabb::merge(s1bbox, bbox);
					}
				}

				// �t��s2����AABB�̕\�ʐς��v�Z���ASAH ���v�Z.
				aabb s2bbox;

				for (int i = info.num; i >= 0; i--) {
					s2SurfaceArea[i] = s2bbox.computeSurfaceArea();

					if (s1.size() > 0 && s2.size() > 0) {
						// SAH-based cost �̌v�Z.
						auto cost = 2 * T_aabb
							+ (s1SurfaceArea[i] * s1.size() + s2SurfaceArea[i] * s2.size()) * T_tri / rootSurfaceArea;

						// �ŗǃR�X�g���X�V���ꂽ��.
						if (cost < bestCost) {
							bestCost = cost;
							bestAxis = axis;
							bestSplitIndex = i;
						}
					}

					if (s1.size() > 0) {
						// s1���ŁAaxis �ɂ��čŉE�ɂ���|���S����s2�̍ō��Ɉڂ�.
						auto p = s1.back();

						// �擪�ɑ}��.
						s2.insert(s2.begin(), p);

						s1.pop_back();

						// �ڂ����|���S����AABB���}�[�W����S2��AABB�Ƃ���.
						auto bbox = p->getBoundingbox();
						s2bbox = aabb::merge(s2bbox, bbox);
					}
				}
			}

			if (bestAxis >= 0) {
				// bestAxis �Ɋ�Â��A���E�ɕ���.
				// bestAxis �Ń\�[�g.
				sortList(info.list, info.num, bestAxis);

				// ���E�̎q�m�[�h���쐬.
				info.node->m_left = new bvhnode(info.node);
				info.node->m_right = new bvhnode(info.node);

				// ���X�g�𕪊�.
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

	bvhnode* bvh::getNestedNode(bvhnode* node, aten::mat4* mtxL2W/*= nullptr*/)
	{
		bvhnode* ret = nullptr;

		if (node) {
			auto item = node->getItem();
			if (item) {
				auto internalObj = const_cast<hitable*>(item->getHasObject());
				if (internalObj) {
					auto t = transformable::getShapeAsHitable(item);

					if (mtxL2W) {
						aten::mat4 mtxW2L;
						t->getMatrices(*mtxL2W, mtxW2L);
					}

					auto accel = internalObj->getInternalAccelerator();

					// NOTE
					// �{���Ȃ炱�̃L���X�g�͕s�������ABVH�ł��邱�Ƃ͎����Ȃ̂�.
					auto bvh = *(aten::bvh**)&accel;

					ret = bvh->getRoot();
				}
			}

			if (!ret) {
				ret = node;
			}
		}

		return ret;
	}

	void bvh::drawAABB(
		aten::hitable::FuncDrawAABB func,
		const aten::mat4& mtxL2W)
	{
		bvhnode* node = m_root;

		static const uint32_t stacksize = 64;
		const bvhnode* stackbuf[stacksize];

		stackbuf[0] = m_root;
		int stackpos = 1;

		while (stackpos > 0) {
			auto node = stackbuf[stackpos - 1];

			stackpos -= 1;

			node->drawAABB(func, mtxL2W);

			auto left = node->m_left;
			auto right = node->m_right;

			if (left) {
				stackbuf[stackpos++] = left;
			}
			if (right) {
				stackbuf[stackpos++] = right;
			}
		}
	}
}
