#include "object/object.h"
#include "math/intersect.h"

namespace aten
{
	bool face::hit(
		const ray& r,
		real t_min, real t_max,
		hitrecord& rec) const
	{
		bool isHit = false;

#if 0
		const auto v0 = vtx[idx[0]];
		const auto v1 = vtx[idx[1]];
		const auto v2 = vtx[idx[2]];
#else
		const auto v0 = vtx[0];
		const auto v1 = vtx[1];
		const auto v2 = vtx[2];
#endif

		const auto res = intersertTriangle(r, v0->pos, v1->pos, v2->pos);

		if (res.isIntersect) {
			if (res.t < rec.t) {
				rec.t = res.t;

				rec.normal = v0->nml;
				rec.p = r.org + rec.t * r.dir;

				isHit = true;
			}
		}

		return isHit;
	}

	void face::build(vertex* v0, vertex* v1, vertex* v2)
	{
		vec3 vmax(
			max(v0->pos.x, max(v1->pos.x, v2->pos.x)),
			max(v0->pos.y, max(v1->pos.y, v2->pos.y)),
			max(v0->pos.z, max(v1->pos.z, v2->pos.z)));

		vec3 vmin(
			min(v0->pos.x, min(v1->pos.x, v2->pos.x)),
			min(v0->pos.y, min(v1->pos.y, v2->pos.y)),
			min(v0->pos.z, min(v1->pos.z, v2->pos.z)));

		bbox.init(vmin, vmax);

		vtx[0] = v0;
		vtx[1] = v1;
		vtx[2] = v2;
	}

	void shape::build()
	{
		bvhnode::build(
			(hitable**)&faces[0],
			faces.size());
	}

	bool shape::hit(
		const ray& r,
		real t_min, real t_max,
		hitrecord& rec) const
	{
		auto isHit = bvhnode::hit(r, t_min, t_max, rec);

		if (isHit) {
			rec.obj = (hitable*)this;
			rec.mtrl = mtrl;
		}

		return isHit;
	}

	objinstance::objinstance(object* obj)
	{
		m_obj = obj;

		build(
			(hitable**)&m_obj->m_shapes[0], 
			m_obj->m_shapes.size());
	}

	bool objinstance::hit(
		const ray& r,
		real t_min, real t_max,
		hitrecord& rec) const
	{
		// TODO
		// Compute ray to objinstance coordinate.

		auto isHit = bvhnode::hit(r, t_min, t_max, rec);
		return isHit;
	}

	aabb objinstance::getBoundingbox() const
	{
		// TODO
		// Compute by transform matrix;

		auto box = m_obj->m_aabb;

		return std::move(box);
	}
}
