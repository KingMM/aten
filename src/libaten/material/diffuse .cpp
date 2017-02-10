#include "material/diffuse.h"

namespace aten {
	real diffuse::pdf(const vec3& normal, const vec3& dir) const
	{
		auto c = dot(normal, dir);
		AT_ASSERT(c > AT_MATH_EPSILON);

		auto ret = c / AT_MATH_PI;
		
		return ret;
	}

	vec3 diffuse::sampleDirection(
		const vec3& in,
		const vec3& normal, 
		sampler* sampler) const
	{
		// normal�̕�������Ƃ������K�������(w, u, v)�����.
		// ���̊��ɑ΂��锼�����Ŏ��̃��C���΂�.
		vec3 w, u, v;

		w = normal;

		// w�ƕ��s�ɂȂ�Ȃ��悤�ɂ���.
		if (fabs(w.x) > 0.1) {
			u = normalize(cross(vec3(0.0, 1.0, 0.0), w));
		}
		else {
			u = normalize(cross(vec3(1.0, 0.0, 0.0), w));
		}
		v = cross(w, u);

		// �R�T�C�������g�����d�_�I�T���v�����O.
		const real r1 = 2 * AT_MATH_PI * sampler->nextSample();
		const real r2 = sampler->nextSample();
		const real r2s = sqrt(r2);

		const real x = aten::cos(r1) * r2s;
		const real y = aten::sin(r1) * r2s;
		const real z = aten::sqrt(CONST_REAL(1.0) - r2);

		vec3 dir = normalize((u * x + v * y + w * z));

		return std::move(dir);
	}

	vec3 diffuse::brdf(const vec3& normal, const vec3& dir) const
	{
		vec3 ret = m_color / AT_MATH_PI;
		return ret;
	}
}