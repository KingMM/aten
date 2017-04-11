#include "renderer/bdpt2.h"
#include "misc/thread.h"
#include "misc/timer.h"
#include "material/lambert.h"
#include "sampler/xorshift.h"
#include "sampler/halton.h"
#include "sampler/sobolproxy.h"
#include "sampler/UniformDistributionSampler.h"

//#define BDPT_DEBUG

#ifdef BDPT_DEBUG
#pragma optimize( "", off) 
#endif

namespace aten
{
	static inline real russianRoulette(const vec3& v)
	{
		real p = std::max(v.r, std::max(v.g, v.b));

		if (p == real(0)) {
			return real(1);
		}

		auto t = normalize(v);
		p = std::max(t.r, std::max(t.g, t.b));

		return p;
	}

	BDPT2::Result BDPT2::genEyePath(
		std::vector<Vertex>& vs,
		int x, int y,
		sampler* sampler,
		scene* scene,
		camera* camera) const
	{
		real u = (real)(x + sampler->nextSample()) / (real)m_width;
		real v = (real)(y + sampler->nextSample()) / (real)m_height;

		auto camsample = camera->sample(u, v, sampler);

		// �����Y��̒��_�ix0�j�𒸓_���X�g�ɒǉ�.
		vs.push_back(Vertex(
			camsample.posOnLens,
			camsample.nmlOnLens,
			camsample.nmlOnLens,
			ObjectType::Lens,
			camsample.pdfOnLens,
			vec3(1),
			vec3(0),
			nullptr,
			nullptr,
			0, 0));

		int depth = 0;

		ray ray = camsample.r;

		vec3 throughput(1);
		real totalAreaPdf = camsample.pdfOnLens;

		vec3 prevNormal = camera->getDir();
		real sampledPdf = real(1);

		while (depth < m_maxDepth) {
			hitrecord rec;
			if (!scene->hit(ray, AT_MATH_EPSILON, AT_MATH_INF, rec)) {
				break;
			}

			// �����ʒu�̖@��.
			// ���̂���̃��C�̓��o���l��.
			vec3 orienting_normal = dot(rec.normal, ray.dir) < 0.0 ? rec.normal : -rec.normal;

			// ���V�A�����[���b�g�ɂ���āA�V�������_���u���ۂɁv�T���v�����O���A��������̂��ǂ��������肷��.
			auto rrProb = russianRoulette(throughput);
			auto rr = sampler->nextSample();
			if (rr >= rrProb) {
				break;
			}

			// �V�������_���T���v�����O���ꂽ�̂ŁA�g�[�^���̊m�����x�ɏ�Z����.
			totalAreaPdf *= rrProb;

			const vec3 toNextVtx = ray.org - rec.p;

			if (depth == 0) {
				// x1�̃T���v�����O�m�����x�̓C���[�W�Z���T��̃T���v�����O�m�����x��ϊ����邱�Ƃŋ��߂�,
				auto pdfOnImageSensor = camera->getPdfImageSensorArea(
					rec.p,
					orienting_normal,
					camsample.posOnImageSensor,
					camsample.posOnLens,
					camsample.posOnObjectplane);

				totalAreaPdf *= pdfOnImageSensor;

				// �􉽓I�ȌW���v�Z + �Z���T�[�Z���V�e�B�r�e�B�̍����v�Z����
				// x1 -> x0�ւ̕��ˋP�x���ŏI�I�ɃC���[�W�Z���T�ɗ^�����^�x
				auto W_dash = camera->getWdash(
					rec.p,
					orienting_normal,
					camsample.posOnImageSensor,
					camsample.posOnLens,
					camsample.posOnObjectplane);

				throughput = W_dash * throughput;
			}
			else {
				// �V�������_���T���v�����O���邽�߂̊m�����x�֐��͗��̊p���x�Ɋւ�����̂ł��������߁A�����ʐϑ��x�Ɋւ���m�����x�֐��ɕϊ�����.
				const real c = dot(normalize(toNextVtx), orienting_normal);
				const real dist2 = toNextVtx.squared_length();
				const real areaPdf = sampledPdf * (c / dist2);

				totalAreaPdf *= areaPdf;
			}

			// �W�I���g���^�[��.
			const real c0 = dot(normalize(toNextVtx), orienting_normal);
			const real c1 = dot(normalize(-toNextVtx), prevNormal);
			const real dist2 = toNextVtx.squared_length();
			const double G = c0 * c1 / dist2;
			throughput = G * throughput;

			// �����Ƀq�b�g�����炻���ŒǐՏI��.
			if (rec.mtrl->isEmissive()) {
				vec3 bsdf = lambert::bsdf(rec.mtrl, rec.u, rec.v);

				vs.push_back(Vertex(
					rec.p,
					rec.normal,
					orienting_normal,
					ObjectType::Light,
					totalAreaPdf,
					throughput,
					bsdf,
					rec.obj,
					rec.mtrl,
					rec.u, rec.v));

				vec3 emit = rec.mtrl->color();
				vec3 contrib = throughput * emit / totalAreaPdf;

				return std::move(Result(contrib, x, y, true));
			}

			auto sampling = rec.mtrl->sample(ray, orienting_normal, rec, sampler, rec.u, rec.v);

			// �V�������_�𒸓_���X�g�ɒǉ�����.
			vs.push_back(Vertex(
				rec.p,
				rec.normal,
				orienting_normal,
				ObjectType::Object,
				totalAreaPdf,
				throughput,
				sampling.bsdf,
				rec.obj,
				rec.mtrl,
				rec.u, rec.v));

			sampledPdf = sampling.pdf;
			throughput *= sampling.bsdf;

			vec3 nextDir = normalize(sampling.dir);
			
			if (rec.mtrl->isSingular()) {
				// For canceling cosine term.
				auto costerm = dot(normalize(toNextVtx), orienting_normal);
				throughput /= costerm;

				// Just only for refraction.
				// Cancel probability to select reflection or refraction.
				throughput *= sampling.subpdf;
			}

			// refraction�̔��ˁA���܂̊m�����|�����킹��.
			// refraction�ȊO�ł� 1 �Ȃ̂ŉe���͂Ȃ�.
			totalAreaPdf *= sampling.subpdf;

			ray = aten::ray(rec.p + nextDir * AT_MATH_EPSILON, nextDir);

			prevNormal = orienting_normal;
			depth++;
		}

		return std::move(Result(vec3(), -1, -1, false));
	}

