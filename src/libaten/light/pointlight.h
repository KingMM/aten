#pragma once

#include "light/light.h"

namespace aten {
	class PointLight : public Light {
	public:
		PointLight()
			: Light(LightTypeSingluar)
		{}
		PointLight(
			const vec3& pos,
			const vec3& le,
			real constAttn = 1,
			real linearAttn = 0,
			real expAttn = 0)
			: Light(LightTypeSingluar)
		{
			m_param.pos = pos;
			m_param.le = le;

			setAttenuation(constAttn, linearAttn, expAttn);
		}

		PointLight(Values& val)
			: Light(LightTypeSingluar, val)
		{
			m_param.constAttn = val.get("constAttn", m_param.constAttn);
			m_param.linearAttn = val.get("linearAttn", m_param.linearAttn);
			m_param.expAttn = val.get("expAttn", m_param.expAttn);

			setAttenuation(m_param.constAttn, m_param.linearAttn, m_param.expAttn);
		}

		virtual ~PointLight() {}

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

		virtual LightSampleResult sample(const vec3& org, sampler* sampler) const override final
		{
			return std::move(sample(m_param, org, sampler));
		}

		static LightSampleResult sample(
			const LightParameter& param,
			const vec3& org,
			sampler* sampler)
		{
			LightSampleResult result;

			result.pos = param.pos;
			result.pdf = real(1);
			result.dir = param.pos - org;
			result.nml = vec3();	// Not used...

			auto dist2 = result.dir.squared_length();
			auto dist = aten::sqrt(dist2);

			// ������.
			// http://ogldev.atspace.co.uk/www/tutorial20/tutorial20.html
			// ��L�ɂ��ƁAL = Le / dist2 �Ő��������A3D�O���t�B�b�N�X�ł͌����ړI�ɂ��܂��낵���Ȃ��̂ŁA���������g���Čv�Z����.
			real attn = param.constAttn + param.linearAttn * dist + param.expAttn * dist2;

			// TODO
			// Is it correct?
			attn = std::max(attn, real(1));
			
			result.le = param.le;
			result.intensity = 1 / attn;
			result.finalColor = param.le / attn;

			return std::move(result);
		}
	};
}