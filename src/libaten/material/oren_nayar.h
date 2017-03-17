﻿#pragma once

#include "material/material.h"
#include "texture/texture.h"

namespace aten
{
	class OrenNayar : public material {
	public:
		OrenNayar() {}
		OrenNayar(
			const vec3& albedo,
			real roughness,
			texture* albedoMap = nullptr,
			texture* normalMap = nullptr,
			texture* roughnessMap = nullptr)
			: material(albedo, 1, albedoMap, normalMap), m_roughnessMap(roughnessMap)
		{
			m_roughness = aten::clamp<real>(roughness, 0, 1);
		}

		virtual ~OrenNayar() {}

	public:
		virtual real pdf(
			const vec3& normal, 
			const vec3& wi,
			const vec3& wo,
			real u, real v,
			sampler* sampler) const override final;

		virtual vec3 sampleDirection(
			const vec3& in,
			const vec3& normal,
			real u, real v,
			sampler* sampler) const override final;

		virtual vec3 bsdf(
			const vec3& normal, 
			const vec3& wi,
			const vec3& wo,
			real u, real v) const override final;

		virtual sampling sample(
			const vec3& in,
			const vec3& normal,
			const hitrecord& hitrec,
			sampler* sampler,
			real u, real v) const override final;

		virtual real computeFresnel(
			const vec3& normal,
			const vec3& wi,
			const vec3& wo,
			real outsideIor = 1) const override final
		{
			return real(1);
		}

	private:
		inline real sampleRoughness(real u, real v) const;

	private:
		real m_roughness{ real(0) };
		texture* m_roughnessMap{ nullptr };
	};
}