	BDPT2::Result BDPT2::genLightPath(
		std::vector<Vertex>& vs,
		aten::Light* light,
		sampler* sampler,
		scene* scene,
		camera* camera) const
	{
		// TODO
		// Only AreaLight...

		// ������ɃT���v���_�����iy0�j.
		auto res = light->getSamplePosNormalPdf(sampler);
		auto posOnLight = std::get<0>(res);
		auto nmlOnLight = std::get<1>(res);
		auto pdfOnLight = std::get<2>(res);

		// �m�����x�̐ς�ێ��i�ʐϑ��x�Ɋւ���m�����x�j.
		double totalAreaPdf = pdfOnLight;

		// ������ɐ������ꂽ���_�𒸓_���X�g�ɒǉ�.
		vs.push_back(Vertex(
			posOnLight,
			nmlOnLight,
			nmlOnLight,
			ObjectType::Light,
			totalAreaPdf,
			vec3(0),
			light->getLe(),
			light));

		// ���݂̕��ˋP�x�i�����e�J�����ϕ��̃X���[�v�b�g�j.
		// �{���͎��̒��_�iy1�j�����܂�Ȃ��ƌ������炻�̕����ւ̕��ˋP�x�l�͌��܂�Ȃ����A����͊��S�g�U���������肵�Ă���̂ŁA�����Ɉ˂炸�Ɉ��̒l�ɂȂ�.
		vec3 throughput = light->getLe();

		int depth = 0;

		// ���S�g�U���������肵�Ă���̂ŁADiffuse�ʂɂ�����T���v�����O���@�Ɠ������̂������Ď��̕��������߂�.
		nmlOnLight = normalize(nmlOnLight);
		vec3 dir = lambert::sampleDirection(nmlOnLight, sampler);
		real sampledPdf = lambert::pdf(nmlOnLight, dir);
		vec3 prevNormal = nmlOnLight;

		ray ray = aten::ray(posOnLight + dir * AT_MATH_EPSILON, dir);

		while (depth < m_maxDepth) {
			hitrecord rec;
			bool isHit = scene->hit(ray, AT_MATH_EPSILON, AT_MATH_INF, rec);

			vec3 posOnImageSensor;
			vec3 posOnLens;
			vec3 posOnObjectPlane;
			int pixelx;
			int pixely;

			// �����Y�ƌ�������.
			auto lens_t = camera->hitOnLens(
				ray,
				posOnLens,
				posOnObjectPlane,
				posOnImageSensor,
				pixelx, pixely);
			
			if (AT_MATH_EPSILON < lens_t && lens_t < rec.t) {
				// ���C�������Y�Ƀq�b�g���C���[�W�Z���T�Ƀq�b�g.

				pixelx = aten::clamp(pixelx, 0, m_width - 1);
				pixelx = aten::clamp(pixelx, 0, m_height - 1);

				vec3 dir = ray.org - posOnLens;
				const real dist2 = dir.squared_length();
				dir.normalize();

				const vec3& camnml = camera->getDir();

				// �����Y�̏�̓_�̃T���v�����O�m�����v�Z�B
				{
					const real c = dot(dir, camnml);
					const real areaPdf = sampledPdf * c / dist2;

					totalAreaPdf *= areaPdf;
				}

				// �W�I���g���^�[��
				{
					const real c0 = dot(dir, camnml);
					const real c1 = dot(-dir, prevNormal);
					const real G = c0 * c1 / dist2;

					throughput *= G;
				}

				// �����Y��ɐ������ꂽ�_�𒸓_���X�g�ɒǉ��i��{�I�Ɏg��Ȃ��j.
				vs.push_back(Vertex(
					posOnLens,
					camnml,
					camnml,
					ObjectType::Lens,
					totalAreaPdf,
					throughput,
					vec3(0),
					nullptr,
					nullptr,
					0, 0));

				const real W_dash = camera->getWdash(
					ray.org,
					vec3(0, 1, 0),	// pinhole�̂Ƃ��͂����ɂ��Ȃ�.�܂��Athinlens�̂Ƃ��͎g��Ȃ��̂ŁA�K���Ȓl�ł���.
					posOnImageSensor,
					posOnLens,
					posOnObjectPlane);

				const vec3 contrib = throughput * W_dash / totalAreaPdf;

				return std::move(Result(contrib, pixelx, pixely, true));
			}

			if (!isHit) {
				break;
			}

			// �����ʒu�̖@��.
			// ���̂���̃��C�̓��o���l��.
			vec3 orienting_normal = dot(rec.normal, ray.dir) < 0.0 ? rec.normal : -rec.normal;

			// ���V�A�����[���b�g�ɂ���āA�V�������_���u���ۂɁv�T���v�����O���A��������̂��ǂ��������肷��.
			auto rrProb = russianRoulette(throughput);
			auto rr = sampler->nextSample();
			if (rr >= rrProb) {
				break;
			}

			// �V�������_���T���v�����O���ꂽ�̂ŁA�g�[�^���̊m�����x�ɏ�Z����.
			totalAreaPdf *= rrProb;

			const vec3 toNextVtx = ray.org - rec.p;

			{
				// �V�������_���T���v�����O���邽�߂̊m�����x�֐��͗��̊p���x�Ɋւ�����̂ł��������߁A�����ʐϑ��x�Ɋւ���m�����x�֐��ɕϊ�����.
				const real c = dot(normalize(toNextVtx), orienting_normal);
				const real dist2 = toNextVtx.squared_length();
				const real areaPdf = sampledPdf * (c / dist2);

				// �S�Ă̒��_���T���v�����O����m�����x�̑��v���o���B
				totalAreaPdf *= areaPdf;
			}

			{
				// �W�I���g���^�[��.
				const real c0 = dot(normalize(toNextVtx), orienting_normal);
				const real c1 = dot(normalize(-toNextVtx), prevNormal);
				const real dist2 = toNextVtx.squared_length();
				const real G = c0 * c1 / dist2;

				throughput = G * throughput;
			}

			auto sampling = rec.mtrl->sample(ray, orienting_normal, rec, sampler, rec.u, rec.v);

			// �V�������_�𒸓_���X�g�ɒǉ�����.
			vs.push_back(Vertex(
				rec.p,
				rec.normal,
				orienting_normal,
				ObjectType::Object,
				totalAreaPdf,
				throughput,
				sampling.bsdf,
				rec.obj,
				rec.mtrl,
				rec.u, rec.v));

			sampledPdf = sampling.pdf;
			throughput *= sampling.bsdf;

			vec3 nextDir = normalize(sampling.dir);

			if (rec.mtrl->isSingular()) {
				// For canceling cosine term.
				auto costerm = dot(normalize(toNextVtx), orienting_normal);
				throughput /= costerm;

				// Just only for refraction.
				// Cancel probability to select reflection or refraction.
				throughput *= sampling.subpdf;
			}

			// refraction�̔��ˁA���܂̊m�����|�����킹��.
			// refraction�ȊO�ł� 1 �Ȃ̂ŉe���͂Ȃ�.
			totalAreaPdf *= sampling.subpdf;

			ray = aten::ray(rec.p + nextDir * AT_MATH_EPSILON, nextDir);

			prevNormal = orienting_normal;
			depth++;
		}

		return std::move(Result(vec3(), -1, -1, false));
	}

