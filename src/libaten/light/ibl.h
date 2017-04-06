#pragma once

#include <vector>
#include "light/light.h"
#include "renderer/envmap.h"

namespace aten {
	class ImageBasedLight : public Light {
	public:
		ImageBasedLight() {}
		ImageBasedLight(envmap* envmap)
		{
			setEnvMap(envmap);
		}

		ImageBasedLight(Values& val)
			: Light(val)
		{
			texture* tex = (texture*)val.get("envmap", nullptr);
			
			envmap* bg = new envmap();
			bg->init(tex);

			setEnvMap(bg);
		}

		virtual ~ImageBasedLight() {}

	public:
		void setEnvMap(envmap* envmap)
		{
			if (m_envmap != envmap) {
				m_envmap = envmap;
				preCompute();
			}
		}

		envmap* getEnvMap()
		{
			return m_envmap;
		}

		virtual real samplePdf(const ray& r) const override final;

		virtual LightSampleResult sample(const vec3& org, sampler* sampler) const override final;

		virtual bool isSingular() const
		{
			return false;
		}

		virtual bool isInifinite() const override final
		{
			return true;
		}

		virtual bool isIBL() const override final
		{
			return true;
		}

	private:
		void preCompute();

	private:
		envmap* m_envmap{ nullptr };
		real m_avgIllum{ real(0) };

		// v������cdf(cumulative distribution function = �ݐϕ��z�֐� = sum of pdf).
		std::vector<real> m_cdfV;

		// u������cdf(cumulative distribution function = �ݐϕ��z�֐� = sum of pdf).
		// �񂲂Ƃɕێ�����.
		std::vector<std::vector<real>> m_cdfU;
	};
}