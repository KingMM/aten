#include "material/lambert_refraction.h"

#pragma optimize( "", off)

namespace AT_NAME
{
        AT_DEVICE_MTRL_API real LambertRefraction::pdf(
            const aten::vec3& normal,
            const aten::vec3& wo)
        {
            auto c = dot(normal, wo);
            c = aten::abs(c);

            auto ret = c / AT_MATH_PI;

            return ret;
        }

        AT_DEVICE_MTRL_API aten::vec3 LambertRefraction::sampleDirection(
            const aten::MaterialParameter* param,
            const aten::vec3& normal,
            const aten::vec3& wi,
            real u, real v,
            aten::sampler* sampler)
        {
            const auto& in = -wi;

            bool into = (dot(in, normal) > real(0));
            const auto& nml = into ? normal : -normal;

            // normal�̕�������Ƃ������K�������(w, u, v)�����.
            // ���̊��ɑ΂��锼�����Ŏ��̃��C���΂�.
            auto n = nml;
            auto t = aten::getOrthoVector(n);
            auto b = cross(n, t);

            // �R�T�C�������g�����d�_�I�T���v�����O.
            const real r1 = 2 * AT_MATH_PI * sampler->nextSample();
            const real r2 = sampler->nextSample();
            const real r2s = sqrt(r2);

            const real x = aten::cos(r1) * r2s;
            const real y = aten::sin(r1) * r2s;
            const real z = aten::sqrt(real(1) - r2);

            aten::vec3 dir = normalize((t * x + b * y + n * z));

            dir = -dir;

            return std::move(dir);
        }

        AT_DEVICE_MTRL_API aten::vec3 LambertRefraction::bsdf(
            const aten::MaterialParameter* param,
            real u, real v)
        {
            aten::vec3 albedo = param->baseColor;
            albedo *= sampleTexture(
                param->albedoMap,
                u, v,
                real(1));

            aten::vec3 ret = albedo / AT_MATH_PI;
            return ret;
        }

        AT_DEVICE_MTRL_API aten::vec3 LambertRefraction::bsdf(
            const aten::MaterialParameter* param,
            const aten::vec3& externalAlbedo)
        {
            aten::vec3 albedo = param->baseColor;
            albedo *= externalAlbedo;

            aten::vec3 ret = albedo / AT_MATH_PI;
            return ret;
        }

        AT_DEVICE_MTRL_API void LambertRefraction::sample(
            MaterialSampling* result,
            const aten::MaterialParameter* param,
            const aten::vec3& normal,
            const aten::vec3& wi,
            const aten::vec3& orgnormal,
            aten::sampler* sampler,
            real u, real v,
            bool isLightPath/*= false*/)
        {
            MaterialSampling ret;

            result->dir = sampleDirection(param, normal, wi, u, v, sampler);
            result->pdf = pdf(normal, result->dir);
            result->bsdf = bsdf(param, u, v);
        }
}