	// ���_from���璸�_next���T���v�����O�����Ƃ���Ƃ��A�ʐϑ��x�Ɋւ���T���v�����O�m�����x���v�Z����.
	real BDPT2::computAreaPdf(
		camera* camera,
		const std::vector<const Vertex*>& vs,
		const int prev_idx,			// ���_cur�̑O�̒��_�̃C���f�b�N�X.
		const int cur_idx,          // ���_cur�̃C���f�b�N�X.
		const int next_idx) const   // ���_next�̃C���f�b�N�X.
	{
		const Vertex& curVtx = *vs[cur_idx];
		const Vertex& nextVtx = *vs[next_idx];

		Vertex const* prevVtx = nullptr;
		if (0 <= prev_idx && prev_idx < vs.size()) {
			prevVtx = vs[prev_idx];
		}

		const vec3 to = nextVtx.pos - curVtx.pos;
		const vec3 normalizedTo = normalize(to);

		real pdf = real(0);

		// ���_from�̃I�u�W�F�N�g�̎�ނɂ���āA���̎��̒��_�̃T���v�����O�m�����x�̌v�Z���@���ς��.
		if (curVtx.objType == ObjectType::Light) {
			// TODO
			// Light�͊��S�g�U�ʂƂ��Ĉ���.
			pdf = lambert::pdf(curVtx.orienting_normal, normalizedTo);
		}
		else if (curVtx.objType == ObjectType::Lens) {
			// �����Y��̓_����V�[����̓_���T���v�����O����Ƃ��̖ʐϑ��x�Ɋւ���m�����x���v�Z.
			// �C���[�W�Z���T��̓_�̃T���v�����O�m�����x�����ɕϊ�����.

			// �V�[����̓_���烌���Y�ɓ��郌�C.
			ray r(nextVtx.pos, -normalizedTo);

			vec3 posOnLens;
			vec3 posOnObjectplane;
			vec3 posOnImagesensor;
			int x, y;

			auto lens_t = camera->hitOnLens(
				r,
				posOnLens,
				posOnObjectplane,
				posOnImagesensor,
				x, y);

			if (lens_t > AT_MATH_EPSILON) {
				// ���C�������Y�Ƀq�b�g���C���[�W�Z���T�Ƀq�b�g.

				// �C���[�W�Z���T��̃T���v�����O�m�����x���v�Z.
				// �C���[�W�Z���T�̖ʐϑ��x�Ɋւ���m�����x���V�[����̃T���v�����O�m�����x�i�ʐϑ��x�Ɋւ���m�����x�j�ɕϊ�����Ă���.
				const real imageSensorAreaPdf = camera->getPdfImageSensorArea(
					nextVtx.pos,
					nextVtx.orienting_normal,
					posOnImagesensor,
					posOnLens,
					posOnObjectplane);

				return imageSensorAreaPdf;
			}
			else {
				return real(0);
			}
		}
		else {
			if (prevVtx) {
				const vec3 wi = normalize(curVtx.pos - prevVtx->pos);
				const vec3 wo = normalize(nextVtx.pos - curVtx.pos);

				if (curVtx.mtrl->isTranslucent()) {
					// TODO
				}
				else {
					pdf = curVtx.mtrl->pdf(curVtx.orienting_normal, wi, wo, curVtx.u, curVtx.v);
				}
			}
		}

		// ���̒��_�̖@�����A���݂̒��_����̕����x�N�g���Ɋ�Â��ĉ��߂ċ��߂�.
		const vec3 next_new_orienting_normal = dot(to, nextVtx.nml) < 0.0
			? nextVtx.nml
			: -nextVtx.nml;

		// ���̊p���x�Ɋւ���m�����x��ʐϑ��x�Ɋւ���m�����x�ɕϊ�.
		const real c = dot(-normalizedTo, next_new_orienting_normal);
		const real dist2 = to.squared_length();;
		pdf *= c / dist2;

		return pdf;
	}

