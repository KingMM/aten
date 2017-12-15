#pragma once

#include "aten4idaten.h"
#include "cuda/cudamemory.h"
#include "cuda/cudaGLresource.h"

#include "kernel/renderer.h"

namespace idaten
{
	class SVGFPathTracing : public Renderer {
	public:
		enum Mode {
			SVGF,			// Spatio-temporal Variance Guided Filter.
			TF,				// Temporal Filter.
			PT,				// Path Tracing.
			VAR,			// Variance (For debug).
			AOVar,			// Arbitrary Output Variables.
		};

		enum AOVMode {
			Normal,
			TexColor,
			Depth,
			WireFrame,
			BaryCentric,
		};

		static const int ShdowRayNum = 2;

#ifdef __AT_CUDA__
		struct Path {
			aten::vec3 throughput;
			aten::vec3 contrib;
			aten::sampler sampler;

			real pdfb;
			int samples;

			bool isHit;
			bool isTerminate;
			bool isSingular;
			bool isKill;
		};
		C_ASSERT((sizeof(Path) % 4) == 0);

		struct ShadowRay {
			aten::ray ray[ShdowRayNum];
			aten::vec3 lightcontrib[ShdowRayNum];
			real distToLight[ShdowRayNum];
			int targetLightId[ShdowRayNum];

			struct {
				uint32_t isActive : 1;
			};
		};
#else
		struct Path;
		struct ShadowRay;
#endif

		struct PickedInfo {
			int ix{ -1 };
			int iy{ -1 };
			aten::vec3 color;
			aten::vec3 normal;
			float depth;
			int meshid;
			int triid;
			int mtrlid;
		};

	public:
		SVGFPathTracing() {}
		virtual ~SVGFPathTracing() {}

	public:
		virtual void render(
			int width, int height,
			int maxSamples,
			int maxBounce) override final;

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
			const EnvmapResource& envmapRsc) override;

		void setAovExportBuffer(GLuint gltexId);

		void setGBuffer(GLuint gltexGbuffer);

		Mode getMode() const
		{
			return m_mode;
		}
		void setMode(Mode mode)
		{
			auto prev = m_mode;
			m_mode = mode;
			if (prev != m_mode) {
				reset();
			}
		}

		AOVMode getAOVMode() const
		{
			return m_aovMode;
		}
		void setAOVMode(AOVMode mode)
		{
			m_aovMode = mode;
		}

		virtual void reset() override final
		{
			m_frame = 1;
			m_curAOVPos = 0;
		}

		uint32_t frame() const
		{
			return m_frame;
		}

		void willPickPixel(int ix, int iy)
		{
			m_willPicklPixel = true;
			m_pickedInfo.ix = ix;
			m_pickedInfo.iy = iy;
		}

		bool getPickedPixelInfo(PickedInfo& ret)
		{
			bool isValid = (m_pickedInfo.ix >= 0);

			ret = m_pickedInfo;

			m_pickedInfo.ix = -1;
			m_pickedInfo.iy = -1;

			return isValid;
		}

		void setTemporalWeightThreshold(float th)
		{
			m_thresholdTemporalWeight = aten::clamp(th, 0.0f, 1.0f);
		}

		float getTemporalWeightThreshold() const
		{
			return m_thresholdTemporalWeight;
		}

		void setAtrousTapRadiusScale(int s)
		{
			m_atrousTapRadiusScale = std::max(s, 1);
		}

		int getAtrousTapRadiusScale() const
		{
			return m_atrousTapRadiusScale;
		}

		void setHitDistanceLimit(float d)
		{
			m_hitDistLimit = d;
		}

	protected:
		enum Resolution {
			Hi,
			Low,
			Num
		};

		virtual void onGenPath(
			int width, int height,
			int sample, int maxSamples,
			int seed,
			cudaTextureObject_t texVtxPos,
			cudaTextureObject_t texVtxNml);

		virtual void onHitTest(
			Resolution resType,
			int width, int height,
			int bounce,
			cudaTextureObject_t texVtxPos);

		virtual void onScreenSpaceHitTest(
			int width, int height,
			int bounce,
			cudaTextureObject_t texVtxPos);

