#include "svgf/svgf.h"

#include "kernel/compaction.h"

#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include "cuda/helper_math.h"
#include "cuda/cudautil.h"
#include "cuda/cudamemory.h"

#include "aten4idaten.h"

namespace idaten
{
	void SVGFPathTracing::update(
		GLuint gltex,
		int width, int height,
		const aten::CameraParameter& camera,
		const std::vector<aten::GeomParameter>& shapes,
		const std::vector<aten::MaterialParameter>& mtrls,
		const std::vector<aten::LightParameter>& lights,
		const std::vector<std::vector<aten::GPUBvhNode>>& nodes,
		const std::vector<aten::PrimitiveParamter>& prims,
		const std::vector<aten::vertex>& vtxs,
		const std::vector<aten::mat4>& mtxs,
		const std::vector<TextureResource>& texs,
		const EnvmapResource& envmapRsc)
	{
		idaten::Renderer::update(
			gltex,
			width, height,
			camera,
			shapes,
			mtrls,
			lights,
			nodes,
			prims,
			vtxs,
			mtxs,
			texs, envmapRsc);

		m_hitbools.init(width * height);
		m_hitidx.init(width * height);

		m_sobolMatrices.init(AT_COUNTOF(sobol::Matrices::matrices));
		m_sobolMatrices.writeByNum(sobol::Matrices::matrices, m_sobolMatrices.maxNum());

		auto& r = aten::getRandom();
		m_random.init(width * height);
		m_random.writeByNum(&r[0], width * height);

		for (int i = 0; i < 2; i++) {
			m_aovNormalDepth[i].init(width * height);
			m_aovTexclrTemporalWeight[i].init(width * height);
			m_aovColorVariance[i].init(width * height);
			m_aovMomentMeshid[i].init(width * height);
		}

		for (int i = 0; i < AT_COUNTOF(m_atrousClrVar); i++) {
			m_atrousClrVar[i].init(width * height);
		}

		m_tmpBuf.init(width * height);
	}

	void SVGFPathTracing::update(
		const std::vector<std::vector<aten::GPUBvhNode>>& nodes,
		const std::vector<aten::mat4>& mtxs)
	{
		// Only for top layer...
		m_nodeparam[0].init(
			(aten::vec4*)&nodes[0][0], 
			sizeof(aten::GPUBvhNode) / sizeof(float4), 
			nodes[0].size());

		if (!mtxs.empty()) {
			m_mtxparams.writeByNum(&mtxs[0], mtxs.size());
			m_mtxparams.reset();
		}
	}

	void SVGFPathTracing::setAovExportBuffer(GLuint gltexId)
	{
		m_aovGLBuffer.init(gltexId, CudaGLRscRegisterType::WriteOnly);
	}

	void SVGFPathTracing::setGBuffer(GLuint gltexGbuffer)
	{
		m_gbuffer.init(gltexGbuffer, idaten::CudaGLRscRegisterType::ReadOnly);
	}

	static bool doneSetStackSize = false;

	void SVGFPathTracing::render(
		int width, int height,
		int maxSamples,
		int maxBounce)
	{
#ifdef __AT_DEBUG__
		if (!doneSetStackSize) {
			size_t val = 0;
			cudaThreadGetLimit(&val, cudaLimitStackSize);
			cudaThreadSetLimit(cudaLimitStackSize, val * 4);
			doneSetStackSize = true;
		}
#endif

		int bounce = 0;

		m_paths.init(width * height);
		m_isects.init(width * height);
		m_rays.init(width * height);

		m_shadowRays.init(width * height);

		cudaMemset(m_paths.ptr(), 0, m_paths.bytes());

		CudaGLResourceMap rscmap(&m_glimg);
		auto outputSurf = m_glimg.bind();

		auto vtxTexPos = m_vtxparamsPos.bind();
		auto vtxTexNml = m_vtxparamsNml.bind();

		{
			std::vector<cudaTextureObject_t> tmp;
			for (int i = 0; i < m_nodeparam.size(); i++) {
				auto nodeTex = m_nodeparam[i].bind();
				tmp.push_back(nodeTex);
			}
			m_nodetex.writeByNum(&tmp[0], tmp.size());
		}

		if (!m_texRsc.empty())
		{
			std::vector<cudaTextureObject_t> tmp;
			for (int i = 0; i < m_texRsc.size(); i++) {
				auto cudaTex = m_texRsc[i].bind();
				tmp.push_back(cudaTex);
			}
			m_tex.writeByNum(&tmp[0], tmp.size());
		}

		cudaSurfaceObject_t aovExportBuffer = 0;
		if (m_aovGLBuffer.isValid()) {
			m_aovGLBuffer.map();
			aovExportBuffer = m_aovGLBuffer.bind();
		}

		static const int rrBounce = 3;

		// Set bounce count to 1 forcibly, aov render mode.
		maxBounce = (m_mode == Mode::AOVar ? 1 : maxBounce);

		auto time = AT_NAME::timer::getSystemTime();

		for (int i = 0; i < maxSamples; i++) {
			int seed = time.milliSeconds;
			//int seed = 0;

			onGenPath(
				width, height,
				i, maxSamples,
				seed,
				vtxTexPos,
				vtxTexNml);

			bounce = 0;

			while (bounce < maxBounce) {
				onHitTest(
					width, height,
					bounce,
					vtxTexPos);
				
				onShadeMiss(width, height, bounce, aovExportBuffer);

				int hitcount = 0;
				idaten::Compaction::compact(
					m_hitidx,
					m_hitbools,
					&hitcount);

				//AT_PRINTF("%d\n", hitcount);

				if (hitcount == 0) {
					break;
				}

				onShade(
					outputSurf,
					aovExportBuffer,
					hitcount,
					width, height,
					bounce, rrBounce,
					vtxTexPos, vtxTexNml);

				bounce++;
			}
		}

		onGather(outputSurf, width, height, maxSamples);

		if (m_mode == Mode::SVGF)
		{
			onVarianceEstimation(outputSurf, width, height);

			onAtrousFilter(outputSurf, width, height);

			copyFromTmpBufferToAov(width, height);
		}
		else if (m_mode == Mode::VAR) {
			onVarianceEstimation(outputSurf, width, height);
		}

		pick(
			m_pickedInfo.ix, m_pickedInfo.iy, 
			width, height,
			vtxTexPos);

		//checkCudaErrors(cudaDeviceSynchronize());

		// Toggle aov buffer pos.
		m_curAOVPos = 1 - m_curAOVPos;

		m_frame++;

		{
			m_vtxparamsPos.unbind();
			m_vtxparamsNml.unbind();

			for (int i = 0; i < m_nodeparam.size(); i++) {
				m_nodeparam[i].unbind();
			}
			m_nodetex.reset();

			for (int i = 0; i < m_texRsc.size(); i++) {
				m_texRsc[i].unbind();
			}
			m_tex.reset();
		}

		if (m_aovGLBuffer.isValid()) {
			m_aovGLBuffer.unbind();
			m_aovGLBuffer.unmap();
		}
	}
}
