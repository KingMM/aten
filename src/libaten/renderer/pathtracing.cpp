#include "renderer/pathtracing.h"
#include "misc/omputil.h"
#include "misc/timer.h"
#include "sampler/xorshift.h"
#include "sampler/halton.h"
#include "sampler/sobolproxy.h"
#include "sampler/wanghash.h"
#include "sampler/cmj.h"

#include "material/lambert.h"

//#define RELEASE_DEBUG

#ifdef RELEASE_DEBUG
#define BREAK_X	(171)
#define BREAK_Y	(511 - 503)
#pragma optimize( "", off)
#endif

namespace aten
{
	// NOTE
	// https://www.slideshare.net/shocker_0x15/ss-52688052

	PathTracing::Path PathTracing::radiance(
		sampler* sampler,
		const ray& inRay,
		camera* cam,
		CameraSampleResult& camsample,
		scene* scene)
	{
		return radiance(sampler, m_maxDepth, inRay, cam, camsample, scene);
	}

	PathTracing::Path PathTracing::radiance(
		sampler* sampler,
		uint32_t maxDepth,
		const ray& inRay,
		camera* cam,
		CameraSampleResult& camsample,
		scene* scene)
	{
		uint32_t depth = 0;
		uint32_t rrDepth = m_rrDepth;

		Path path;
		path.ray = inRay;

		while (depth < maxDepth) {
			path.rec = hitrecord();

			bool willContinue = true;
			Intersection isect;

			if (scene->hit(path.ray, AT_MATH_EPSILON, AT_MATH_INF, path.rec, isect)) {
				willContinue = shade(sampler, scene, cam, camsample, depth, path);
			}
			else {
				shadeMiss(scene, depth, path);
				willContinue = false;
			}

			if (depth < m_startDepth && !path.isTerminate) {
				path.contrib = vec3(0);
			}

			if (!willContinue) {
				break;
			}

			depth++;
		}

		return std::move(path);
	}

	bool PathTracing::shade(
		sampler* sampler,
		scene* scene,
		camera* cam,
		CameraSampleResult& camsample,
		int depth,
		Path& path)
	{
		lambert voxelMtrl(path.rec.albedo, true);

		uint32_t rrDepth = m_rrDepth;

		material* mtrl = path.rec.isVoxel
			? &voxelMtrl
			: material::getMaterial(path.rec.mtrlid);

		bool isBackfacing = dot(path.rec.normal, -path.ray.dir) < real(0);

		// 交差位置の法線.
		// 物体からのレイの入出を考慮.
		vec3 orienting_normal = path.rec.normal;

		// Implicit conection to light.
		if (mtrl->isEmissive()) {
			if (!isBackfacing) {
				real weight = 1.0f;

				if (depth > 0 && !(path.prevMtrl && path.prevMtrl->isSingular())) {
					auto cosLight = dot(orienting_normal, -path.ray.dir);
					auto dist2 = aten::squared_length(path.rec.p - path.ray.org);

					if (cosLight >= 0) {
						auto pdfLight = 1 / path.rec.area;

						// Convert pdf area to sradian.
						// http://www.slideshare.net/h013/edubpt-v100
						// p31 - p35
						pdfLight = pdfLight * dist2 / cosLight;

						weight = path.pdfb / (pdfLight + path.pdfb);
					}
				}

				path.contrib += path.throughput * weight * mtrl->color();
			}

			path.isTerminate = true;
			return false;
		}

		if (!mtrl->isTranslucent() && isBackfacing) {
			orienting_normal = -orienting_normal;
		}

		// Apply normal map.
		mtrl->applyNormalMap(orienting_normal, orienting_normal, path.rec.u, path.rec.v);
		
		// Explicit conection to light.
		if (!mtrl->isSingular())
		{
			real lightSelectPdf = 1;
			LightSampleResult sampleres;

			auto light = scene->sampleLight(
				path.rec.p,
				orienting_normal,
				sampler,
				lightSelectPdf, sampleres);

			if (light) {
				const vec3& posLight = sampleres.pos;
				const vec3& nmlLight = sampleres.nml;
				real pdfLight = sampleres.pdf;

				auto lightobj = sampleres.obj;

				vec3 dirToLight = normalize(sampleres.dir);

				auto shadowRayOrg = path.rec.p + AT_MATH_EPSILON * orienting_normal;
				auto tmp = path.rec.p + dirToLight - shadowRayOrg;
				auto shadowRayDir = normalize(tmp);
				aten::ray shadowRay(shadowRayOrg, shadowRayDir);

				hitrecord tmpRec;

				if (scene->hitLight(light, posLight, shadowRay, AT_MATH_EPSILON, AT_MATH_INF, tmpRec)) {
					// Shadow ray hits the light.
					auto cosShadow = dot(orienting_normal, dirToLight);

					auto bsdf = mtrl->bsdf(orienting_normal, path.ray.dir, dirToLight, path.rec.u, path.rec.v);
					auto pdfb = mtrl->pdf(orienting_normal, path.ray.dir, dirToLight, path.rec.u, path.rec.v);

					bsdf *= path.throughput;

					// Get light color.
					auto emit = sampleres.finalColor;

					if (light->isSingular() || light->isInfinite()) {
						if (pdfLight > real(0) && cosShadow >= 0) {
							// TODO
							// ジオメトリタームの扱いについて.
							// singular light の場合は、finalColor に距離の除算が含まれている.
							// inifinite light の場合は、無限遠方になり、pdfLightに含まれる距離成分と打ち消しあう？.
							// （打ち消しあうので、pdfLightには距離成分は含んでいない）.
							auto misW = pdfLight / (pdfb + pdfLight);
							path.contrib += (misW * bsdf * emit * cosShadow / pdfLight) / lightSelectPdf;
						}
					}
					else {
						auto cosLight = dot(nmlLight, -dirToLight);

						if (cosShadow >= 0 && cosLight >= 0) {
							auto dist2 = squared_length(sampleres.dir);
							auto G = cosShadow * cosLight / dist2;

							if (pdfb > real(0) && pdfLight > real(0)) {
								// Convert pdf from steradian to area.
								// http://www.slideshare.net/h013/edubpt-v100
								// p31 - p35
								pdfb = pdfb * cosLight / dist2;

								auto misW = pdfLight / (pdfb + pdfLight);

								path.contrib += (misW * (bsdf * emit * G) / pdfLight) / lightSelectPdf;
							}
						}
					}
				}
			}
		}

		real russianProb = real(1);

		if (depth > rrDepth) {
			auto t = normalize(path.throughput);
			auto p = std::max(t.r, std::max(t.g, t.b));

			russianProb = sampler->nextSample();

			if (russianProb >= p) {
				path.contrib = vec3();
				return false;
			}
			else {
				russianProb = p;
			}
		}

		auto sampling = mtrl->sample(path.ray, orienting_normal, path.rec.normal, sampler, path.rec.u, path.rec.v);

		auto nextDir = normalize(sampling.dir);
		auto pdfb = sampling.pdf;
		auto bsdf = sampling.bsdf;

		real c = 1;
		if (!mtrl->isSingular()) {
			// TODO
			// AMDのはabsしているが....
			//c = aten::abs(dot(orienting_normal, nextDir));
			c = dot(orienting_normal, nextDir);
		}

		//if (pdfb > 0) {
		if (pdfb > 0 && c > 0) {
			path.throughput *= bsdf * c / pdfb;
			path.throughput /= russianProb;
		}
		else {
			return false;
		}

		path.prevMtrl = mtrl;

		path.pdfb = pdfb;

		// Make next ray.
		path.ray = aten::ray(path.rec.p, nextDir);

		return true;
	}

