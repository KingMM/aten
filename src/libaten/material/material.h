#pragma once

#include "types.h"
#include "math/vec3.h"
#include "sampler/sampler.h"
#include "texture/texture.h"
#include "misc/value.h"
#include "math/ray.h"

#include "material/sample_texture.h"

namespace AT_NAME {
	class Light;
}

namespace aten
{
	struct hitrecord;

	struct MaterialAttribute {
		struct {
			uint32_t isEmissive : 1;
			uint32_t isSingular : 1;
			uint32_t isTranslucent : 1;
			uint32_t isGlossy : 1;
			uint32_t isNPR : 1;
		};

		AT_DEVICE_API MaterialAttribute(
			bool _isEmissive = false,
			bool _isSingular = false,
			bool _isTranslucent = false,
			bool _isGlossy = false,
			bool _isNPR = false)
			: isEmissive(_isEmissive), isSingular(_isSingular), isTranslucent(_isTranslucent),
			isGlossy(_isGlossy), isNPR(_isNPR)
		{}
		AT_DEVICE_API MaterialAttribute(const MaterialAttribute& type)
			: MaterialAttribute(type.isEmissive, type.isSingular, type.isTranslucent, type.isGlossy, type.isNPR)
		{}
	};

	//													          Em     Sp      Tr    Gl    NPR
	#define MaterialAttributeMicrofacet	aten::MaterialAttribute(false, false, false, true,  false)
	#define MaterialAttributeLambert	aten::MaterialAttribute(false, false, false, false, false)
	#define MaterialAttributeEmissive	aten::MaterialAttribute(true,  false, false, false, false)
	#define MaterialAttributeSpecular	aten::MaterialAttribute(false, true,  false, true,  false)
	#define MaterialAttributeRefraction	aten::MaterialAttribute(false, true,  true,  true,  false)
	#define MaterialAttributeNPR		aten::MaterialAttribute(false, false, false, false, true)

	enum MaterialType : int {
		Emissive,
		Lambert,
		OrneNayar,
		Specular,
		Refraction,
		Blinn,
		GGX,
		Beckman,
		Disney,
		CarPaint,
		Toon,
		Layer,

		MaterialTypeMax,
	};

	struct MaterialParameter {
		MaterialType type;

		aten::vec3 baseColor;			// �T�[�t�F�C�X�J���[�C�ʏ�e�N�X�`���}�b�v�ɂ���ċ��������.

		// NOTE
		// https://www.cs.uaf.edu/2012/spring/cs481/section/0/lecture/02_14_refraction.html
		// - Index Of Refraction
		//	 Water's index of refraction is a mild 1.3; diamond's is a high 2.4.
		// - eta
		//   ���ܗ��̔�.
		//   ex) eta = 1.0 / 1.4  : air/glass's index of refraction.
		real ior{ 1.0 };				// ���ܗ�.

#if 0
		real roughness{ 0.5 };			// �\�ʂ̑e���ŁC�f�B�t���[�Y�ƃX�y�L�����[���X�|���X�̗����𐧌䂵�܂�.
		real shininess{ 1.0 };

