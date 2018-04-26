#pragma once

#include "multigpu/renderer_multigpu.h"
#include "svgf/svgf.h"

namespace idaten
{
	template<class T> class GpuProxy;

	class SVGFPathTracingMultiGPU : public RendererMultiGPU<SVGFPathTracing> {
		friend class GpuProxy<SVGFPathTracingMultiGPU>;

	public:
		SVGFPathTracingMultiGPU() {}
		virtual ~SVGFPathTracingMultiGPU() {}

	public:
		virtual void render(
			const TileDomain& tileDomain,
			int maxSamples,
			int maxBounce) override final;

		virtual void postRender(int width, int height) override final;

	protected:
		void copyFrom(SVGFPathTracingMultiGPU& tracer);

	private:
		void onRender(
			const TileDomain& tileDomain,
			int width, int height,
			int maxSamples,
			int maxBounce,
			cudaSurfaceObject_t outputSurf,
			cudaTextureObject_t vtxTexPos,
			cudaTextureObject_t vtxTexNml);
	};
}
