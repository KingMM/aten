#include <vector>
#include "tiny_obj_loader.h"
#include "ObjLoader.h"
#include "AssetManager.h"
#include "utility.h"

namespace aten
{
	static std::string g_base;

	void ObjLoader::setBasePath(const std::string& base)
	{
		g_base = removeTailPathSeparator(base);
	}

	object* ObjLoader::load(const std::string& path)
	{
		std::string pathname;
		std::string extname;
		std::string filename;

		getStringsFromPath(
			path,
			pathname,
			extname,
			filename);

		std::string fullpath = path;
		if (!g_base.empty()) {
			fullpath = g_base + "/" + fullpath;
		}

		auto obj = load(filename, fullpath);

		return obj;
	}

	object* ObjLoader::load(const std::string& tag, const std::string& path)
	{
		object* obj = AssetManager::getObj(tag);
		if (obj) {
			AT_PRINTF("There is same tag object. [%s]\n", tag.c_str());
			return obj;
		}

		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> mtrls;
		std::string err;

		// TODO
		// mtl_basepath

		auto flags = tinyobj::triangulation | tinyobj::calculate_normals;

		auto result = tinyobj::LoadObj(
			shapes, mtrls,
			err,
			path.c_str(), nullptr,
			flags);
		AT_VRETURN(result, nullptr);

		obj = new object();

		vec3 shapemin = make_float3(AT_MATH_INF);
		vec3 shapemax = make_float3(-AT_MATH_INF);

		for (int p = 0; p < shapes.size(); p++) {
			const auto& shape = shapes[p];
			aten::shape* dstshape = new aten::shape();

			auto idxnum = shape.mesh.indices.size();
			auto vtxnum = shape.mesh.positions.size();

			auto curVtxPos = VertexManager::getVertexNum();

			vec3 pmin = make_float3(AT_MATH_INF);
			vec3 pmax = make_float3(-AT_MATH_INF);

			for (uint32_t i = 0; i < vtxnum; i += 3) {
				vertex v;

				vertex vtx;

				vtx.pos.x = shape.mesh.positions[i + 0];
				vtx.pos.y = shape.mesh.positions[i + 1];
				vtx.pos.z = shape.mesh.positions[i + 2];

				vtx.nml.x = shape.mesh.normals[i + 0];
				vtx.nml.y = shape.mesh.normals[i + 1];
				vtx.nml.z = shape.mesh.normals[i + 2];

				VertexManager::addVertex(vtx);

				pmin = make_float3(
					std::min(pmin.x, v.pos.x),
					std::min(pmin.y, v.pos.y),
					std::min(pmin.z, v.pos.z));
				pmax = make_float3(
					std::max(pmax.x, v.pos.x),
					std::max(pmax.y, v.pos.y),
					std::max(pmax.z, v.pos.z));
			}

			shapemin = make_float3(
				std::min(shapemin.x, pmin.x),
				std::min(shapemin.y, pmin.y),
				std::min(shapemin.z, pmin.z));
			shapemax = make_float3(
				std::max(shapemax.x, pmax.x),
				std::max(shapemax.y, pmax.y),
				std::max(shapemax.z, pmax.z));

			for (uint32_t i = 0; i < idxnum; i += 3) {
				face* f = new face();

				f->param.idx[0] = shape.mesh.indices[i + 0] + curVtxPos;
				f->param.idx[1] = shape.mesh.indices[i + 1] + curVtxPos;
				f->param.idx[2] = shape.mesh.indices[i + 2] + curVtxPos;

				f->build();

				f->parent = dstshape;

				dstshape->faces.push_back(f);
			}

			// Assign material.
			auto mtrlidx = -1;
			if (shape.mesh.material_ids.size() > 0) {
				mtrlidx = shape.mesh.material_ids[0];
			}
			if (mtrlidx >= 0) {
				const auto mtrl = mtrls[mtrlidx];
				dstshape->param.mtrl.ptr = AssetManager::getMtrl(mtrl.name);
			}
			if (!dstshape->param.mtrl.ptr){
				// dummy....
				AT_ASSERT(false);
				dstshape->param.mtrl.ptr = new lambert(vec3());
			}

			vtxnum = shape.mesh.texcoords.size();

			for (uint32_t i = 0; i < vtxnum; i += 2) {
				uint32_t vpos = i / 2;

				auto& vtx = VertexManager::getVertex(vpos);

				vtx.uv.x = shape.mesh.texcoords[i + 0];
				vtx.uv.y = shape.mesh.texcoords[i + 1];
			}

			obj->shapes.push_back(dstshape);
		}

		obj->bbox.init(shapemin, shapemax);

		AssetManager::registerObj(tag, obj);

		return obj;
	}
}
