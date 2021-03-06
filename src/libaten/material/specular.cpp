#include "material/specular.h"
#include "material/sample_texture.h"

namespace AT_NAME
{
    AT_DEVICE_MTRL_API real specular::pdf(
        const aten::MaterialParameter* param,
        const aten::vec3& normal,
        const aten::vec3& wi,
        const aten::vec3& wo,
        real u, real v)
    {
        return real(1);
    }

    AT_DEVICE_MTRL_API real specular::pdf(
        const aten::vec3& normal, 
        const aten::vec3& wi,
        const aten::vec3& wo,
        real u, real v) const
    {
        return pdf(&m_param, normal, wi, wo, u, v);
    }

    AT_DEVICE_MTRL_API aten::vec3 specular::sampleDirection(
        const aten::MaterialParameter* param,
        const aten::vec3& normal,
        const aten::vec3& wi,
        real u, real v,
        aten::sampler* sampler)
    {
        auto reflect = wi - 2 * dot(normal, wi) * normal;
        reflect = normalize(reflect);

        return std::move(reflect);
    }

    AT_DEVICE_MTRL_API aten::vec3 specular::sampleDirection(
        const aten::ray& ray,
        const aten::vec3& normal,
        real u, real v,
        aten::sampler* sampler) const
    {
        const aten::vec3& in = ray.dir;

        return std::move(sampleDirection(&m_param, normal, in, u, v, sampler));
    }

    AT_DEVICE_MTRL_API aten::vec3 specular::bsdf(
        const aten::MaterialParameter* param,
        const aten::vec3& normal,
        const aten::vec3& wi,
        const aten::vec3& wo,
        real u, real v)
    {
        auto c = dot(normal, wo);

#if 1
        aten::vec3 bsdf = param->baseColor;
#else
        aten::vec3 bsdf;

        // For canceling cosine factor.
        if (c > 0) {
            bsdf = m_color / c;
        }
#endif

        bsdf *= sampleTexture(param->albedoMap, u, v, aten::vec3(real(1)));

        if (param->ior > real(0)) {
            real nc = real(1);        // ^σΜόά¦.
            real nt = param->ior;    // ¨ΜΰΜόά¦.

                                    // SchlickΙζιFresnelΜ½ΛWΜίπg€.
            const real a = nt - nc;
            const real b = nt + nc;
            const real r0 = (a * a) / (b * b);

            const real c = dot(normal, wo);

            // ½ΛϋόΜυͺ½Λ΅Δray.dirΜϋόΙ^ΤB―ΙόάϋόΜυͺ½Λ·ιϋόΙ^Τ.
            const real fresnel = r0 + (1 - r0) * aten::pow(c, 5);

            bsdf *= fresnel;
        }

        return std::move(bsdf);
    }

    AT_DEVICE_MTRL_API aten::vec3 specular::bsdf(
        const aten::MaterialParameter* param,
        const aten::vec3& normal,
        const aten::vec3& wi,
        const aten::vec3& wo,
        real u, real v,
        const aten::vec3& externalAlbedo)
    {
        auto c = dot(normal, wo);

        aten::vec3 bsdf = param->baseColor;
        bsdf *= externalAlbedo;

        return std::move(bsdf);
    }

    AT_DEVICE_MTRL_API aten::vec3 specular::bsdf(
        const aten::vec3& normal, 
        const aten::vec3& wi,
        const aten::vec3& wo,
        real u, real v) const
    {
        return std::move(bsdf(&m_param, normal, wi, wo, u, v));
    }

    AT_DEVICE_MTRL_API MaterialSampling specular::sample(
        const aten::ray& ray,
        const aten::vec3& normal,
        const aten::vec3& orgnormal,
        aten::sampler* sampler,
        real u, real v,
        bool isLightPath/*= false*/) const
    {
        MaterialSampling ret;
        
        sample(
            &ret, 
            &m_param,
            normal,
            ray.dir,
            orgnormal,
            sampler,
            u, v,
            isLightPath);

        return std::move(ret);
    }

    AT_DEVICE_MTRL_API void specular::sample(
        MaterialSampling* result,
        const aten::MaterialParameter* param,
        const aten::vec3& normal,
        const aten::vec3& wi,
        const aten::vec3& orgnormal,
        aten::sampler* sampler,
        real u, real v,
        bool isLightPath/*= false*/)
    {
        result->dir = sampleDirection(param, normal, wi, u, v, sampler);
        result->pdf = pdf(param, normal, wi, result->dir, u, v);
        result->bsdf = bsdf(param, normal, wi, result->dir, u, v);
    }

    AT_DEVICE_MTRL_API void specular::sample(
        MaterialSampling* result,
        const aten::MaterialParameter* param,
        const aten::vec3& normal,
        const aten::vec3& wi,
        const aten::vec3& orgnormal,
        aten::sampler* sampler,
        real u, real v,
        const aten::vec3& externalAlbedo,
        bool isLightPath/*= false*/)
    {
        result->dir = sampleDirection(param, normal, wi, u, v, sampler);
        result->pdf = pdf(param, normal, wi, result->dir, u, v);
        result->bsdf = bsdf(param, normal, wi, result->dir, u, v, externalAlbedo);
    }

    bool specular::edit(aten::IMaterialParamEditor* editor)
    {
        auto b0 = AT_EDIT_MATERIAL_PARAM_RANGE(editor, m_param, ior, real(0.01), real(10));
        auto b1 = AT_EDIT_MATERIAL_PARAM(editor, m_param, baseColor);

        AT_EDIT_MATERIAL_PARAM_TEXTURE(editor, m_param, albedoMap);
        AT_EDIT_MATERIAL_PARAM_TEXTURE(editor, m_param, normalMap);

        return b0 || b1;
    }
}