		real subsurface{ 0.0 };			// �\�ʉ��̋ߎ���p���ăf�B�t���[�Y�`��𐧌䂷��.
		real metallic{ 0.0 };			// �����x(0 = �U�d��, 1 = ����)�B�����2�̈قȂ郂�f���̐��`�u�����h�ł��B�������f���̓f�B�t���[�Y�R���|�[�l���g���������C�܂��F�����t�����ꂽ���˃X�y�L�����[�������C��{�F�ɓ������Ȃ�܂�.
		real specular{ 0.5 };			// ���ˋ��ʔ��˗ʁB����͖����I�ȋ��ܗ��̑���ɂ���܂�.
		real specularTint{ 0.0 };		// ���˃X�y�L�����[����{�F�Ɍ������F�������A�[�e�B�X�e�B�b�N�Ȑ��䂷�邽�߂̏����B�O���[�W���O�X�y�L�����[�̓A�N���}�e�B�b�N�̂܂܂ł�.
		real anisotropic{ 0.0 };		// �ٕ����̓x�����B����̓X�y�L�����[�n�C���C�g�̃A�X�y�N�g��𐧌䂵�܂�(0 = ������, 1 = �ő�ٕ���).
		real sheen{ 0.0 };				// �ǉ��I�ȃO���[�W���O�R���|�[�l���g�C��ɕz�ɑ΂��ĈӐ}���Ă���.
		real sheenTint{ 0.5 };			// ��{�F�Ɍ���������F�����̗�.
		real clearcoat{ 0.0 };			// ���̓��ʂȖړI�̃X�y�L�����[���[�u.
		real clearcoatGloss{ 1.0 };		// �N���A�R�[�g�̌���x�𐧌䂷��(0 = �g�T�e���h��, 1 = �g�O���X�h��).
#else
		union {
			struct {
				real roughness;			// �\�ʂ̑e���ŁC�f�B�t���[�Y�ƃX�y�L�����[���X�|���X�̗����𐧌䂵�܂�.
				real shininess;
				real subsurface;		// �\�ʉ��̋ߎ���p���ăf�B�t���[�Y�`��𐧌䂷��.
				real metallic;			// �����x(0 = �U�d��, 1 = ����)�B�����2�̈قȂ郂�f���̐��`�u�����h�ł��B�������f���̓f�B�t���[�Y�R���|�[�l���g���������C�܂��F�����t�����ꂽ���˃X�y�L�����[�������C��{�F�ɓ������Ȃ�܂�.
				real specular;			// ���ˋ��ʔ��˗ʁB����͖����I�ȋ��ܗ��̑���ɂ���܂�.
				real specularTint;		// ���˃X�y�L�����[����{�F�Ɍ������F�������A�[�e�B�X�e�B�b�N�Ȑ��䂷�邽�߂̏����B�O���[�W���O�X�y�L�����[�̓A�N���}�e�B�b�N�̂܂܂ł�.
				real anisotropic;		// �ٕ����̓x�����B����̓X�y�L�����[�n�C���C�g�̃A�X�y�N�g��𐧌䂵�܂�(0 = ������, 1 = �ő�ٕ���).
				real sheen;				// �ǉ��I�ȃO���[�W���O�R���|�[�l���g�C��ɕz�ɑ΂��ĈӐ}���Ă���.
				real sheenTint;			// ��{�F�Ɍ���������F�����̗�.
				real clearcoat;			// ���̓��ʂȖړI�̃X�y�L�����[���[�u.
				real clearcoatGloss;	// �N���A�R�[�g�̌���x�𐧌䂷��(0 = �g�T�e���h��, 1 = �g�O���X�h��).
			};
			struct {
				real clearcoatRoughness;
				real flakeLayerRoughness;
				real flake_scale;				// Smaller values zoom into the flake map, larger values zoom out.
				real flake_size;				// Relative size of the flakes
				real flake_size_variance;		// 0.0 makes all flakes the same size, 1.0 assigns random size between 0 and the given flake size
				real flake_normal_orientation;	// Blend between the flake normals (0.0) and the surface normal (1.0)
				real flake_reflection;
				real flake_transmittance;
				aten::vec3 glitterColor;
				aten::vec3 flakeColor;
				real flake_intensity;
			} carpaint;
		};
#endif

		MaterialAttribute attrib;

		struct {
			uint32_t isIdealRefraction : 1;
		};

		union {
			struct {
				int albedoMap;
				int normalMap;
				int roughnessMap;
			};
			int layer[3];
		};

		AT_DEVICE_API MaterialParameter()
		{
			isIdealRefraction = false;
			albedoMap = -1;
			normalMap = -1;
			roughnessMap = -1;

			roughness = real(0.5);
			shininess = real(1.0);

			subsurface = real(0.0);
			metallic = real(0.0);
			specular = real(0.5);
			specularTint = real(0.0);
			anisotropic = real(0.0);
			sheen = real(0.0);
			sheenTint = real(0.5);
			clearcoat = real(0.0);
			clearcoatGloss = real(1.0);
		}
		AT_DEVICE_API MaterialParameter(MaterialType _type, const MaterialAttribute& _attrib)
			: type(_type), attrib(_attrib)
		{
			isIdealRefraction = false;
			albedoMap = -1;
			normalMap = -1;
			roughnessMap = -1;

			roughness = real(0.5);
			shininess = real(1.0);

			subsurface = real(0.0);
			metallic = real(0.0);
			specular = real(0.5);
			specularTint = real(0.0);
			anisotropic = real(0.0);
			sheen = real(0.0);
			sheenTint = real(0.5);
			clearcoat = real(0.0);
			clearcoatGloss = real(1.0);
		}
	};

	class IMaterialParamEditor {
	protected:
		IMaterialParamEditor() {}
		virtual ~IMaterialParamEditor() {}

	public:
		virtual bool edit(const char* name, real& param, real _min = real(0), real _max = real(1)) = 0;
		virtual bool edit(const char* name, vec3& param) = 0;

		void editTex(const char* name, int texid)
		{
			if (texid >= 0) {
				auto tex = aten::texture::getTexture(texid);
				if (tex) {
					edit(name, tex->name());
				}
			}
		}

	protected:
		virtual void edit(const char* name, const char* str) = 0;
	};

	enum class MtrlParamType {
		Vec3,
		Texture,
		Double,

		Num,
	};
}

