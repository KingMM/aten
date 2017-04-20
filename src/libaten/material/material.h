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

	struct MaterialType {
		struct {
			const uint32_t isEmissive : 1;
			const uint32_t isSingular : 1;
			const uint32_t isTranslucent : 1;
			const uint32_t isGlossy : 1;
			const uint32_t isNPR : 1;
		};

		MaterialType(
			bool _isEmissive = false,
			bool _isSingular = false,
			bool _isTranslucent = false,
			bool _isGlossy = false,
			bool _isNPR = false)
			: isEmissive(_isEmissive), isSingular(_isSingular), isTranslucent(_isTranslucent),
			isGlossy(_isGlossy), isNPR(_isNPR)
		{}
		MaterialType(const MaterialType& type)
			: MaterialType(type.isEmissive, type.isSingular, type.isTranslucent, type.isGlossy, type.isNPR)
		{}
	};
	
	//													Em     Sp      Tr    Gl    NPR
	#define MaterialTypeMicrofacet	aten::MaterialType(false, false, false, true,  false)
	#define MaterialTypeLambert		aten::MaterialType(false, false, false, false, false)
	#define MaterialTypeEmissive	aten::MaterialType(true,  false, false, false, false)
	#define MaterialTypeSpecular	aten::MaterialType(false, true,  false, true,  false)
	#define MaterialTypeRefraction	aten::MaterialType(false, true,  true,  true,  false)
	#define MaterialTypeNPR			aten::MaterialType(false, false, false, false, true)

	struct MaterialParameter {
		vec3 baseColor;					// �T�[�t�F�C�X�J���[�C�ʏ�e�N�X�`���}�b�v�ɂ���ċ��������.

		real ior{ 1.0f };
		real roughness{ 0.0f };			// �\�ʂ̑e���ŁC�f�B�t���[�Y�ƃX�y�L�����[���X�|���X�̗����𐧌䂵�܂�.
		real shininess{ 1.0f };

		real subsurface{ 0.0f };		// �\�ʉ��̋ߎ���p���ăf�B�t���[�Y�`��𐧌䂷��.
		real metallic{ 0.0f };			// �����x(0 = �U�d��, 1 = ����)�B�����2�̈قȂ郂�f���̐��`�u�����h�ł��B�������f���̓f�B�t���[�Y�R���|�[�l���g���������C�܂��F�����t�����ꂽ���˃X�y�L�����[�������C��{�F�ɓ������Ȃ�܂�.
		real specular{ 0.0f };			// ���ˋ��ʔ��˗ʁB����͖����I�ȋ��ܗ��̑���ɂ���܂�.
		real specularTint{ 0.0f };		// ���˃X�y�L�����[����{�F�Ɍ������F�������A�[�e�B�X�e�B�b�N�Ȑ��䂷�邽�߂̏����B�O���[�W���O�X�y�L�����[�̓A�N���}�e�B�b�N�̂܂܂ł�.
		real anisotropic{ 0.0f };		// �ٕ����̓x�����B����̓X�y�L�����[�n�C���C�g�̃A�X�y�N�g��𐧌䂵�܂�(0 = ������, 1 = �ő�ٕ���).
		real sheen{ 0.0f };				// �ǉ��I�ȃO���[�W���O�R���|�[�l���g�C��ɕz�ɑ΂��ĈӐ}���Ă���.
		real sheenTint{ 0.0f };			// ��{�F�Ɍ���������F�����̗�.
		real clearcoat{ 0.0f };			// ���̓��ʂȖړI�̃X�y�L�����[���[�u.
		real clearcoatGloss{ 0.0f };	// �N���A�R�[�g�̌���x�𐧌䂷��(0 = �g�T�e���h��, 1 = �g�O���X�h��).
	
		UnionIdxPtr albedoMap;
		UnionIdxPtr normalMap;
		UnionIdxPtr roughnessMap;

		bool isIdealRefraction{ false };

		const MaterialType type;

		MaterialParameter(const MaterialType& _type)
			: type(_type)
		{}
	};

	struct MaterialSampling {
		vec3 dir;
		vec3 bsdf;
		real pdf{ real(0) };
		real fresnel{ real(1) };

		real subpdf{ real(1) };

		MaterialSampling() {}
		MaterialSampling(const vec3& d, const vec3& b, real p)
			: dir(d), bsdf(b), pdf(p)
		{}
	};

	class material {
		friend class LayeredBSDF;

		static std::vector<material*> g_materials;

	protected:
		material();
		virtual ~material();

		material(
			const MaterialType& type,
			const vec3& clr,
			real ior = 1,
			texture* albedoMap = nullptr,
			texture* normalMap = nullptr);

		material(const MaterialType& type, Values& val);

	public:
		bool isEmissive() const
		{
			return m_param.type.isEmissive;
		}

		bool isSingular() const
		{
			return m_param.type.isSingular;
		}

		bool isTranslucent() const
		{
			return m_param.type.isTranslucent;
		}

		// TODO
		virtual bool isGlossy() const
		{
			bool isGlossy = m_param.type.isGlossy;

			if (isGlossy) {
				isGlossy = (m_param.roughness == real(1) ? false : true);
				if (!isGlossy) {
					isGlossy = (m_param.shininess == 0 ? false : true);
				}
			}

			return isGlossy;
		}

		bool isNPR() const
		{
			return m_param.type.isNPR;
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
			return std::move(sampleTexture((const texture*)m_param.albedoMap.ptr, u, v, real(1)));
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

		virtual MaterialSampling sample(
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

		const MaterialParameter& param() const
		{
			return m_param;
		}

		static vec3 sampleTexture(const texture* tex, real u, real v, real defaultValue)
		{
			auto ret = sampleTexture(tex, u, v, vec3(defaultValue));
			return std::move(ret);
		}

		static vec3 sampleTexture(const texture* tex, real u, real v, const vec3& defaultValue)
		{
			vec3 ret = defaultValue;
			if (tex) {
				ret = tex->at(u, v);
			}
			return std::move(ret);
		}

		static uint32_t getMaterialNum();
		static const material* getMaterial(uint32_t idx);
		static int findMaterialIdx(material* mtrl);
		static const std::vector<material*>& getMaterials();

	protected:
		uint32_t m_id{ 0 };

		MaterialParameter m_param;
	};

	class Light;

	class NPRMaterial : public material {
	protected:
		NPRMaterial(const vec3& e, Light* light);

		NPRMaterial(Values& val)
			: material(MaterialTypeNPR, val)
		{}

		virtual ~NPRMaterial() {}

	public:
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
