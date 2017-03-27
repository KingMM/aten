#pragma once

#include "visualizer/visualizer.h"
#include "visualizer/blitter.h"

namespace aten {
	class PracticalNoiseReduction : public visualizer::PreProc{
	public:
		PracticalNoiseReduction() {}

		virtual ~PracticalNoiseReduction() {}

	public:
		virtual void operator()(
			const vec4* src,
			uint32_t width, uint32_t height,
			vec4* dst) override final;

		void setBuffers(
			vec4* direct,
			vec4* indirect,
			vec4* varIndirect,
			vec4* nml_depth)
		{
			m_direct = direct;
			m_indirect = indirect;
			m_variance = varIndirect;
			m_nml_depth = nml_depth;
		}

		void setParams(
			real std_dev_S,
			real std_dev_C,
			real std_dev_D,
			real threshold)
		{
			m_stdDevS = std_dev_S;
			m_stdDevC = std_dev_S;
			m_stdDevD = std_dev_S;

			m_threshold = threshold;
		}

	private:
		vec4* m_direct{ nullptr };

		vec4* m_indirect{ nullptr };
		vec4* m_variance{ nullptr };

		vec4* m_nml_depth{ nullptr };

		real m_stdDevS{ 8 };		// standard deviation for spatial.
		real m_stdDevC{ 0.5 };		// standard deviation for color.
		real m_stdDevD{ 2 };		// standard deviation for depth.
		real m_threshold{ 0.1 };
	};
}
