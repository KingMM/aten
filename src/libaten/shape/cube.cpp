#include "shape/cube.h"

namespace AT_NAME
{
	cube::cube(const aten::vec3& center, real w, real h, real d, material* mtrl)
		: transformable(), m_param(center, aten::make_float3(w, h, d), mtrl)
	{
		m_aabb.init(
			center - m_param.size * 0.5,
			center + m_param.size * 0.5);
	}

	cube::Face cube::findFace(const aten::vec3& d)
	{
		auto x = aten::abs(d.x);
		auto y = aten::abs(d.y);
		auto z = aten::abs(d.z);

		if (x > y && x > z) {
			// X軸に平行な面.
			if (d.x > 0) {
				return Face::POS_X;
			}
			else {
				return Face::NEG_X;
			}
		}
		else if (y > x && y > z) {
			// Y軸に平行な面.
			if (d.y > 0) {
				return Face::POS_Y;
			}
			else {
				return Face::NEG_Y;
			}
		}
		else {
			// Z軸に平行な面.
			if (d.z > 0) {
				return Face::POS_Z;
			}
			else {
				return Face::NEG_Z;
			}
		}
	}

	bool cube::hit(
		const aten::ray& r,
		real t_min, real t_max,
		aten::hitrecord& rec) const
	{
		real t = 0;
		bool isHit = m_aabb.hit(r, t_min, t_max, &t);

		if (isHit) {
			rec.p = r.org + t * r.dir;

			rec.t = t;

			// どの面にヒットしたか探す.
			{
				auto dir = normalize(rec.p - m_param.center);
				auto face = findFace(dir);

				switch (face) {
				case POS_X:
					rec.normal = aten::make_float3(1, 0, 0);
					break;
				case NEG_X:
					rec.normal = aten::make_float3(-1, 0, 0);
					break;
				case POS_Y:
					rec.normal = aten::make_float3(0, 1, 0);
					break;
				case NEG_Y:
					rec.normal = aten::make_float3(0, -1, 0);
					break;
				case POS_Z:
					rec.normal = aten::make_float3(0, 0, 1);
					break;
				case NEG_Z:
					rec.normal = aten::make_float3(0, 0, -1);
					break;
				}
			}

			rec.obj = (hitable*)this;
			rec.mtrl = (material*)m_param.mtrl.ptr;

			// tangent coordinate.
			rec.du = normalize(getOrthoVector(rec.normal));
			rec.dv = normalize(cross(rec.normal, rec.du));

			rec.area = m_aabb.computeSurfaceArea();
		}

		return isHit;
	}

	void cube::evalHitResult(const aten::ray& r, aten::hitrecord& rec) const
	{
		evalHitResult(r, aten::mat4(), rec);
	}

	void cube::evalHitResult(const aten::ray& r, const aten::mat4& mtxL2W, aten::hitrecord& rec) const
	{
		// TODO
		AT_ASSERT(false);	// Not support.

		Face face = (Face)rec.param.face;

		switch (face) {
		case POS_X:
			rec.normal = aten::make_float3(1, 0, 0);
			break;
		case NEG_X:
			rec.normal = aten::make_float3(-1, 0, 0);
			break;
		case POS_Y:
			rec.normal = aten::make_float3(0, 1, 0);
			break;
		case NEG_Y:
			rec.normal = aten::make_float3(0, -1, 0);
			break;
		case POS_Z:
			rec.normal = aten::make_float3(0, 0, 1);
			break;
		case NEG_Z:
			rec.normal = aten::make_float3(0, 0, -1);
			break;
		}

		// tangent coordinate.
		rec.du = normalize(getOrthoVector(rec.normal));
		rec.dv = normalize(cross(rec.normal, rec.du));

		rec.area = m_aabb.computeSurfaceArea();
	}

	aten::vec3 cube::getRandomPosOn(aten::sampler* sampler) const
	{
		aten::vec3 pos;
		onGetRandomPosOn(pos, sampler);

		return std::move(pos);
	}

