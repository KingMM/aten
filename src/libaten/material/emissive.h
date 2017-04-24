#pragma once

#include "material/material.h"

namespace AT_NAME
{
	class emissive : public material {
	public:
		emissive(const aten::vec3& e)
			: material(aten::MaterialType::Emissive, MaterialAttributeEmissive, e)
		{}

		emissive(aten::Values& val)
			: material(aten::MaterialType::Emissive, MaterialAttributeEmissive, val)
		{}

		virtual ~emissive() {}

	public:
		virtual real pdf(
			const aten::vec3& normal,
			const aten::vec3& wi,
			const aten::vec3& wo,
			real u, real v) const override final;

		virtual aten::vec3 sampleDirection(
			const aten::ray& ray,
			const aten::vec3& normal,
			real u, real v,
			aten::sampler* sampler) const override final;

		virtual aten::vec3 bsdf(
			const aten::vec3& normal,
			const aten::vec3& wi,
			const aten::vec3& wo,
			real u, real v) const override final;

		virtual MaterialSampling sample(
			const aten::ray& ray,
			const aten::vec3& normal,
			const aten::hitrecord& hitrec,
			aten::sampler* sampler,
			real u, real v,
			bool isLightPath = false) const override final;

		static real pdf(
			const aten::MaterialParameter& param,
			const aten::vec3& normal,
			const aten::vec3& wi,
			const aten::vec3& wo,
			real u, real v);

		static aten::vec3 sampleDirection(
			const aten::MaterialParameter& param,
			const aten::vec3& normal,
			const aten::vec3& wi,
			real u, real v,
			aten::sampler* sampler);

		static aten::vec3 bsdf(
			const aten::MaterialParameter& param,
			const aten::vec3& normal,
			const aten::vec3& wi,
			const aten::vec3& wo,
			real u, real v);

		static MaterialSampling sample(
			const aten::MaterialParameter& param,
			const aten::vec3& normal,
			const aten::vec3& wi,
			const aten::hitrecord& hitrec,
			aten::sampler* sampler,
			real u, real v,
			bool isLightPath = false);
	};
}
