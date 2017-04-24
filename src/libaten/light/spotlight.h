#pragma once

#include "light/light.h"

namespace AT_NAME {
	class SpotLight : public Light {
	public:
		SpotLight()
			: Light(aten::LightType::Spot, LightAttributeSingluar)
		{}
		SpotLight(
			const aten::vec3& pos,	// light position.
			const aten::vec3& dir,	// light direction from the position.
			const aten::vec3& le,		// light color.
			real constAttn,
			real linearAttn,
			real expAttn,
			real innerAngle,	// Umbra angle of spotlight in radians.
			real outerAngle,	// Penumbra angle of spotlight in radians.
			real falloff)		// Falloff factor.
			: Light(aten::LightType::Spot, LightAttributeSingluar)
		{
			m_param.pos = pos;
			m_param.dir = aten::normalize(dir);
			m_param.le = le;

			setAttenuation(constAttn, linearAttn, expAttn);
			setSpotlightFactor(innerAngle, outerAngle, falloff);
		}

		SpotLight(aten::Values& val)
			: Light(aten::LightType::Spot, LightAttributeSingluar, val)
		{
			m_param.constAttn = val.get("constAttn", m_param.constAttn);
			m_param.linearAttn = val.get("linearAttn", m_param.linearAttn);
			m_param.expAttn = val.get("expAttn", m_param.expAttn);

			setAttenuation(m_param.constAttn, m_param.linearAttn, m_param.expAttn);

			m_param.innerAngle = val.get("innerAngle", m_param.innerAngle);
			m_param.outerAngle = val.get("outerAngle", m_param.outerAngle);
			m_param.falloff = val.get("falloff", m_param.falloff);

			setSpotlightFactor(m_param.innerAngle, m_param.outerAngle, m_param.falloff);
		}

		virtual ~SpotLight() {}

	public:
		void setAttenuation(
			real constAttn,
			real linearAttn,
			real expAttn)
		{
			m_param.constAttn = std::max(constAttn, real(0));
			m_param.linearAttn = std::max(linearAttn, real(0));
			m_param.expAttn = std::max(expAttn, real(0));
		}

		void setSpotlightFactor(
			real innerAngle,	// Umbra angle of spotlight in radians.
			real outerAngle,	// Penumbra angle of spotlight in radians.
			real falloff)		// Falloff factor.
		{
			m_param.innerAngle = aten::clamp<real>(innerAngle, 0, AT_MATH_PI - AT_MATH_EPSILON);
			m_param.outerAngle = aten::clamp<real>(outerAngle, innerAngle, AT_MATH_PI - AT_MATH_EPSILON);
			m_param.falloff = falloff;
		}

		virtual aten::LightSampleResult sample(const aten::vec3& org, aten::sampler* sampler) const override final
		{
			return std::move(sample(m_param, org, sampler));
		}

		static AT_DEVICE_API aten::LightSampleResult sample(
			const aten::LightParameter& param,
			const aten::vec3& org,
			aten::sampler* sampler)
		{
			aten::LightSampleResult result;

			result.pos = param.pos;
			result.pdf = real(1);
			result.dir = param.pos - org;
			result.nml = aten::vec3();	// Not used...

			// NOTE
			// https://msdn.microsoft.com/ja-jp/library/bb172279(v=vs.85).aspx

			auto lightdir = normalize(result.dir);

			auto rho = dot(-param.dir, lightdir);

			auto cosHalfTheta = aten::cos(param.innerAngle * real(0.5));
			auto cosHalfPhi = aten::cos(param.outerAngle * real(0.5));

			real spot = 0;

			if (rho > cosHalfTheta) {
				// �{�e���ɓ����Ă��� -> �ő�����C�g�̉e�����󂯂�.
				spot = 1;
			}
			else if (rho <= cosHalfPhi) {
				// ���e�O�ɏo�Ă��� -> ���C�g�̉e����S���󂯂Ȃ�.
				spot = 0;
			}
			else {
				// �{�e�̊O�A���e�̒�.
				spot = (rho - cosHalfPhi) / (cosHalfTheta - cosHalfPhi);
				spot = aten::pow(spot, param.falloff);
			}

			// ������.
			// http://ogldev.atspace.co.uk/www/tutorial20/tutorial20.html
			// ��L�ɂ��ƁAL = Le / dist2 �Ő��������A3D�O���t�B�b�N�X�ł͌����ړI�ɂ��܂��낵���Ȃ��̂ŁA���������g���Čv�Z����.
			auto dist2 = result.dir.squared_length();
			auto dist = aten::sqrt(dist2);
			real attn = param.constAttn + param.linearAttn * dist + param.expAttn * dist2;

			// TODO
			// Is it correct?
			attn = aten::cmpMax(attn, real(1));
			
			result.le = param.le;
			result.intensity = spot / attn;
			result.finalColor = param.le * spot / attn;

			return std::move(result);
		}
	};
}