#if defined(_WIN32) || defined(_WIN64)
#define AT_EDIT_MATERIAL_PARAM(e, param, name)	(e)->edit(#name, param.##name)
#define AT_EDIT_MATERIAL_PARAM_RANGE(e, param, name, _min, _max)	(e)->edit(#name, param.##name, _min, _max)
#define AT_EDIT_MATERIAL_PARAM_TEXTURE(e, param, name)	(e)->editTex(#name, param.##name)
#else
// TODO
// For linux, to avoid token concat error.
#define AT_EDIT_MATERIAL_PARAM(e, param, name) false
#define AT_EDIT_MATERIAL_PARAM_RANGE(e, param, name, _min, _max) false
#define AT_EDIT_MATERIAL_PARAM_TEXTURE(e, param, name) false
#endif

namespace AT_NAME
{
	struct MaterialSampling {
		aten::vec3 dir;
		aten::vec3 bsdf;
		real pdf{ real(0) };
		real fresnel{ real(1) };

		real subpdf{ real(1) };

		AT_DEVICE_MTRL_API MaterialSampling() {}
		AT_DEVICE_MTRL_API MaterialSampling(const aten::vec3& d, const aten::vec3& b, real p)
			: dir(d), bsdf(b), pdf(p)
		{}
	};

	class material {
		friend class LayeredBSDF;

		static std::vector<material*> g_materials;
		static std::vector<const char*> g_mtrlTypeNames;

	protected:
		material(
			aten::MaterialType type, 
			const aten::MaterialAttribute& attrib,
			bool local = false);

		material(
			aten::MaterialType type,
			const aten::MaterialAttribute& attrib,
			const aten::vec3& clr,
			real ior = 1,
			aten::texture* albedoMap = nullptr,
			aten::texture* normalMap = nullptr,
			bool local = false);

		material(
			aten::MaterialType type, 
			const aten::MaterialAttribute& attrib, 
			aten::Values& val,
			bool local = false);

	public:
		virtual ~material();

		bool isEmissive() const
		{
			return m_param.attrib.isEmissive;
		}

		bool isSingular() const
		{
			return m_param.attrib.isSingular;
		}

		bool isTranslucent() const
		{
			return m_param.attrib.isTranslucent;
		}

		// TODO
		virtual bool isGlossy() const
		{
			bool isGlossy = m_param.attrib.isGlossy;

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
			return m_param.attrib.isNPR;
		}

		const aten::vec3& color() const
		{
			return m_param.baseColor;
		}

		int id() const
		{
			return m_id;
		}

		void setTextures(
			aten::texture* albedoMap,
			aten::texture* normalMap,
			aten::texture* roughnessMap);

		virtual AT_DEVICE_MTRL_API aten::vec3 sampleAlbedoMap(real u, real v) const
		{
			return std::move(sampleTexture(m_param.albedoMap, u, v, real(1)));
		}

		virtual AT_DEVICE_MTRL_API void applyNormalMap(
			const aten::vec3& orgNml,
			aten::vec3& newNml,
			real u, real v) const;

		static AT_DEVICE_MTRL_API void applyNormalMap(
			const int normalMapIdx,
			const aten::vec3& orgNml,
			aten::vec3& newNml,
			real u, real v)
		{
			if (normalMapIdx >= 0) {
				auto nml = sampleTexture(normalMapIdx, u, v, real(0));
				nml = real(2) * nml - aten::vec3(1);	// [0, 1] -> [-1, 1].
				nml = normalize(nml);

				aten::vec3 n = normalize(orgNml);
				aten::vec3 t = aten::getOrthoVector(n);
				aten::vec3 b = cross(n, t);

				newNml = nml.z * n + nml.x * t + nml.y * b;
				newNml = normalize(newNml);
			}
			else {
				newNml = normalize(orgNml);
			}
		}

		virtual AT_DEVICE_MTRL_API real computeFresnel(
			const aten::vec3& normal,
			const aten::vec3& wi,
			const aten::vec3& wo,
			real outsideIor = 1) const
		{
			return computeFresnel(&m_param, normal, wi, wo, outsideIor);
		}

		static AT_DEVICE_MTRL_API real computeFresnel(
			const aten::MaterialParameter* mtrl,
			const aten::vec3& normal,
			const aten::vec3& wi,
			const aten::vec3& wo,
			real outsideIor)
		{
			aten::vec3 V = -wi;
			aten::vec3 L = wo;
			aten::vec3 H = normalize(L + V);

			auto ni = outsideIor;
			auto nt = mtrl->ior;

			// NOTE
			// Fschlick(v,h) �� R0 + (1 - R0)(1 - cos��)^5
			// R0 = ((n1 - n2) / (n1 + n2))^2

			auto r0 = (ni - nt) / (ni + nt);
			r0 = r0 * r0;

			auto LdotH = aten::abs(dot(L, H));

			auto F = r0 + (1 - r0) * aten::pow((1 - LdotH), 5);

			return F;
		}

