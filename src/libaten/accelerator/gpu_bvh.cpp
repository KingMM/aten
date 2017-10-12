#include "accelerator/gpu_bvh.h"
#include "accelerator/bvh.h"
#include "shape/tranformable.h"
#include "object/object.h"

#include <random>
#include <vector>

#pragma optimize( "", off)

namespace aten {
	static std::vector<std::vector<GPUBvhNode>> s_listGpuBvhNode;
	static std::vector<aten::mat4> s_mtxs;

	void GPUBvh::build(
		hitable** list,
		uint32_t num)
	{
		m_bvh.build(list, num);

		// Gather local-world matrix.
		{
			auto& shapes = const_cast<std::vector<transformable*>&>(transformable::getShapes());

			for (auto s : shapes) {
				auto& param = const_cast<aten::ShapeParameter&>(s->getParam());

				if (param.type == ShapeType::Instance) {
					aten::mat4 mtxL2W, mtxW2L;
					s->getMatrices(mtxL2W, mtxW2L);

					if (!mtxL2W.isIdentity()) {
						param.mtxid = (int)(s_mtxs.size() / 2);

						s_mtxs.push_back(mtxL2W);
						s_mtxs.push_back(mtxW2L);
					}
				}
			}
		}

		std::vector<std::vector<bvhnode*>> listBvhNode;
		
		// Register to linear list to traverse bvhnode easily.
		auto root = m_bvh.getRoot();
		listBvhNode.push_back(std::vector<bvhnode*>());
		registerBvhNodeToLinearList(root, listBvhNode[0]);

		auto& bvhList = bvh::getBvhList();

		for (int i = 0; i < bvhList.size(); i++) {
			auto bvh = bvhList[i];
			root = bvh->getRoot();
			
			listBvhNode.push_back(std::vector<bvhnode*>());

			registerBvhNodeToLinearList(root, listBvhNode[i + 1]);
		}

		// Register bvh node for gpu.
		for (int i = 0; i < listBvhNode.size(); i++) {
			s_listGpuBvhNode.push_back(std::vector<GPUBvhNode>());
			registerGpuBvhNode(listBvhNode[i], s_listGpuBvhNode[i]);
		}

		// Set traverse order for linear bvh.
		for (int i = 0; i < listBvhNode.size(); i++) {
			setOrderForLinearBVH(listBvhNode[i], s_listGpuBvhNode[i]);
		}

		dump(s_listGpuBvhNode[0], "node.txt");
	}

	void GPUBvh::dump(std::vector<GPUBvhNode>& nodes, const char* path)
	{
		FILE* fp = nullptr;
		fopen_s(&fp, path, "wt");
		if (!fp) {
			AT_ASSERT(false);
			return;
		}

		for (const auto& n : nodes) {
			fprintf(fp, "%d %d %d %d (%.3f, %.3f, %.3f) (%.3f, %.3f, %.3f)\n", 
				(int)n.hit, (int)n.miss, (int)n.shapeid, (int)n.primid,
				n.boxmin.x, n.boxmin.y, n.boxmin.z,
				n.boxmax.x, n.boxmax.y, n.boxmax.z);
		}

		fclose(fp);
	}