	real BDPT2::computeMISWeight(
		camera* camera,
		real totalAreaPdf,
		const std::vector<Vertex>& eye_vs,
		int numEyeVtx,
		const std::vector<Vertex>& light_vs,
		int numLightVtx) const
	{
		// NOTE
		// https://www.slideshare.net/h013/edubpt-v100
		// p157 - p167

		// ������̃T���v�����O�m��.
		const auto& beginLight = light_vs[0];
		const real areaPdf_y0 = beginLight.totalAreaPdf;

		// �J������̃T���v�����O�m��.
		const auto& beginEye = eye_vs[0];
		const real areaPdf_x0 = beginEye.totalAreaPdf;

		std::vector<const Vertex*> vs(numEyeVtx + numLightVtx);

		// ���_�����ɕ��ׂ�B
		// vs[0] = y0, vs[1] = y1, ... vs[k-1] = x1, vs[k] = x0
		
		// light�T�u�p�X.
		for (int i = 0; i < numLightVtx; ++i) {
			vs[i] = &light_vs[i];
		}

		// eye�T�u�p�X.
		for (int i = numEyeVtx - 1; i >= 0; --i) {
			vs[numLightVtx + numEyeVtx - 1 - i] = &eye_vs[i];
		}

		// �I�_�̃C���f�b�N�X.
		const int k = numLightVtx + numEyeVtx - 1;

		// pi1/pi ���v�Z.
		std::vector<real> pi1_pi(numLightVtx + numEyeVtx);
		{
			{
				const auto* vtx = vs[0];
				auto rr = russianRoulette(vtx->throughput);
				auto areaPdf_y1 = computAreaPdf(camera, vs, 2, 1, 0);
				pi1_pi[0] = areaPdf_y0 / (areaPdf_y1 * rr);
			}

			// ���V�A�����[���b�g�̊m���͑ł����������̂ł���Ȃ��H
			for (int i = 1; i < k; i++)
			{
				auto a = computAreaPdf(camera, vs, i - 2, i - 1, i);
				auto b = computAreaPdf(camera, vs, i + 2, i + 1, i);
				pi1_pi[i] = a / b;
			}

			{
				const auto* vtx = vs[k];
				auto rr = russianRoulette(vtx->throughput);
				auto areaPdf_x1 = computAreaPdf(camera, vs, k - 2, k - 1, k);
				pi1_pi[k] = (areaPdf_x1 * rr) / areaPdf_x0;
			}
		}

		// p�����߂�
		std::vector<real> p(numEyeVtx + numLightVtx + 1);
		{
			// �^�񒆂�totalAreaPdf���Z�b�g.
			p[numLightVtx] = totalAreaPdf;

			// �^�񒆂��N�_�ɔ������v�Z.

			// light�T�u�p�X.
			for (int i = numLightVtx; i <= k; ++i) {
				p[i + 1] = p[i] * pi1_pi[i];
			}

			// eye�T�u�p�X.
			for (int i = numLightVtx - 1; i >= 0; --i) {
				p[i] = p[i + 1] / pi1_pi[i];
			}

			for (int i = 0; i < vs.size(); ++i) {
				const auto& vtx = *vs[i];

				// ��������ӂɌ��܂�̂ŁA�e�����y�ڂ��Ȃ�.
				if (vtx.mtrl && vtx.mtrl->isSingular()) {
					p[i] = 0.0;
					p[i + 1] = 0.0;
				}
			}
		}

		// Power-heuristic
		real misWeight = real(0);
		for (int i = 0; i < p.size(); ++i) {
			const real v = p[i] / p[numLightVtx];
			misWeight += v * v; // beta = 2
		}

		if (misWeight > real(0)) {
			misWeight = real(1) / misWeight;
		}

		return misWeight;
	}

