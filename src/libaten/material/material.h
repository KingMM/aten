#pragma once

#include "types.h"
#include "math/vec3.h"
#include "sampler/sampler.h"
#include "texture/texture.h"
#include "misc/value.h"
#include "math/ray.h"

namespace aten
{
	struct hitrecord;

	struct MaterialParam {
		// �T�[�t�F�C�X�J���[�C�ʏ�e�N�X�`���}�b�v�ɂ���ċ��������.
		union {
			vec3 baseColor;
			float baseColorArray[3];
		};

		float ior{ 1.0f };
		float roughness{ 0.0f };			// �\�ʂ̑e���ŁC�f�B�t���[�Y�ƃX�y�L�����[���X�|���X�̗����𐧌䂵�܂�.
		float shininess{ 0.0f };

		float subsurface{ 0.0f };			// �\�ʉ��̋ߎ���p���ăf�B�t���[�Y�`��𐧌䂷��.
		float metallic{ 0.0f };				// �����x(0 = �U�d��, 1 = ����)�B�����2�̈قȂ郂�f���̐��`�u�����h�ł��B�������f���̓f�B�t���[�Y�R���|�[�l���g���������C�܂��F�����t�����ꂽ���˃X�y�L�����[�������C��{�F�ɓ������Ȃ�܂�.
		float specular{ 0.0f };				// ���ˋ��ʔ��˗ʁB����͖����I�ȋ��ܗ��̑���ɂ���܂�.
		float specularTint{ 0.0f };			// ���˃X�y�L�����[����{�F�Ɍ������F�������A�[�e�B�X�e�B�b�N�Ȑ��䂷�邽�߂̏����B�O���[�W���O�X�y�L�����[�̓A�N���}�e�B�b�N�̂܂܂ł�.
		float anisotropic{ 0.0f };			// �ٕ����̓x�����B����̓X�y�L�����[�n�C���C�g�̃A�X�y�N�g��𐧌䂵�܂�(0 = ������, 1 = �ő�ٕ���).
		float sheen{ 0.0f };				// �ǉ��I�ȃO���[�W���O�R���|�[�l���g�C��ɕz�ɑ΂��ĈӐ}���Ă���.
		float sheenTint{ 0.0f };			// ��{�F�Ɍ���������F�����̗�.
		float clearcoat{ 0.0f };			// ���̓��ʂȖړI�̃X�y�L�����[���[�u.
		float clearcoatGloss{ 0.0f };		// �N���A�R�[�g�̌���x�𐧌䂷��(0 = �g�T�e���h��, 1 = �g�O���X�h��).

		union {								
			int albedoMapIdx;
			void* albedoMap{ nullptr };
		};

		union {
			int normalMapIdx;
			void* normalMap{ nullptr };
		};

		union {
			int roughnessMapIdx;
			void* roughnessMap{ nullptr };
		};

		MaterialParam() {}
	};

	class material {
		friend class LayeredBSDF;

	protected:
		material();
		virtual ~material() {}

	protected:
		material(
			const vec3& clr, 
			real ior = 1,
			texture* albedoMap = nullptr, 
			texture* normalMap = nullptr) 
		{
			m_param.baseColor = clr;
			m_param.ior = ior;
			m_param.albedoMap = albedoMap;
			m_param.normalMap = normalMap;
		}
		material(Values& val)
		{
			m_param.baseColor = val.get("color", m_param.baseColor);
			m_param.ior = val.get("ior", m_param.ior);
			m_param.albedoMap = (texture*)val.get("albedomap", (void*)m_param.albedoMap);
			m_param.normalMap = (texture*)val.get("normalmap", (void*)m_param.normalMap);
		}

	public:
		virtual bool isEmissive() const
		{
			return false;
		}

		virtual bool isSingular() const
		{
			return false;
		}

		virtual bool isTranslucent() const
		{
			return false;
		}

		virtual bool isGlossy() const
		{
			return false;
		}

		virtual bool isNPR() const
		{
			return false;
		}

		const vec3& color() const
		{
			return m_param.baseColor;
		}

		uint32_t id() const
		{
			return m_id;
		}

		virtual vec3 sampleAlbedoMap(real u, real v) const
		{
			vec3 albedo(1, 1, 1);
			if (m_param.albedoMap) {
				albedo = ((texture*)m_param.albedoMap)->at(u, v);
			}
			return std::move(albedo);
		}

		virtual void applyNormalMap(
			const vec3& orgNml,
			vec3& newNml,
			real u, real v) const;

		virtual real computeFresnel(
			const vec3& normal,
			const vec3& wi,
			const vec3& wo,
			real outsideIor = 1) const;

		virtual real pdf(
			const vec3& normal, 
			const vec3& wi,
			const vec3& wo,
			real u, real v) const = 0;

		virtual vec3 sampleDirection(
			const ray& ray,
			const vec3& normal, 
			real u, real v,
			sampler* sampler) const = 0;

		virtual vec3 bsdf(
			const vec3& normal, 
			const vec3& wi,
			const vec3& wo,
			real u, real v) const = 0;

		struct sampling {
			vec3 dir;
			vec3 bsdf;
			real pdf{ real(0) };
			real fresnel{ real(1) };

			real subpdf{ real(1) };

			sampling() {}
			sampling(const vec3& d, const vec3& b, real p)
				: dir(d), bsdf(b), pdf(p)
			{}
		};

		virtual sampling sample(
			const ray& ray,
			const vec3& normal,
			const hitrecord& hitrec,
			sampler* sampler,
			real u, real v,
			bool isLightPath = false) const = 0;

		real ior() const
		{
			return m_param.ior;
		}

	protected:
		static vec3 sampleTexture(texture* tex, real u, real v, real defaultValue)
		{
			auto ret = sampleTexture(tex, u, v, vec3(defaultValue));
			return std::move(ret);
		}

		static vec3 sampleTexture(texture* tex, real u, real v, const vec3& defaultValue)
		{
			vec3 ret = defaultValue;
			if (tex) {
				ret = tex->at(u, v);
			}
			return std::move(ret);
		}

		static void serialize(const material* mtrl, MaterialParam& param);

	protected:
		uint32_t m_id{ 0 };

		MaterialParam m_param;
	};

	class Light;

	class NPRMaterial : public material {
	protected:
		NPRMaterial() {}
		NPRMaterial(const vec3& e, Light* light);

		NPRMaterial(Values& val)
			: material(val)
		{}

		virtual ~NPRMaterial() {}

	public:
		virtual bool isNPR() const override final
		{
			return true;
		}

		virtual real computeFresnel(
			const vec3& normal,
			const vec3& wi,
			const vec3& wo,
			real outsideIor = 1) const override final
		{
			return real(1);
		}

		void setTargetLight(Light* light);

		const Light* getTargetLight() const;

		virtual vec3 bsdf(
			real cosShadow,
			real u, real v) const = 0;

	private:
		Light* m_targetLight{ nullptr };
	};


	real schlick(
		const vec3& in,
		const vec3& normal,
		real ni, real nt);

	real computFresnel(
		const vec3& in,
		const vec3& normal,
		real ni, real nt);
}