		virtual void onShadeMiss(
			Resolution resType,
			int width, int height,
			int bounce,
			cudaSurfaceObject_t aovExportBuffer);

		virtual void onShade(
			Resolution resType,
			cudaSurfaceObject_t outputSurf,
			cudaSurfaceObject_t aovExportBuffer,
			int hitcount,
			int width, int height,
			int bounce, int rrBounce,
			cudaTextureObject_t texVtxPos,
			cudaTextureObject_t texVtxNml);

		virtual void onGather(
			Resolution resType,
			cudaSurfaceObject_t outputSurf,
			int width, int height,
			int maxSamples);

		void onTemporalReprojection(
			Resolution resType,
			cudaSurfaceObject_t outputSurf,
			int width, int height);

		void onVarianceEstimation(
			Resolution resType,
			cudaSurfaceObject_t outputSurf,
			int width, int height);

		void onAtrousFilter(
			Resolution resType,
			cudaSurfaceObject_t outputSurf,
			int width, int height);

		void coarseBuffer(int width, int height);

		void upsamplingAndMerge(
			cudaSurfaceObject_t outputSurf,
			int width, int height);

		void onFillAOV(
			cudaSurfaceObject_t outputSurf,
			int width, int height);

		void pick(
			int ix, int iy,
			int width, int height,
			cudaTextureObject_t texVtxPos);

		int getCurAovs()
		{
			return m_curAOVPos;
		}
		int getPrevAovs()
		{
			return 1 - m_curAOVPos;
		}

		bool isFirstFrame() const
		{
			return (m_frame == 1);
		}

	protected:
		idaten::TypedCudaMemory<Path> m_paths[Resolution::Num];
		idaten::TypedCudaMemory<aten::Intersection> m_isects;
		idaten::TypedCudaMemory<aten::ray> m_rays;

		idaten::TypedCudaMemory<int> m_hitbools;
		idaten::TypedCudaMemory<int> m_hitidx;

		idaten::TypedCudaMemory<ShadowRay> m_shadowRays;

		idaten::TypedCudaMemory<unsigned int> m_sobolMatrices;
		idaten::TypedCudaMemory<unsigned int> m_random;

		// Current AOV buffer position.
		int m_curAOVPos{ 0 };

		// AOV buffer. Current frame and previous frame.
		idaten::TypedCudaMemory<float4> m_aovNormalDepth[Resolution::Num][2];
		idaten::TypedCudaMemory<float4> m_aovTexclrTemporalWeight[Resolution::Num][2];
		idaten::TypedCudaMemory<float4> m_aovColorVariance[Resolution::Num][2];
		idaten::TypedCudaMemory<float4> m_aovMomentMeshid[Resolution::Num][2];

		idaten::TypedCudaMemory<int> m_boolsSingular;
		idaten::TypedCudaMemory<int> m_idxSingular;

		aten::mat4 m_mtxW2V;		// World - View.
		aten::mat4 m_mtxV2C;		// View - Clip.
		aten::mat4 m_mtxC2V;		// Clip - View.

		// View - World.
		aten::mat4 m_mtxV2W;
		aten::mat4 m_mtxPrevW2V;

		idaten::TypedCudaMemory<aten::mat4> m_mtxs;

		unsigned int m_frame{ 1 };

		// For A-trous wavelet.
		idaten::TypedCudaMemory<float4> m_atrousClrVar[2];

		idaten::TypedCudaMemory<float4> m_tmpBuf;

		float m_thresholdTemporalWeight{ 0.0f };
		int m_atrousTapRadiusScale{ 1 };

		// Distance limitation to kill path.
		float m_hitDistLimit{ AT_MATH_INF };

		// AOV buffer to use in OpenGL.
		idaten::CudaGLSurface m_aovGLBuffer;

		// G-Buffer rendered by OpenGL.
		idaten::CudaGLSurface m_gbuffer;

		idaten::TypedCudaMemory<PickedInfo> m_pick;

		bool m_willPicklPixel{ false };
		PickedInfo m_pickedInfo;

		Mode m_mode{ Mode::SVGF };
		AOVMode m_aovMode{ AOVMode::WireFrame };
	};
}