	void BDPT2::combine(
		const int x, const int y,
		std::vector<Result>& result,
		const std::vector<Vertex>& eye_vs,
		const std::vector<Vertex>& light_vs,
		scene* scene,
		camera* camera) const
	{
		const int eyeNum = (int)eye_vs.size();
		const int lightNum = (int)light_vs.size();

		for (int numEyeVtx = 1; numEyeVtx <= eyeNum; ++numEyeVtx)
		{
			for (int numLightVtx = 1; numLightVtx <= lightNum; ++numLightVtx)
			{
				int targetX = x;
				int targetY = y;

				// ���ꂼ��̃p�X�̒[�_.
				const Vertex& eye_end = eye_vs[numEyeVtx - 1];
				const Vertex& light_end = light_vs[numLightVtx - 1];

				// �g�[�^���̊m�����x�v�Z.
				const real totalAreaPdf = eye_end.totalAreaPdf * light_end.totalAreaPdf;
				if (totalAreaPdf == 0) {
					// �����Ȃ������̂ŁA�������Ȃ�.
					continue;
				}

				// MC�X���[�v�b�g.
				vec3 eyeThroughput = eye_end.throughput;
				vec3 lightThroughput = light_end.throughput;

				// ���_��ڑ����邱�ƂŐV������������鍀.
				vec3 throughput(1);

				// numLightVtx == 1�̂Ƃ��A�񊮑S�g�U�����̏ꍇ�͑���̒��_�̈ʒu�����MC�X���[�v�b�g���ω����邽�߉��߂Č�������̕��ˋP�x�l���v�Z����.
				if (numLightVtx == 1) {
					// TODO
					// ����͊��S�g�U�����Ȃ̂ŒP����emission�̒l������.
					const auto& lightVtx = light_vs[0];
					lightThroughput = lightVtx.light->getLe();
				}

				// �[�_�Ԃ��ڑ��ł��邩.
				const vec3 lightEndToEyeEnd = eye_end.pos - light_end.pos;
				ray r(light_end.pos, normalize(lightEndToEyeEnd));

				hitrecord rec;
				bool isHit = scene->hit(r, AT_MATH_EPSILON, AT_MATH_INF, rec);

				if (eye_end.objType == ObjectType::Lens) {
					// light�T�u�p�X�𒼐ڃ����Y�ɂȂ���.
					vec3 posOnLens;
					vec3 posOnObjectplane;
					vec3 posOnImagesensor;
					int px, py;

					const auto lens_t = camera->hitOnLens(
						r,
						posOnLens,
						posOnObjectplane,
						posOnImagesensor,
						px, py);

					if (AT_MATH_EPSILON < lens_t
						&& lens_t < rec.t)
					{
						// ���C�������Y�Ƀq�b�g���C���[�W�Z���T�Ƀq�b�g.
						targetX = aten::clamp(px, 0, m_width - 1);
						targetY = aten::clamp(px, 0, m_height - 1);

						const real W_dash = camera->getWdash(
							rec.p,
							rec.normal,
							posOnImagesensor,
							posOnLens,
							posOnObjectplane);

						throughput *= W_dash;
					}
					else {
						// light�T�u�p�X�𒼐ڃ����Y�ɂȂ��悤�Ƃ������A�Օ����ꂽ��C���[�W�Z���T�Ƀq�b�g���Ȃ������ꍇ�A�I���.
						continue;
					}
				}
				else if (eye_end.objType == ObjectType::Light) {
					// eye�T�u�p�X�̒[�_�������i���˗�0�j�������ꍇ�͏d�݂��[���ɂȂ�p�X�S�̂̊�^���[���ɂȂ�̂ŁA�����I���.
					// �����͔��˗�0��z�肵�Ă���.
					continue;
				}
				else {
					if (eye_end.mtrl->isSingular()) {
						// eye�T�u�p�X�̒[�_���X�y�L�����₾�����ꍇ�͏d�݂��[���ɂȂ�p�X�S�̂̊�^���[���ɂȂ�̂ŁA�����I���.
						// �X�y�L�����̏ꍇ�͔��˕����ň�ӂɌ��܂�Aligh�T�u�p�X�̒[�_�ւ̕�������v����m�����[��.
						continue;
					}
					else {
						// �[�_���m���ʂ̕��̂ŎՕ�����邩�ǂ����𔻒肷��B�Օ�����Ă����珈���I���.
						const real len = (eye_end.pos - rec.p).length();
						if (len >= AT_MATH_EPSILON) {
							continue;
						}

						const auto& bsdf = eye_end.bsdf;
						throughput *= bsdf;
					}
				}

				if (light_end.objType == ObjectType::Lens
					|| light_end.objType == ObjectType::Light)
				{
					// light�T�u�p�X�̒[�_�������Y�������ꍇ�͏d�݂��[���ɂȂ�p�X�S�̂̊�^���[���ɂȂ�̂ŁA�����I���.
					// �����Y��̓X�y�L�����Ƃ݂Ȃ�.

					// eye�T�u�p�X�̒[�_�������i���˗�0�j�������ꍇ�͏d�݂��[���ɂȂ�p�X�S�̂̊�^���[���ɂȂ�̂ŁA�����I���.
					// �����͔��˗�0��z�肵�Ă���.
				}
				else {
					if (light_end.mtrl->isSingular()) {
						// eye�T�u�p�X�̒[�_���X�y�L�����₾�����ꍇ�͏d�݂��[���ɂȂ�p�X�S�̂̊�^���[���ɂȂ�̂ŁA�����I���.
						// �X�y�L�����̏ꍇ�͔��˕����ň�ӂɌ��܂�Aligh�T�u�p�X�̒[�_�ւ̕�������v����m�����[��.
						continue;
					}
					else {
						const auto& bsdf = light_end.bsdf;
						throughput *= bsdf;
					}
				}

				// �[�_�Ԃ̃W�I���g���t�@�N�^
				{
					real cx = dot(normalize(-lightEndToEyeEnd), eye_end.orienting_normal);
					cx = std::max(cx, real(0));

					real cy = dot(normalize(lightEndToEyeEnd), light_end.orienting_normal);
					cy = std::max(cy, real(0));

					const real dist2 = lightEndToEyeEnd.squared_length();

					const real G = cx * cy / dist2;

					throughput *= G;
				}

				// MIS.
				const real misWeight = computeMISWeight(
					camera,
					totalAreaPdf,
					eye_vs, numEyeVtx,
					light_vs, numLightVtx);

				if (misWeight <= real(0)) {
					continue;
				}

				// �ŏI�I�ȃ����e�J�����R���g���r���[�V���� =
				//	MIS�d��.
				//	* ���_��ڑ����邱�ƂŐV������������鍀.
				//  * eye�T�u�p�X�X���[�v�b�g.
				//	* light�T�u�p�X�X���[�v�b�g.
				//	/ �p�X�̃T���v�����O�m�����x�̑��v.
				const vec3 contrib = misWeight * throughput * eyeThroughput * lightThroughput / totalAreaPdf;
				result.push_back(Result(
					contrib,
					targetX, targetY,
					numEyeVtx <= 1 ? false : true));
			}
		}
	}

