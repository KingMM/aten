#include "kernel/material.cuh"

#include "kernel/idatendefs.cuh"

AT_CUDA_INLINE __device__ void sampleMaterial(
	AT_NAME::MaterialSampling* result,
	const Context* ctxt,
	const aten::MaterialParameter* mtrl,
	const aten::vec3& normal,
	const aten::vec3& wi,
	const aten::vec3& orgnormal,
	aten::sampler* sampler,
	float u, float v)
{
	switch (mtrl->type) {
	case aten::MaterialType::Emissive:
		AT_NAME::emissive::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, false);
		break;
	case aten::MaterialType::Lambert:
		AT_NAME::lambert::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, false);
		break;
	case aten::MaterialType::OrneNayar:
		AT_NAME::OrenNayar::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, false);
		break;
	case aten::MaterialType::Specular:
		AT_NAME::specular::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, false);
		break;
	case aten::MaterialType::Refraction:
		AT_NAME::refraction::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, false);
		break;
	case aten::MaterialType::Blinn:
		AT_NAME::MicrofacetBlinn::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, false);
		break;
	case aten::MaterialType::GGX:
		AT_NAME::MicrofacetGGX::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, false);
		break;
	case aten::MaterialType::Beckman:
		AT_NAME::MicrofacetBeckman::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, false);
		break;
	}
}

AT_CUDA_INLINE __device__ void sampleMaterial(
	AT_NAME::MaterialSampling* result,
	const Context* ctxt,
	const aten::MaterialParameter* mtrl,
	const aten::vec3& normal,
	const aten::vec3& wi,
	const aten::vec3& orgnormal,
	aten::sampler* sampler,
	float u, float v,
	const aten::vec3& externalAlbedo)
{
	switch (mtrl->type) {
	case aten::MaterialType::Emissive:
		AT_NAME::emissive::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, externalAlbedo, false);
		break;
	case aten::MaterialType::Lambert:
		AT_NAME::lambert::sample(result, mtrl, normal, wi, orgnormal, sampler, externalAlbedo, false);
		break;
	case aten::MaterialType::OrneNayar:
		AT_NAME::OrenNayar::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, externalAlbedo, false);
		break;
	case aten::MaterialType::Specular:
		AT_NAME::specular::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, externalAlbedo, false);
		break;
	case aten::MaterialType::Refraction:
		// TODO
		AT_NAME::refraction::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, false);
		break;
	case aten::MaterialType::Blinn:
		AT_NAME::MicrofacetBlinn::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, externalAlbedo, false);
		break;
	case aten::MaterialType::GGX:
		AT_NAME::MicrofacetGGX::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, externalAlbedo, false);
		break;
	case aten::MaterialType::Beckman:
		AT_NAME::MicrofacetBeckman::sample(result, mtrl, normal, wi, orgnormal, sampler, u, v, externalAlbedo, false);
		break;
	}
}

AT_CUDA_INLINE __device__ real samplePDF(
	const Context* ctxt,
	const aten::MaterialParameter* mtrl,
	const aten::vec3& normal,
	const aten::vec3& wi,
	const aten::vec3& wo,
	real u, real v)
{
	real pdf = real(0);

	switch (mtrl->type) {
	case aten::MaterialType::Emissive:
		pdf = AT_NAME::emissive::pdf(mtrl, normal, wi, wo, u, v);
		break;
	case aten::MaterialType::Lambert:
		pdf = AT_NAME::lambert::pdf(normal, wo);
		break;
	case aten::MaterialType::OrneNayar:
		pdf = AT_NAME::OrenNayar::pdf(mtrl, normal, wi, wo, u, v);
		break;
	case aten::MaterialType::Specular:
		pdf = AT_NAME::specular::pdf(mtrl, normal, wi, wo, u, v);
		break;
	case aten::MaterialType::Refraction:
		pdf = AT_NAME::refraction::pdf(mtrl, normal, wi, wo, u, v);
		break;
	case aten::MaterialType::Blinn:
		pdf = AT_NAME::MicrofacetBlinn::pdf(mtrl, normal, wi, wo, u, v);
		break;
	case aten::MaterialType::GGX:
		pdf = AT_NAME::MicrofacetGGX::pdf(mtrl, normal, wi, wo, u, v);
		break;
	case aten::MaterialType::Beckman:
		pdf = AT_NAME::MicrofacetBeckman::pdf(mtrl, normal, wi, wo, u, v);
		break;
	}

	return pdf;
}

