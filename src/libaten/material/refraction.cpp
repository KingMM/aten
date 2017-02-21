#include "material/refraction.h"
#include "scene/hitable.h"

namespace aten
{
	real refraction::pdf(
		const vec3& normal, 
		const vec3& wi,
		const vec3& wo) const
	{
		AT_ASSERT(false);

		auto ret = real(1);
		return ret;
	}

	vec3 refraction::sampleDirection(
		const vec3& in,
		const vec3& normal,
		sampler* sampler) const
	{
		AT_ASSERT(false);

		auto reflect = in - 2 * dot(normal, in) * normal;
		reflect.normalize();

		return std::move(reflect);
	}

	vec3 refraction::brdf(
		const vec3& normal, 
		const vec3& wi,
		const vec3& wo,
		real u, real v) const
	{
		AT_ASSERT(false);

		return std::move(m_color);
	}

	material::sampling refraction::sample(
		const vec3& in,
		const vec3& normal,
		const hitrecord& hitrec,
		sampler* sampler,
		real u, real v) const
	{
		sampling ret;

		// レイが入射してくる側の物体の屈折率.
		real ni = real(1);	// 真空

		// 物体内部の屈折率.
		real nt = m_nt;

		bool into = (dot(hitrec.normal, normal) > real(0));

		auto reflect = in - 2 * dot(hitrec.normal, in) * hitrec.normal;
		reflect.normalize();

		real cos_i = dot(in, normal);
		real nnt = into ? ni / nt : nt / ni;

		// NOTE
		// cos_t^2 = 1 - sin_t^2
		// sin_t^2 = (ni/nt)^2 * sin_i^2 = (ni/nt)^2 * (1 - cos_i^2)
		// sin_i / sin_t = nt/ni -> sin_t = (ni/nt) * sin_i = (ni/nt) * sqrt(1 - cos_i)
		real cos_t_2 = real(1) - (nnt * nnt) * (real(1) - cos_i * cos_i);

		if (cos_t_2 < real(0)) {
			//AT_PRINTF("Reflection in refraction...\n");

			// 全反射.
			ret.pdf = real(1);

			ret.dir = reflect;

			auto c = dot(normal, ret.dir);
			if (c > real(0)) {
				ret.brdf = m_color / c;
			}

			return std::move(ret);
		}

		vec3 n = into ? hitrec.normal : -hitrec.normal;
#if 0
		vec3 refract = in * nnt - hitrec.normal * (into ? 1.0 : -1.0) * (cos_i * nnt + sqrt(cos_t_2));
#else
		// NOTE
		// https://www.vcl.jp/~kanazawa/raytracing/?page_id=478

		auto invnnt = 1 / nnt;
		vec3 refract = nnt * (in - (aten::sqrt(invnnt * invnnt - (1 - cos_i * cos_i)) - (-cos_i)) * normal);
#endif
		refract.normalize();

		const auto r0 = ((nt - ni) * (nt - ni)) / ((nt + ni) * (nt + ni));

		const auto c = 1 - (into ? -cos_i : dot(refract, -normal));

		// 反射方向の光が反射してray.dirの方向に運ぶ割合。同時に屈折方向の光が反射する方向に運ぶ割合.
		auto fresnel = r0 + (1 - r0) * aten::pow(c, 5);

		// レイの運ぶ放射輝度は屈折率の異なる物体間を移動するとき、屈折率の比の二乗の分だけ変化する.
		real nn = nnt * nnt;

		auto Re = fresnel;
		auto Tr = (1 - Re) * nn;

		auto r = 0.5;
		if (sampler) {
			r = sampler->nextSample();
		}

#if 1
		auto prob = 0.25 + 0.5 * Re;
		if (r < prob) {
			// 反射.
			ret.dir = reflect;
			
			auto denom = dot(normal, reflect);
			ret.brdf = Re * m_color / denom;
			ret.brdf /= prob;
		}
		else {
			// 屈折.
			ret.dir = refract;

			auto denom = dot(normal, refract);
			ret.brdf = Tr * m_color / denom;
			ret.brdf /= (1 - prob);
		}
#else
		ret.dir = refract;

		auto denom = dot(normal, refract);
		ret.brdf = Tr * m_color / denom;
#endif

		ret.pdf = 1;

		return std::move(ret);
	}
}
