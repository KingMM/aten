#pragma once

#include "light/light.h"

namespace aten {
	class PointLight : public Light {
	public:
		PointLight() {}
		PointLight(
			const vec3& pos,
			const vec3& le,
			real constAttn = 1,
			real linearAttn = 0,
			real expAttn = 0)
		{
			m_param.pos = pos;
			m_param.le = le;

			setAttenuation(constAttn, linearAttn, expAttn);
		}

		PointLight(Values& val)
			: Light(val)
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
			LightSampleResult result;

			result.pos = m_param.pos;
			result.pdf = real(1);
			result.dir = m_param.pos - org;
			result.nml = vec3();	// Not used...

			auto dist2 = result.dir.squared_length();
			auto dist = aten::sqrt(dist2);

			// ������.
			// http://ogldev.atspace.co.uk/www/tutorial20/tutorial20.html
			// ��L�ɂ��ƁAL = Le / dist2 �Ő��������A3D�O���t�B�b�N�X�ł͌����ړI�ɂ��܂��낵���Ȃ��̂ŁA���������g���Čv�Z����.
			real attn = m_param.constAttn + m_param.linearAttn * dist + m_param.expAttn * dist2;

			// TODO
			// Is it correct?
			attn = std::max(attn, real(1));
			
			result.le = m_param.le;
			result.intensity = 1 / attn;
			result.finalColor = m_param.le / attn;

			return std::move(result);
		}
	};
}