AT_CUDA_INLINE __device__ aten::vec3 sampleDirection(
	const Context* ctxt,
	const aten::MaterialParameter* mtrl,
	const aten::vec3& normal,
	const aten::vec3& wi,
	real u, real v,
	aten::sampler* sampler)
{
	switch (mtrl->type) {
	case aten::MaterialType::Emissive:
		return AT_NAME::emissive::sampleDirection(mtrl, normal, wi, u, v, sampler);
	case aten::MaterialType::Lambert:
		return AT_NAME::lambert::sampleDirection(normal, sampler);
	case aten::MaterialType::OrneNayar:
		return AT_NAME::OrenNayar::sampleDirection(mtrl, normal, wi, u, v, sampler);
	case aten::MaterialType::Specular:
		return AT_NAME::specular::sampleDirection(mtrl, normal, wi, u, v, sampler);
	case aten::MaterialType::Refraction:
		return AT_NAME::refraction::sampleDirection(mtrl, normal, wi, u, v, sampler);
	case aten::MaterialType::Blinn:
		return AT_NAME::MicrofacetBlinn::sampleDirection(mtrl, normal, wi, u, v, sampler);
	case aten::MaterialType::GGX:
		return AT_NAME::MicrofacetGGX::sampleDirection(mtrl, normal, wi, u, v, sampler);
	case aten::MaterialType::Beckman:
		return AT_NAME::MicrofacetBeckman::sampleDirection(mtrl, normal, wi, u, v, sampler);
	}

	return std::move(aten::vec3(0, 1, 0));
}

AT_CUDA_INLINE __device__ aten::vec3 sampleBSDF(
	const Context* ctxt,
	const aten::MaterialParameter* mtrl,
	const aten::vec3& normal,
	const aten::vec3& wi,
	const aten::vec3& wo,
	real u, real v)
{
	switch (mtrl->type) {
	case aten::MaterialType::Emissive:
		return AT_NAME::emissive::bsdf(mtrl, normal, wi, wo, u, v);
	case aten::MaterialType::Lambert:
		return AT_NAME::lambert::bsdf(mtrl, u, v);
	case aten::MaterialType::OrneNayar:
		return AT_NAME::OrenNayar::bsdf(mtrl, normal, wi, wo, u, v);
	case aten::MaterialType::Specular:
		return AT_NAME::specular::bsdf(mtrl, normal, wi, wo, u, v);
	case aten::MaterialType::Refraction:
		return AT_NAME::refraction::bsdf(mtrl, normal, wi, wo, u, v);
	case aten::MaterialType::Blinn:
		return AT_NAME::MicrofacetBlinn::bsdf(mtrl, normal, wi, wo, u, v);
	case aten::MaterialType::GGX:
		return AT_NAME::MicrofacetGGX::bsdf(mtrl, normal, wi, wo, u, v);
	case aten::MaterialType::Beckman:
		return AT_NAME::MicrofacetBeckman::bsdf(mtrl, normal, wi, wo, u, v);
	}

	return std::move(aten::vec3());
}

AT_CUDA_INLINE __device__ aten::vec3 sampleBSDF(
	const Context* ctxt,
	const aten::MaterialParameter* mtrl,
	const aten::vec3& normal,
	const aten::vec3& wi,
	const aten::vec3& wo,
	real u, real v,
	const aten::vec3& externalAlbedo)
{
	switch (mtrl->type) {
	case aten::MaterialType::Emissive:
		return AT_NAME::emissive::bsdf(mtrl, externalAlbedo);
	case aten::MaterialType::Lambert:
		return AT_NAME::lambert::bsdf(mtrl, externalAlbedo);
	case aten::MaterialType::OrneNayar:
		return AT_NAME::OrenNayar::bsdf(mtrl, normal, wi, wo, u, v, externalAlbedo);
	case aten::MaterialType::Specular:
		return AT_NAME::specular::bsdf(mtrl, normal, wi, wo, u, v, externalAlbedo);
	case aten::MaterialType::Refraction:
		return AT_NAME::refraction::bsdf(mtrl, normal, wi, wo, u, v, externalAlbedo);
	case aten::MaterialType::Blinn:
		return AT_NAME::MicrofacetBlinn::bsdf(mtrl, normal, wi, wo, u, v, externalAlbedo);
	case aten::MaterialType::GGX:
		return AT_NAME::MicrofacetGGX::bsdf(mtrl, normal, wi, wo, u, v, externalAlbedo);
	case aten::MaterialType::Beckman:
		return AT_NAME::MicrofacetBeckman::bsdf(mtrl, normal, wi, wo, u, v, externalAlbedo);
	}

	return std::move(aten::vec3());
}

AT_CUDA_INLINE __device__ real computeFresnel(
	const aten::MaterialParameter* mtrl,
	const aten::vec3& normal,
	const aten::vec3& wi,
	const aten::vec3& wo,
	real outsideIor/*= 1*/)
{
	switch (mtrl->type) {
	case aten::MaterialType::Blinn:
	case aten::MaterialType::GGX:
	case aten::MaterialType::Beckman:
		return AT_NAME::material::computeFresnel(mtrl, normal, wi, wo, outsideIor);
	case aten::MaterialType::Emissive:
		return AT_NAME::emissive::computeFresnel(mtrl, normal, wi, wo, outsideIor);
	case aten::MaterialType::Lambert:
		return AT_NAME::lambert::computeFresnel(mtrl, normal, wi, wo, outsideIor);
	case aten::MaterialType::OrneNayar:
		return AT_NAME::OrenNayar::computeFresnel(mtrl, normal, wi, wo, outsideIor);
	case aten::MaterialType::Specular:
		return AT_NAME::specular::computeFresnel(mtrl, normal, wi, wo, outsideIor);
	case aten::MaterialType::Refraction:
		return AT_NAME::refraction::computeFresnel(mtrl, normal, wi, wo, outsideIor);
	}

	return real(1);
}