	static inline bvhnode* getInternalNode(bvhnode* node)
	{
		bvhnode* ret = nullptr;

		if (node) {
			auto item = node->getItem();
			if (item) {
				auto internalObj = const_cast<hitable*>(item->getHasObject());
				if (internalObj) {
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

	void GPUBvh::registerBvhNodeToLinearList(
		bvhnode* root,
		std::vector<bvhnode*>& listBvhNode)
	{
		static const uint32_t stacksize = 64;
		bvhnode* stackbuf[stacksize] = { nullptr };
		bvhnode** stack = &stackbuf[0];

		// push terminator.
		*stack++ = nullptr;

		int stackpos = 1;

		bvhnode* pnode = root;

		while (pnode != nullptr) {
			pnode = getInternalNode(pnode);

			bvhnode* pleft = pnode->getLeft();
			bvhnode* pright = pnode->getRight();

			pnode->setTraversalOrder((int)listBvhNode.size());
			listBvhNode.push_back(pnode);

			if (pnode->isLeaf()) {
				auto item = pnode->getItem();
				auto externalId = item->getExtraId() + 1;	// Index 0 is for main tree.
				pnode->setExternalId(externalId);

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

	void GPUBvh::registerGpuBvhNode(
		std::vector<bvhnode*>& listBvhNode,
		std::vector<GPUBvhNode>& listGpuBvhNode)
	{
		listGpuBvhNode.reserve(listBvhNode.size());

		for (const auto& node : listBvhNode) {
			GPUBvhNode gpunode;

			// NOTE
			// Differ set hit/miss index.

			auto bbox = node->getBoundingbox();

			auto parent = node->getParent();
			gpunode.parent = (float)(parent ? parent->getTraversalOrder() : -1);

			gpunode.padding0 = -1.0f;

			if (node->isLeaf()) {
				hitable* item = node->getItem();
				auto internalObj = item->getHasObject();

				if (internalObj) {
					// This node is instance, so find index as internal object in instance.
					item = const_cast<hitable*>(internalObj);

#if 0
					int idx = transformable::findShapeIdxAsHitable(item);
					auto instance = transformable::getShape(idx);
					AT_ASSERT(instance);

					aten::mat4 mtxL2W;
					aten::mat4 mtxW2L;
					instance->getMatrices(mtxL2W, mtxW2L);

					// Apply local-world matrix.
					bbox = aten::aabb::transform(bbox, mtxL2W);
#else
					bbox = item->getTransformedBoundingBox();
#endif
				}

				gpunode.shapeid = (float)transformable::findShapeIdxAsHitable(item);

				if (gpunode.shapeid < 0) {
					auto instanceParent = item->getInstanceParent();

					if (instanceParent) {
						bbox = instanceParent->getTransformedBoundingBox();
						gpunode.shapeid = (float)transformable::findShapeIdxAsHitable(instanceParent);
					}
					else {
						// Not shape, so this node is primitive.
						gpunode.primid = (float)face::findIdx(item);
					}
				}

				gpunode.meshid = (float)item->meshid();

				gpunode.exid = (float)node->getExternalId();
			}

			gpunode.boxmax = bbox.maxPos();
			gpunode.boxmin = bbox.minPos();

			listGpuBvhNode.push_back(gpunode);
		}
	}

	void GPUBvh::setOrderForLinearBVH(
		std::vector<bvhnode*>& listBvhNode,
		std::vector<GPUBvhNode>& listGpuBvhNode)
	{
		auto num = listGpuBvhNode.size();

		for (int n = 0; n < num; n++) {
			auto node = listBvhNode[n];
			auto& gpunode = listGpuBvhNode[n];

			bvhnode* next = nullptr;
			if (n + 1 < num) {
				next = listBvhNode[n + 1];
			}

			if (node->isLeaf()) {
				// Hit/Miss.
				// Always the next node in the array.
				if (next) {
					gpunode.hit = (float)next->getTraversalOrder();
					gpunode.miss = (float)next->getTraversalOrder();
				}
				else {
					gpunode.hit = -1;
					gpunode.miss = -1;
				}
			}
			else {
				// Hit.
				// Always the next node in the array.
				if (next) {
					gpunode.hit = (float)next->getTraversalOrder();
				}
				else {
					gpunode.hit = -1;
				}

				// Miss.

				// Search the parent.
				bvhnode* parent = (gpunode.parent >= 0.0f
					? listBvhNode[(int)gpunode.parent]
					: nullptr);

				if (parent) {
					bvhnode* left = getInternalNode(parent->getLeft());
					bvhnode* right = getInternalNode(parent->getRight());

					bool isLeft = (left == node);

					if (isLeft) {
						// Internal, left: sibling node.
						auto sibling = right;
						isLeft = (sibling != nullptr);

						if (isLeft) {
							gpunode.miss = (float)sibling->getTraversalOrder();
						}
					}

					bvhnode* curParent = parent;

					if (!isLeft) {
						// Internal, right: parent's sibling node (until it exists) .
						for (;;) {
							// Search the grand parent.
							const auto& parentNode = listGpuBvhNode[curParent->getTraversalOrder()];
							bvhnode* grandParent = (parentNode.parent >= 0
								? listBvhNode[(int)parentNode.parent]
								: nullptr);

							if (grandParent) {
								bvhnode* _left = getInternalNode(grandParent->getLeft());;
								bvhnode* _right = getInternalNode(grandParent->getRight());;

								auto sibling = _right;
								if (sibling) {
									if (sibling != curParent) {
										gpunode.miss = (float)sibling->getTraversalOrder();
										break;
									}
								}
							}
							else {
								gpunode.miss = -1;
								break;
							}

							curParent = grandParent;
						}
					}
				}
				else {
					gpunode.miss = -1;
				}
			}
		}
	}

	bool GPUBvh::hit(
		const ray& r,
		real t_min, real t_max,
		Intersection& isect) const
	{
		return hit(0, s_listGpuBvhNode, r, t_min, t_max, isect);
	}

	bool GPUBvh::hit(
		int exid,
		std::vector<std::vector<GPUBvhNode>>& listGpuBvhNode,
		const ray& r,
		real t_min, real t_max,
		Intersection& isect) const
	{
		static const uint32_t stacksize = 64;

		auto& shapes = transformable::getShapes();
		auto& prims = face::faces();

		real hitt = AT_MATH_INF;

		int nodeid = 0;

		for (;;) {
			GPUBvhNode* node = nullptr;

			if (nodeid >= 0) {
				node = &listGpuBvhNode[exid][nodeid];
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
						const auto& mtxW2L = s_mtxs[mtxid * 2 + 1];

						transformedRay = mtxW2L.applyRay(r);
					}
					else {
						transformedRay = r;
					}

					isHit = hit(
						node->exid,
						listGpuBvhNode,
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
}
