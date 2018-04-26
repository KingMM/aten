#pragma once

#include "aten4idaten.h"
#include "cuda/cudamemory.h"
#include "cuda/cudaGLresource.h"
#include "cuda/cudaTextureResource.h"
#include "kernel/compaction.h"

namespace idaten
{
	struct TileDomain {
		int x;
		int y;
		int w;
		int h;

		TileDomain() {}
		TileDomain(int _x, int _y, int _w, int _h)
			: x(_x), y(_y), w(_w), h(_h)
		{}
	};

	struct EnvmapResource {
		int idx{ -1 };
		real avgIllum;
		real multiplyer{ real(1) };

		EnvmapResource() {}

		EnvmapResource(int i, real illum, real mul = real(1))
			: idx(i), avgIllum(illum), multiplyer(mul)
		{}
	};

	class Renderer {
	protected:
		Renderer() {}
		~Renderer() {}

	public:
		virtual void render(
			const TileDomain& tileDomain,
			int maxSamples,
			int maxBounce) = 0;

		virtual void update(
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
			const EnvmapResource& envmapRsc);

		virtual void reset() {}

		void updateCamera(const aten::CameraParameter& camera);

		virtual void enableRenderAOV(
			GLuint gltexPosition,
			GLuint gltexNormal,
			GLuint gltexAlbedo,
			const aten::vec3& posRange)
		{
			// Nothing is done...
		}

		std::vector<idaten::CudaTextureResource>& getCudaTextureResourceForBvhNodes()
		{
			return m_nodeparam;
		}

		idaten::CudaTextureResource getCudaTextureResourceForVtxPos()
		{
			return m_vtxparamsPos;
		}

		idaten::Compaction& getCompaction()
		{
			return m_compaction;
		}

	protected:
		idaten::Compaction m_compaction;

		idaten::CudaMemory m_dst;
		idaten::TypedCudaMemory<aten::CameraParameter> m_cam;
		idaten::TypedCudaMemory<aten::GeomParameter> m_shapeparam;
		idaten::TypedCudaMemory<aten::MaterialParameter> m_mtrlparam;
		idaten::TypedCudaMemory<aten::LightParameter> m_lightparam;
		idaten::TypedCudaMemory<aten::PrimitiveParamter> m_primparams;

		idaten::TypedCudaMemory<aten::mat4> m_mtxparams;
		
		std::vector<idaten::CudaTextureResource> m_nodeparam;
		idaten::TypedCudaMemory<cudaTextureObject_t> m_nodetex;

		std::vector<idaten::CudaTexture> m_texRsc;
		idaten::TypedCudaMemory<cudaTextureObject_t> m_tex;
		EnvmapResource m_envmapRsc;

		idaten::CudaGLSurface m_glimg;
		idaten::CudaTextureResource m_vtxparamsPos;
		idaten::CudaTextureResource m_vtxparamsNml;

		aten::CameraParameter m_camParam;
	};
}
