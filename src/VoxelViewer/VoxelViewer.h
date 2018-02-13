#pragma once

#include "aten.h"

class VoxelViewer {
public:
	VoxelViewer() {}
	~VoxelViewer() {}

public:
	bool init(
		int width, int height,
		const char* pathVS,
		const char* pathFS);

	void draw(
		const aten::camera* cam,
		const std::vector<std::vector<aten::ThreadedSbvhNode>>& nodes,
		const std::vector<aten::BvhVoxel>& voxels,
		bool isWireframe,
		int drawVoxelIdx);

private:
	aten::shader m_shader;
	aten::GeomVertexBuffer m_vb;
	aten::GeomIndexBuffer m_ib;

	aten::GeomIndexBuffer m_ibForWireframe;

	int m_width{ 0 };
	int m_height{ 0 };
};