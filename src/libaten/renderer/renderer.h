#pragma once

#include "types.h"
#include "math/vec3.h"
#include "renderer/background.h"

namespace aten
{
	struct Destination {
		int width{ 0 };
		int height{ 0 };
		uint32_t maxDepth{ 1 };
		uint32_t russianRouletteDepth{ 1 };
		uint32_t sample{ 1 };
		uint32_t mutation{ 1 };
		vec3* buffer{ nullptr };
	};

	class scene;
	class camera;

	class Renderer {
	protected:
		Renderer() {}
		virtual ~Renderer() {}

	public:
		virtual void render(
			Destination& dst,
			scene* scene,
			camera* camera) = 0;

		void setBG(background* bg)
		{
			m_bg = bg;
		}

	protected:
		virtual vec3 sampleBG(const ray& inRay) const
		{
			if (m_bg) {
				return m_bg->sample(inRay);
			}
			return std::move(vec3());
		}

		background* bg()
		{
			return m_bg;
		}

	private:
		background* m_bg{ nullptr };
	};
}