	void PathTracing::shadeMiss(
		scene* scene,
		int depth,
		Path& path)
	{
		auto ibl = scene->getIBL();
		if (ibl) {
			if (depth == 0) {
				auto bg = ibl->getEnvMap()->sample(path.ray);
				path.contrib += path.throughput * bg;
				path.isTerminate = true;
			}
			else {
				auto pdfLight = ibl->samplePdf(path.ray);
				auto misW = path.pdfb / (pdfLight + path.pdfb);
				auto emit = ibl->getEnvMap()->sample(path.ray);
				path.contrib += path.throughput * misW * emit;
			}
		}
		else {
			auto bg = sampleBG(path.ray);
			path.contrib += path.throughput * bg;
		}
	}

	static uint32_t frame = 0;

	void PathTracing::render(
		Destination& dst,
		scene* scene,
		camera* camera)
	{
		frame++;

		int width = dst.width;
		int height = dst.height;
		uint32_t samples = dst.sample;

		m_maxDepth = dst.maxDepth;
		m_rrDepth = dst.russianRouletteDepth;
		m_startDepth = dst.startDepth;

		if (m_rrDepth > m_maxDepth) {
			m_rrDepth = m_maxDepth - 1;
		}

		auto time = timer::getSystemTime();

#if defined(ENABLE_OMP) && !defined(RELEASE_DEBUG)
#pragma omp parallel
#endif
		{
			auto idx = OMPUtil::getThreadIdx();

			auto t = timer::getSystemTime();

#if defined(ENABLE_OMP) && !defined(RELEASE_DEBUG)
#pragma omp for
#endif
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					int pos = y * width + x;

					vec3 col = vec3(0);
					vec3 col2 = vec3(0);
					uint32_t cnt = 0;

#ifdef RELEASE_DEBUG
					if (x == BREAK_X && y == BREAK_Y) {
						DEBUG_BREAK();
					}
#endif

					for (uint32_t i = 0; i < samples; i++) {
						auto scramble = aten::getRandom(pos) * 0x1fe3434f;

						//XorShift rnd(scramble + t.milliSeconds);
						//Halton rnd(scramble + t.milliSeconds);
						//Sobol rnd;
						CMJ rnd;
						rnd.init(frame, i, scramble);
						//WangHash rnd(scramble + t.milliSeconds);

						real u = real(x + rnd.nextSample()) / real(width);
						real v = real(y + rnd.nextSample()) / real(height);

						auto camsample = camera->sample(u, v, &rnd);

						auto ray = camsample.r;

						auto path = radiance(
							&rnd,
							ray, 
							camera,
							camsample,
							scene);

						if (isInvalidColor(path.contrib)) {
							AT_PRINTF("Invalid(%d/%d[%d])\n", x, y, i);
							continue;
						}

						auto pdfOnImageSensor = camsample.pdfOnImageSensor;
						auto pdfOnLens = camsample.pdfOnLens;

						auto s = camera->getSensitivity(
							camsample.posOnImageSensor,
							camsample.posOnLens);

						auto c = path.contrib * s / (pdfOnImageSensor * pdfOnLens);

						col += c;
						col2 += c * c;
						cnt++;

						if (path.isTerminate) {
							break;
						}
					}

					col /= (real)cnt;

					dst.buffer->put(x, y, vec4(col, 1));

					if (dst.variance) {
						col2 /= (real)cnt;
						dst.variance->put(x, y, vec4(col2 - col * col, real(1)));
					}
				}
			}
		}
	}
}