	void BDPT2::render(
		Destination& dst,
		scene* scene,
		camera* camera)
	{
		m_width = dst.width;
		m_height = dst.height;
		uint32_t samples = dst.sample;

		m_maxDepth = dst.maxDepth;

		const real divPixelProb = real(1) / (real)(m_width * m_height);

		// TODO
		/*
		m_rrDepth = dst.russianRouletteDepth;

		if (m_rrDepth > m_maxDepth) {
		m_rrDepth = m_maxDepth - 1;
		}
		*/

		auto threadnum = thread::getThreadNum();

		std::vector<std::vector<vec4>> image(threadnum);

		for (int i = 0; i < threadnum; i++) {
			image[i].resize(m_width * m_height);
		}

#if defined(ENABLE_OMP) && !defined(BDPT_DEBUG)
#pragma omp parallel
#endif
		{
			auto idx = thread::getThreadIdx();

			auto time = timer::getSystemTime();

#if defined(ENABLE_OMP) && !defined(BDPT_DEBUG)
#pragma omp for
#endif
			for (int y = 0; y < m_height; y++) {
				for (int x = 0; x < m_width; x++) {
					std::vector<Result> result;

					for (uint32_t i = 0; i < samples; i++) {
						//XorShift rnd((y * height * 4 + x * 4) * samples + i + 1);
						//Halton rnd((y * height * 4 + x * 4) * samples + i + 1);
						//Sobol rnd((y * height * 4 + x * 4) * samples + i + 1 + time.milliSeconds);
						Sobol rnd((y * m_height * 4 + x * 4) * samples + i + 1);
						UniformDistributionSampler sampler(&rnd);

						std::vector<Vertex> eyevs;
						std::vector<Vertex> lightvs;

						auto eyeRes = genEyePath(eyevs, x, y, &sampler, scene, camera);
						
#if 0
						if (eyeRes.isTerminate) {
							int pos = eyeRes.y * m_width + eyeRes.x;
							image[idx][pos] += vec4(eyeRes.contrib, 1);
						}
#else
						auto lightNum = scene->lightNum();
						for (uint32_t i = 0; i < lightNum; i++) {
							auto light = scene->getLight(i);
							auto lightRes = genLightPath(lightvs, light, &sampler, scene, camera);

							if (eyeRes.isTerminate) {
								const real misWeight = computeMISWeight(
									camera,
									eyevs[eyevs.size() - 1].totalAreaPdf,
									eyevs,
									(const int)eyevs.size(),   // num_eye_vertex
									lightvs,
									0);                         // num_light_vertex

								const vec3 contrib = misWeight * eyeRes.contrib;
								result.push_back(Result(contrib, eyeRes.x, eyeRes.y, true));
							}

							if (lightRes.isTerminate) {
								const real misWeight = computeMISWeight(
									camera,
									lightvs[lightvs.size() - 1].totalAreaPdf,
									eyevs,
									0,							// num_eye_vertex
									lightvs,
									(const int)lightvs.size());	// num_light_vertex

								const vec3 contrib = misWeight * lightRes.contrib;
								result.push_back(Result(contrib, lightRes.x, lightRes.y, false));
							}

							combine(
								x, y,
								result, 
								eyevs,
								lightvs,
								scene,
								camera);
						}
#endif
					}

#if 1
					for (int i = 0; i < (int)result.size(); i++) {
						const auto& res = result[i];

						const int pos = res.y * m_width + res.x;

						if (res.isStartFromPixel) {
							image[idx][pos] += vec4(res.contrib, 1);
						}
						else {
							// ����ꂽ�T���v���ɂ��āA�T���v�������݂̉�f�ix,y)���甭�˂��ꂽeye�T�u�p�X���܂ނ��̂������ꍇ
							// Ixy �̃����e�J��������l��samples[i].value���̂��̂Ȃ̂ŁA���̂܂ܑ����B���̌�A���̉摜�o�͎��ɔ��˂��ꂽ�񐔂̑��v�iiteration_per_thread * num_threads)�Ŋ���.
							//
							// ����ꂽ�T���v���ɂ��āA���݂̉�f���甭�˂��ꂽeye�T�u�p�X���܂ނ��̂ł͂Ȃ������ꍇ�ilight�T�u�p�X���ʂ̉�f(x',y')�ɓ��B�����ꍇ�j��
							// Ix'y' �̃����e�J��������l��V���������킯�����A���̏ꍇ�A�摜�S�̂ɑ΂��Č���������T���v���𐶐����A���܂���x'y'�Ƀq�b�g�����ƍl���邽��
							// ���̂悤�ȃT���v���ɂ��Ă͍ŏI�I�Ɍ������甭�˂����񐔂̑��v�Ŋ����āA��f�ւ̊�^�Ƃ���K�v������.
							image[idx][pos] += vec4(res.contrib * divPixelProb, 1);
						}
					}
#endif
				}
			}
		}

		std::vector<vec4> tmp(m_width * m_height);

		for (int i = 0; i < threadnum; i++) {
			auto& img = image[i];
			for (int y = 0; y < m_height; y++) {
				for (int x = 0; x < m_width; x++) {
					int pos = y * m_width + x;

					auto clr = img[pos] / samples;
					clr.w = 1;

					tmp[pos] += clr;
				}
			}
		}

#if defined(ENABLE_OMP) && !defined(BDPT_DEBUG)
#pragma omp parallel for
#endif
		for (int y = 0; y < m_height; y++) {
			for (int x = 0; x < m_width; x++) {
				int pos = y * m_width + x;

				auto clr = tmp[pos];
				clr.w = 1;

				dst.buffer->put(x, y, clr);
			}
		}
	}
}