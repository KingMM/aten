#pragma once

#include "material/material.h"

namespace AT_NAME
{
    class MicrofacetBlinn : public material {
        friend class MaterialFactory;

    private:
        MicrofacetBlinn(
            const aten::vec3& albedo = aten::vec3(0.5),
            real shininess = real(0.5), 
            real ior = real(1),
            aten::texture* albedoMap = nullptr,
            aten::texture* normalMap = nullptr)
            : material(aten::MaterialType::Blinn, MaterialAttributeMicrofacet, albedo, ior, albedoMap, normalMap)
        {
            m_param.shininess = shininess;
        }

        MicrofacetBlinn(aten::Values& val)
            : material(aten::MaterialType::Blinn, MaterialAttributeMicrofacet, val)
        {
            m_param.shininess = val.get("shininess", m_param.shininess);
        }

        virtual ~MicrofacetBlinn() {}

    public:
        static AT_DEVICE_MTRL_API real pdf(
            const aten::MaterialParameter* param,
            const aten::vec3& normal,
            const aten::vec3& wi,
            const aten::vec3& wo,
            real u, real v);

        static AT_DEVICE_MTRL_API aten::vec3 sampleDirection(
            const aten::MaterialParameter* param,
            const aten::vec3& normal,
            const aten::vec3& wi,
            real u, real v,
            aten::sampler* sampler);

        static AT_DEVICE_MTRL_API aten::vec3 bsdf(
            const aten::MaterialParameter* param,
            const aten::vec3& normal,
            const aten::vec3& wi,
            const aten::vec3& wo,
            real u, real v);

        static AT_DEVICE_MTRL_API aten::vec3 bsdf(
            const aten::MaterialParameter* param,
            const aten::vec3& normal,
            const aten::vec3& wi,
            const aten::vec3& wo,
            real u, real v,
            const aten::vec3& externalAbledo);

        static AT_DEVICE_MTRL_API void sample(
            MaterialSampling* result,
            const aten::MaterialParameter* param,
            const aten::vec3& normal,
            const aten::vec3& wi,
            const aten::vec3& orgnormal,
            aten::sampler* sampler,
            real u, real v,
            bool isLightPath = false);

        static AT_DEVICE_MTRL_API void sample(
            MaterialSampling* result,
            const aten::MaterialParameter* param,
            const aten::vec3& normal,
            const aten::vec3& wi,
            const aten::vec3& orgnormal,
            aten::sampler* sampler,
            real u, real v,
            const aten::vec3& externalAlbedo,
            bool isLightPath = false);

        virtual AT_DEVICE_MTRL_API real pdf(
            const aten::vec3& normal, 
            const aten::vec3& wi,
            const aten::vec3& wo,
            real u, real v) const override final;

        virtual AT_DEVICE_MTRL_API aten::vec3 sampleDirection(
            const aten::ray& ray,
            const aten::vec3& normal, 
            real u, real v,
            aten::sampler* sampler) const override final;

        virtual AT_DEVICE_MTRL_API aten::vec3 bsdf(
            const aten::vec3& normal, 
            const aten::vec3& wi,
            const aten::vec3& wo,
            real u, real v) const override final;

        virtual AT_DEVICE_MTRL_API MaterialSampling sample(
            const aten::ray& ray,
            const aten::vec3& normal,
            const aten::vec3& orgnormal,
            aten::sampler* sampler,
            real u, real v,
            bool isLightPath = false) const override final;

        virtual bool edit(aten::IMaterialParamEditor* editor) override final;

    private:
        static AT_DEVICE_MTRL_API aten::vec3 bsdf(
            const aten::vec3& albedo,
            const real shininess,
            const real ior,
            real& fresnel,
            const aten::vec3& normal,
            const aten::vec3& wi,
            const aten::vec3& wo,
            real u, real v);
    };
}