		virtual AT_DEVICE_MTRL_API real pdf(
			const aten::vec3& normal,
			const aten::vec3& wi,
			const aten::vec3& wo,
			real u, real v) const = 0;

		virtual AT_DEVICE_MTRL_API aten::vec3 sampleDirection(
			const aten::ray& ray,
			const aten::vec3& normal,
			real u, real v,
			aten::sampler* sampler) const = 0;

		virtual AT_DEVICE_MTRL_API aten::vec3 bsdf(
			const aten::vec3& normal,
			const aten::vec3& wi,
			const aten::vec3& wo,
			real u, real v) const = 0;

		virtual AT_DEVICE_MTRL_API MaterialSampling sample(
			const aten::ray& ray,
			const aten::vec3& normal,
			const aten::vec3& orgnormal,
			aten::sampler* sampler,
			real u, real v,
			bool isLightPath = false) const = 0;

		real ior() const
		{
			return m_param.ior;
		}

		const aten::MaterialParameter& param() const
		{
			return m_param;
		}
		aten::MaterialParameter& param() 
		{
			return m_param;
		}

		void copyParam(const aten::MaterialParameter& param)
		{
			AT_ASSERT(m_param.type == param.type);

			if (m_param.type == param.type) {
				m_param = param;
			}
		}

		// TODO
		void copyParamEx(const aten::MaterialParameter& param)
		{
			m_param.baseColor = param.baseColor;
			m_param.ior = param.ior;
			m_param.shininess = param.shininess;
			m_param.roughness = param.roughness;			
			m_param.subsurface = param.subsurface;
			m_param.metallic = param.metallic;
			m_param.specular = param.specular;
			m_param.specularTint = param.specularTint;
			m_param.anisotropic = param.anisotropic;
			m_param.sheen = param.sheen;
			m_param.sheenTint = param.sheenTint;
			m_param.clearcoat = param.clearcoat;
			m_param.clearcoatGloss = param.clearcoatGloss;

			m_param.albedoMap = param.albedoMap;
			m_param.normalMap = param.normalMap;
			m_param.roughnessMap = param.roughnessMap;

			m_param.attrib = param.attrib;
		}

		void setName(const char* name)
		{
			m_name = name;
		}

		const char* name() const
		{
			return m_name.c_str();
		}

		const std::string& nameString() const
		{
			return m_name;
		}

		virtual bool edit(aten::IMaterialParamEditor* editor)
		{
			//AT_ASSERT(false);
			return false;
		}

		static inline AT_DEVICE_MTRL_API aten::vec3 sampleTexture(const int texid, real u, real v, real defaultValue, int lod = 0)
		{
			auto ret = AT_NAME::sampleTexture(texid, u, v, aten::vec3(defaultValue), lod);
			return std::move(ret);
		}

		static uint32_t getMaterialNum();
		static material* getMaterial(uint32_t idx);

		// This is very dangerous, be very careful to use...
		static bool deleteMaterial(material* mtrl, bool needDelete = false);

		static void clearMaterialList();

		// TODO
		// �}�e���A����ID���������Ă���̂ŁA���̊֐��͕s�v.
		static int findMaterialIdx(material* mtrl);

		static const std::vector<material*>& getMaterials();

		static const char* getMaterialTypeName(aten::MaterialType type);
		static std::vector<const char*>& getMaterialTypeName();

	private:
		static int initMaterial(material* mtrl, bool local);
		static void initMaterialTypeName();

	protected:
		int m_id{ 0 };

		aten::MaterialParameter m_param;

		// For debug.
		std::string m_name;
	};

	class NPRMaterial : public material {
	protected:
		NPRMaterial(
			aten::MaterialType type,
			const aten::vec3& e, AT_NAME::Light* light);

		NPRMaterial(aten::MaterialType type, aten::Values& val)
			: material(type, MaterialAttributeNPR, val)
		{}

		virtual ~NPRMaterial() {}

	public:
		virtual AT_DEVICE_MTRL_API real computeFresnel(
			const aten::vec3& normal,
			const aten::vec3& wi,
			const aten::vec3& wo,
			real outsideIor = 1) const override final
		{
			return real(1);
		}

		void setTargetLight(AT_NAME::Light* light);

		const AT_NAME::Light* getTargetLight() const;

		virtual aten::vec3 bsdf(
			real cosShadow,
			real u, real v) const = 0;

	private:
		AT_NAME::Light* m_targetLight{ nullptr };
	};


	real schlick(
		const aten::vec3& in,
		const aten::vec3& normal,
		real ni, real nt);

	real computFresnel(
		const aten::vec3& in,
		const aten::vec3& normal,
		real ni, real nt);
}