	cube::Face cube::onGetRandomPosOn(aten::vec3& pos, aten::sampler* sampler) const
	{
		auto r1 = sampler->nextSample();

		static const real d = real(1.0) / real(6.0);
		real th = 0;

		Face face = Face::POS_X;

		for (int i = 0; i < 6; i++) {
			if (th <= r1 && r1 < th + d) {
				face = (Face)i;
				break;
			}

			th += d;
		}

		auto r2 = sampler->nextSample();
		auto r3 = sampler->nextSample();

		auto c = m_param.center;
		auto s = m_param.size * 0.5;

		aten::vec3 leftbottom;
		aten::vec3 lefttop;
		aten::vec3 rightbottom;

		switch (face) {
		case POS_X:
			leftbottom = c + aten::make_float3(s.x, -s.y, s.z);
			lefttop = leftbottom + aten::make_float3(0, 2 * s.y, 0);
			rightbottom = leftbottom + aten::make_float3(0, 0, -2 * s.z);
			break;
		case NEG_X:
			leftbottom = c + aten::make_float3(-s.x, -s.y, s.z);
			lefttop = leftbottom + aten::make_float3(0, 2 * s.y, 0);
			rightbottom = leftbottom + aten::make_float3(0, 0, -2 * s.z);
			break;
		case POS_Y:
			leftbottom = c + aten::make_float3(-s.x, s.y, s.z);
			lefttop = leftbottom + aten::make_float3(0, 0, -2 * s.z);
			rightbottom = leftbottom + aten::make_float3(2 * s.x, 0, 0);
			break;
		case NEG_Y:
			leftbottom = c + aten::make_float3(-s.x, -s.y, s.z);
			lefttop = leftbottom + aten::make_float3(0, 0, -2 * s.z);
			rightbottom = leftbottom + aten::make_float3(2 * s.x, 0, 0);
			break;
		case POS_Z:
			leftbottom = c + aten::make_float3(s.x, -s.y, s.z);
			lefttop = leftbottom + aten::make_float3(0, 2 * s.y, 0);
			rightbottom = leftbottom + aten::make_float3(2 * s.x, 0, 0);
			break;
		case NEG_Z:
			leftbottom = c + aten::make_float3(s.x, -s.y, -s.z);
			lefttop = leftbottom + aten::make_float3(0, 2 * s.y, 0);
			rightbottom = leftbottom + aten::make_float3(2 * s.x, 0, 0);
			break;
		}

		aten::vec3 x = rightbottom - leftbottom;
		aten::vec3 y = lefttop - leftbottom;

		// For debug.
		aten::vec3 nx = normalize(x);
		aten::vec3 ny = normalize(y);

		pos = leftbottom + r2 * x + r3 * y;

		return face;
	}

	aten::hitable::SamplingPosNormalPdf cube::getSamplePosNormalPdf(aten::sampler* sampler) const
	{
		return getSamplePosNormalPdf(aten::mat4::Identity, sampler);
	}

	aten::hitable::SamplingPosNormalPdf cube::getSamplePosNormalPdf(
		const aten::mat4& mtxL2W,
		aten::sampler* sampler) const
	{
		aten::vec3 pos;
		auto face = onGetRandomPosOn(pos, sampler);

		aten::vec3 nml;
		switch (face) {
		case POS_X:
			nml = aten::make_float3(1, 0, 0);
			break;
		case NEG_X:
			nml = aten::make_float3(-1, 0, 0);
			break;
		case POS_Y:
			nml = aten::make_float3(0, 1, 0);
			break;
		case NEG_Y:
			nml = aten::make_float3(0, -1, 0);
			break;
		case POS_Z:
			nml = aten::make_float3(0, 0, 1);
			break;
		case NEG_Z:
			nml = aten::make_float3(0, 0, -1);
			break;
		}

		// TODO
		AT_ASSERT(false);
		auto area = 1;

		return std::move(hitable::SamplingPosNormalPdf(pos + nml * AT_MATH_EPSILON, nml, real(1) / area));
	}
}
