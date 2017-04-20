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

		const auto& v0 = param.vtx[0];
		const auto& v1 = param.vtx[1];
		const auto& v2 = param.vtx[2];

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

				rec.area = param.area;

				//rec.obj = parent;
				rec.obj = (hitable*)this;

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

		param.bbox.init(vmin, vmax);

		param.vtx[0] = v0;
		param.vtx[1] = v1;
		param.vtx[2] = v2;

		// �O�p�`�̖ʐ� = �Q�ӂ̊O�ς̒��� / 2;
		auto e0 = v1->pos - v0->pos;
		auto e1 = v2->pos - v0->pos;
		param.area = real(0.5) * cross(e0, e1).length();
	}

	vec3 face::getRandomPosOn(sampler* sampler) const
	{
		// 0 <= a + b <= 1
		real a = sampler->nextSample();
		real b = sampler->nextSample();

		real d = a + b;

		if (d > 1) {
			a /= d;
			b /= d;
		}

		const auto& v0 = param.vtx[0];
		const auto& v1 = param.vtx[1];
		const auto& v2 = param.vtx[2];

		// �d�S���W�n(barycentric coordinates).
		// v0�.
		// p = (1 - a - b)*v0 + a*v1 + b*v2
		vec3 p = (1 - a - b) * v0->pos + a * v1->pos + b * v2->pos;

		return std::move(p);
	}

	hitable::SamplingPosNormalPdf face::getSamplePosNormalPdf(sampler* sampler) const
	{
		// 0 <= a + b <= 1
		real a = sampler->nextSample();
		real b = sampler->nextSample();

		real d = a + b;

		if (d > 1) {
			a /= d;
			b /= d;
		}

		const auto& v0 = param.vtx[0];
		const auto& v1 = param.vtx[1];
		const auto& v2 = param.vtx[2];

		// �d�S���W�n(barycentric coordinates).
		// v0�.
		// p = (1 - a - b)*v0 + a*v1 + b*v2
		vec3 p = (1 - a - b) * v0->pos + a * v1->pos + b * v2->pos;
		
		vec3 n = (1 - a - b) * v0->nml + a * v1->nml + b * v2->nml;
		n.normalize();

		// �O�p�`�̖ʐ� = �Q�ӂ̊O�ς̒��� / 2;
		auto e0 = v1->pos - v0->pos;
		auto e1 = v2->pos - v0->pos;
		auto area = real(0.5) * cross(e0, e1).length();

		return std::move(hitable::SamplingPosNormalPdf(p + n * AT_MATH_EPSILON, n, area));
	}

	void shape::build()
	{
		m_node.build(
			(bvhnode**)&faces[0],
			(uint32_t)faces.size());

		m_aabb = m_node.getBoundingbox();

		area = 0;
		for (const auto f : faces) {
			area += f->param.area;
		}
	}

	bool shape::hit(
		const ray& r,
		real t_min, real t_max,
		hitrecord& rec) const
	{
		auto isHit = m_node.hit(r, t_min, t_max, rec);

		if (isHit) {
			rec.mtrl = mtrl;
		}

		return isHit;
	}

	void object::build()
	{
		m_node.build((bvhnode**)&shapes[0], (uint32_t)shapes.size());

		m_area = 0;
		m_triangles = 0;

		for (const auto s : shapes) {
			m_area += s->area;
			m_triangles += (uint32_t)s->faces.size();
		}
	}

	bool object::hit(
		const ray& r,
		const mat4& mtxL2W,
		real t_min, real t_max,
		hitrecord& rec)
	{
		bool isHit = m_node.hit(r, t_min, t_max, rec);
		if (isHit) {
			face* f = (face*)rec.obj;

#if 0
			real originalArea = 0;
			{
				const auto& v0 = f->vtx[0]->pos;
				const auto& v1 = f->vtx[1]->pos;
				const auto& v2 = f->vtx[2]->pos;

				// �O�p�`�̖ʐ� = �Q�ӂ̊O�ς̒��� / 2;
				auto e0 = v1 - v0;
				auto e1 = v2 - v0;
				originalArea = 0.5 * cross(e0, e1).length();
			}

			real scaledArea = 0;
			{
				auto v0 = mtxL2W.apply(f->vtx[0]->pos);
				auto v1 = mtxL2W.apply(f->vtx[1]->pos);
				auto v2 = mtxL2W.apply(f->vtx[2]->pos);

				// �O�p�`�̖ʐ� = �Q�ӂ̊O�ς̒��� / 2;
				auto e0 = v1 - v0;
				auto e1 = v2 - v0;
				scaledArea = 0.5 * cross(e0, e1).length();
			}

			real ratio = scaledArea / originalArea;
#else
			real orignalLen = 0;
			{
				const auto& v0 = f->param.vtx[0]->pos;
				const auto& v1 = f->param.vtx[1]->pos;

				orignalLen = (v1 - v0).length();
			}

			real scaledLen = 0;
			{
				auto v0 = mtxL2W.apply(f->param.vtx[0]->pos);
				auto v1 = mtxL2W.apply(f->param.vtx[1]->pos);

				scaledLen = (v1 - v0).length();
			}

			real ratio = scaledLen / orignalLen;
			ratio = ratio * ratio;
#endif

			rec.area = m_area * ratio;

			// �ŏI�I�ɂ́A����ς�shape��n��.
			rec.obj = f->parent;
		}
		return isHit;
	}

	hitable::SamplingPosNormalPdf object::getSamplePosNormalPdf(const mat4& mtxL2W, sampler* sampler) const
	{
		auto r = sampler->nextSample();
		int shapeidx = (int)(r * (shapes.size() - 1));
		auto shape = shapes[shapeidx];

		r = sampler->nextSample();
		int faceidx = (int)(r * (shape->faces.size() - 1));
		auto f = shape->faces[faceidx];

		real orignalLen = 0;
		{
			const auto& v0 = f->param.vtx[0]->pos;
			const auto& v1 = f->param.vtx[1]->pos;

			orignalLen = (v1 - v0).length();
		}

		real scaledLen = 0;
		{
			auto v0 = mtxL2W.apply(f->param.vtx[0]->pos);
			auto v1 = mtxL2W.apply(f->param.vtx[1]->pos);

			scaledLen = (v1 - v0).length();
		}

		real ratio = scaledLen / orignalLen;
		ratio = ratio * ratio;

		auto area = m_area * ratio;

		auto tmp = f->getSamplePosNormalPdf(sampler);

		hitable::SamplingPosNormalPdf res(
			std::get<0>(tmp),
			std::get<1>(tmp),
			real(1) / area);

		return std::move(res);
	}
}
