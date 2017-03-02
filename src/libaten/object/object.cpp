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

				auto p = r.org + rec.t * r.dir;

				// NOTE
				// http://d.hatena.ne.jp/Zellij/20131207/p1

				// �d�S���W�n(barycentric coordinates).
				// v0�.
				// p = (1 - a - b)*v0 + a*v1 + b*v2
				rec.p = (1 - res.a - res.b) * v0->pos + res.a * v1->pos + res.b * v2->pos;
				rec.normal = (1 - res.a - res.b) * v0->nml + res.a * v1->nml + res.b * v2->nml;
				auto uv = (1 - res.a - res.b) * v0->uv + res.a * v1->uv + res.b * v2->uv;

				rec.u = uv.x;
				rec.v = uv.y;

				// tangent coordinate.
				rec.du = normalize(getOrthoVector(rec.normal));
				rec.dv = normalize(cross(rec.normal, rec.du));

				// �O�p�`�̖ʐ� = �Q�ӂ̊O�ς̒��� / 2;
				auto e0 = v1->pos - v0->pos;
				auto e1 = v2->pos - v0->pos;
				rec.area = 0.5 * cross(e0, e1).length();

				rec.obj = parent;

				if (parent) {
					rec.mtrl = parent->mtrl;
				}

				isHit = true;
			}
		}

		return isHit;
	}

	void face::build(vertex* v0, vertex* v1, vertex* v2)
	{
		vec3 vmax(
			std::max(v0->pos.x, std::max(v1->pos.x, v2->pos.x)),
			std::max(v0->pos.y, std::max(v1->pos.y, v2->pos.y)),
			std::max(v0->pos.z, std::max(v1->pos.z, v2->pos.z)));

		vec3 vmin(
			std::min(v0->pos.x, std::min(v1->pos.x, v2->pos.x)),
			std::min(v0->pos.y, std::min(v1->pos.y, v2->pos.y)),
			std::min(v0->pos.z, std::min(v1->pos.z, v2->pos.z)));

		bbox.init(vmin, vmax);

		vtx[0] = v0;
		vtx[1] = v1;
		vtx[2] = v2;
	}

	void shape::build()
	{
		bvhnode::build(
			(bvhnode**)&faces[0],
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
			(bvhnode**)&m_obj->m_shapes[0], 
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
