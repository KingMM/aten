#pragma once

#include "material/material.h"
#include "texture/texture.h"

namespace aten
{
	class DisneyBRDF : public material {
	public:
		struct Parameter {
			vec3 baseColor{ vec3(0.82, 0.67, 0.16) };
			real metallic{ 0 };
			real subsurface{ 0 };
			real specular{ 0.5 };
			real roughness{ 0.5 };
			real specularTint{ 0 };
			real anisotropic{ 0 };
			real sheen{ 0 };
			real sheenTint{ 0.5 };
			real clearcoat{ 0 };
			real clearcoatGloss{ 1 };
		};
	public:
		DisneyBRDF() {}
		DisneyBRDF(
			vec3 baseColor,
			real subsurface,
			real metallic,
			real specular,
			real specularTint,
			real roughness,
			real anisotropic,
			real sheen,
			real sheenTint,
			real clearcoat,
			real clearcoatGloss,
			texture* albedoMap = nullptr,
			texture* normalMap = nullptr,
			texture* roughnessMap = nullptr)
			: material(baseColor, 1, albedoMap, normalMap)
		{
			m_baseColor = baseColor;
			m_subsurface = aten::clamp<real>(subsurface, 0, 1);
			m_metallic = aten::clamp<real>(metallic, 0, 1);
			m_specular = aten::clamp<real>(specular, 0, 1);
			m_specularTint = aten::clamp<real>(specularTint, 0, 1);
			m_roughness = aten::clamp<real>(roughness, 0, 1);
			m_anisotropic = aten::clamp<real>(anisotropic, 0, 1);
			m_sheen = aten::clamp<real>(sheen, 0, 1);
			m_sheenTint = aten::clamp<real>(sheenTint, 0, 1);
			m_clearcoat = aten::clamp<real>(clearcoat, 0, 1);
			m_clearcoatGloss = aten::clamp<real>(clearcoatGloss, 0, 1);

			m_roughnessMap = roughnessMap;
		}

		DisneyBRDF(
			const Parameter& param,
			texture* albedoMap = nullptr,
			texture* normalMap = nullptr,
			texture* roughnessMap = nullptr)
			: material(param.baseColor, 1, albedoMap, normalMap)
		{
			m_baseColor = param.baseColor;
			m_subsurface = aten::clamp<real>(param.subsurface, 0, 1);
			m_metallic = aten::clamp<real>(param.metallic, 0, 1);
			m_specular = aten::clamp<real>(param.specular, 0, 1);
			m_specularTint = aten::clamp<real>(param.specularTint, 0, 1);
			m_roughness = aten::clamp<real>(param.roughness, 0, 1);
			m_anisotropic = aten::clamp<real>(param.anisotropic, 0, 1);
			m_sheen = aten::clamp<real>(param.sheen, 0, 1);
			m_sheenTint = aten::clamp<real>(param.sheenTint, 0, 1);
			m_clearcoat = aten::clamp<real>(param.clearcoat, 0, 1);
			m_clearcoatGloss = aten::clamp<real>(param.clearcoatGloss, 0, 1);

			m_roughnessMap = roughnessMap;
		}

		DisneyBRDF(Values& val)
			: material(val)
		{
			// TODO
			// Clamp parameters.
			m_subsurface = val.get("subsurface", m_subsurface);
			m_metallic = val.get("metallic", m_metallic);
			m_specular = val.get("specular", m_specular);
			m_specularTint = val.get("specularTint", m_specularTint);
			m_roughness = val.get("roughness", m_roughness);
			m_anisotropic = val.get("anisotropic", m_anisotropic);
			m_sheen = val.get("sheen", m_sheen);
			m_sheenTint = val.get("sheenTint", m_sheenTint);
			m_clearcoat = val.get("clearcoat", m_clearcoat);
			m_clearcoatGloss = val.get("clearcoatGloss", m_clearcoatGloss);
			m_roughnessMap = val.get("roughnessmap", m_roughnessMap);
		}

		virtual ~DisneyBRDF() {}

	public:
		virtual bool isGlossy() const override final
		{
			return (m_roughness == 1 ? false : true);
		}

		virtual real pdf(
			const vec3& normal, 
			const vec3& wi,
			const vec3& wo,
			real u, real v,
			sampler* sampler) const override final;

		virtual vec3 sampleDirection(
			const ray& ray,
			const vec3& normal,
			real u, real v,
			sampler* sampler) const override final;

		virtual vec3 bsdf(
			const vec3& normal, 
			const vec3& wi,
			const vec3& wo,
			real u, real v) const override final;

		virtual sampling sample(
			const ray& ray,
			const vec3& normal,
			const hitrecord& hitrec,
			sampler* sampler,
			real u, real v) const override final;

	private:
		real pdf(
			const vec3& V,
			const vec3& N,
			const vec3& L,
			const vec3& X,
			const vec3& Y,
			real u, real v) const;

		vec3 sampleDirection(
			const vec3& V,
			const vec3& N,
			const vec3& X,
			const vec3& Y,
			real u, real v,
			sampler* sampler) const;

		vec3 bsdf(
			real& fresnel,
			const vec3& V,
			const vec3& N,
			const vec3& L,
			const vec3& X,
			const vec3& Y,
			real u, real v) const;

	private:
		vec3 m_baseColor;		// �T�[�t�F�C�X�J���[�C�ʏ�e�N�X�`���}�b�v�ɂ���ċ��������.
		real m_subsurface;		// �\�ʉ��̋ߎ���p���ăf�B�t���[�Y�`��𐧌䂷��.
		real m_metallic;		// �����x(0 = �U�d��, 1 = ����)�B�����2�̈قȂ郂�f���̐��`�u�����h�ł��B�������f���̓f�B�t���[�Y�R���|�[�l���g���������C�܂��F�����t�����ꂽ���˃X�y�L�����[�������C��{�F�ɓ������Ȃ�܂�.
		real m_specular;		// ���ˋ��ʔ��˗ʁB����͖����I�ȋ��ܗ��̑���ɂ���܂�.
		real m_specularTint;	// ���˃X�y�L�����[����{�F�Ɍ������F�������A�[�e�B�X�e�B�b�N�Ȑ��䂷�邽�߂̏����B�O���[�W���O�X�y�L�����[�̓A�N���}�e�B�b�N�̂܂܂ł�.
		real m_roughness;		// �\�ʂ̑e���ŁC�f�B�t���[�Y�ƃX�y�L�����[���X�|���X�̗����𐧌䂵�܂�.
		real m_anisotropic;		// �ٕ����̓x�����B����̓X�y�L�����[�n�C���C�g�̃A�X�y�N�g��𐧌䂵�܂�(0 = ������, 1 = �ő�ٕ���).
		real m_sheen;			// �ǉ��I�ȃO���[�W���O�R���|�[�l���g�C��ɕz�ɑ΂��ĈӐ}���Ă���.
		real m_sheenTint;		// ��{�F�Ɍ���������F�����̗�.
		real m_clearcoat;		// ���̓��ʂȖړI�̃X�y�L�����[���[�u.
		real m_clearcoatGloss;	// �N���A�R�[�g�̌���x�𐧌䂷��(0 = �g�T�e���h��, 1 = �g�O���X�h��).

		texture* m_roughnessMap{ nullptr };
	};
}
