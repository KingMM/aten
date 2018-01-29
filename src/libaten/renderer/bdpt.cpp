#include "renderer/bdpt.h"
#include "misc/omputil.h"
#include "misc/timer.h"
#include "material/lambert.h"
#include "material/refraction.h"
#include "geometry/tranformable.h"
#include "sampler/xorshift.h"
#include "sampler/halton.h"
#include "sampler/sobolproxy.h"

//#define BDPT_DEBUG

#ifdef BDPT_DEBUG
#pragma optimize( "", off)
#endif

namespace aten
{
	static inline real russianRoulette(const vec3& v)
	{
		real p = std::max(v.r, std::max(v.g, v.b));
		p = aten::clamp(p, real(0), real(1));
		return p;
	}

	static inline real russianRoulette(const material* mtrl)
	{
		if (mtrl->isEmissive()) {
			return 1;
		}
		real p = russianRoulette(mtrl->color());
		return p;
	}

	real BDPT::russianRoulette(const Vertex& vtx)
	{
		real pdf = real(0);

		if (vtx.mtrl) {
			pdf = aten::russianRoulette(vtx.mtrl);
		}
		else if (vtx.light) {
			pdf = aten::russianRoulette(vtx.light->getLe());
		}
		else {
			pdf = real(0);
		}

		return pdf;
	}

	BDPT::Result BDPT::genEyePath(
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

		vec3 throughput = vec3(1);
		real totalAreaPdf = camsample.pdfOnLens;

		vec3 prevNormal = camera->getDir();
		real sampledPdf = real(1);

		//while (depth < m_maxDepth) {
		for (;;) {
			hitrecord rec;
			Intersection isect;
			if (!scene->hit(ray, AT_MATH_EPSILON, AT_MATH_INF, rec, isect)) {
				break;
			}

			// �����ʒu�̖@��.
			// ���̂���̃��C�̓��o���l��.
			vec3 orienting_normal = dot(rec.normal, ray.dir) < 0.0 ? rec.normal : -rec.normal;

			auto mtrl = material::getMaterial(rec.mtrlid);
			auto obj = transformable::getShape(rec.objid);

			// ���V�A�����[���b�g�ɂ���āA�V�������_���u���ۂɁv�T���v�����O���A��������̂��ǂ��������肷��.
			auto rrProb = aten::russianRoulette(mtrl);
			auto rr = sampler->nextSample();
			if (rr >= rrProb) {
				break;
			}

			// �V�������_���T���v�����O���ꂽ�̂ŁA�g�[�^���̊m�����x�ɏ�Z����.
			totalAreaPdf *= rrProb;

			const vec3 toNextVtx = ray.org - rec.p;

			if (depth == 0) {
				// NOTE
				// �����_�����O�������Q.
				// http://rayspace.xyz/CG/contents/LTE2.html

				// x1�̃T���v�����O�m�����x�̓C���[�W�Z���T��̃T���v�����O�m�����x��ϊ����邱�Ƃŋ��߂�,
				auto pdfOnImageSensor = camera->convertImageSensorPdfToScenePdf(
					camsample.pdfOnImageSensor,
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
				const real dist2 = squared_length(toNextVtx);
				const real areaPdf = sampledPdf * (c / dist2);

				totalAreaPdf *= areaPdf;
			}

			if (depth == 0 && camera->isPinhole()) {
				// Nothing is done...
			}
			else {
				// �W�I���g���^�[��.
				const real c0 = dot(normalize(toNextVtx), orienting_normal);
				const real c1 = dot(normalize(-toNextVtx), prevNormal);
				const real dist2 = squared_length(toNextVtx);
				const real G = c0 * c1 / dist2;
				throughput = G * throughput;
			}

			// �����Ƀq�b�g�����炻���ŒǐՏI��.
			if (mtrl->isEmissive()) {
				vec3 bsdf = lambert::bsdf(&mtrl->param(), rec.u, rec.v);

				vs.push_back(Vertex(
					rec.p,
					rec.normal,
					orienting_normal,
					ObjectType::Light,
					totalAreaPdf,
					throughput,
					bsdf,
					obj,
					mtrl,
					rec.u, rec.v));

				vec3 emit = mtrl->color();
				vec3 contrib = throughput * emit / totalAreaPdf;

				return std::move(Result(contrib, x, y, true));
			}

			auto sampling = mtrl->sample(ray, orienting_normal, rec.normal, sampler, rec.u, rec.v);

			sampledPdf = sampling.pdf;
			auto sampledBsdf = sampling.bsdf;

			if (mtrl->isSingular()) {
				// For canceling probabaility to select reflection or rafraction.
				sampledBsdf *= sampling.subpdf;

				// For canceling cosine term.
				auto costerm = dot(normalize(toNextVtx), orienting_normal);
				sampledBsdf /= costerm;
			}

			// �V�������_�𒸓_���X�g�ɒǉ�����.
			vs.push_back(Vertex(
				rec.p,
				rec.normal,
				orienting_normal,
				ObjectType::Object,
				totalAreaPdf,
				throughput,
				sampling.bsdf,
				obj,
				mtrl,
				rec.u, rec.v));

			throughput *= sampledBsdf;
			
			// refraction�̔��ˁA���܂̊m�����|�����킹��.
			// refraction�ȊO�ł� 1 �Ȃ̂ŉe���͂Ȃ�.
			totalAreaPdf *= sampling.subpdf;

			vec3 nextDir = normalize(sampling.dir);

			ray = aten::ray(rec.p + nextDir * AT_MATH_EPSILON, nextDir);

			prevNormal = orienting_normal;
			depth++;
		}

		return std::move(Result(vec3(), -1, -1, false));
	}

	BDPT::Result BDPT::genLightPath(
		std::vector<Vertex>& vs,
		aten::Light* light,
		sampler* sampler,
		scene* scene,
		camera* camera) const
	{
		// TODO
		// Only AreaLight...

		// ������ɃT���v���_�����iy0�j.
		aten::hitable::SamplePosNormalPdfResult res;
		light->getSamplePosNormalArea(&res, sampler);
		auto posOnLight = res.pos;
		auto nmlOnLight = res.nml;
		auto pdfOnLight = real(1) / res.area;

		// �m�����x�̐ς�ێ��i�ʐϑ��x�Ɋւ���m�����x�j.
		auto totalAreaPdf = pdfOnLight;

		// ������ɐ������ꂽ���_�𒸓_���X�g�ɒǉ�.
		vs.push_back(Vertex(
			posOnLight,
			nmlOnLight,
			nmlOnLight,
			ObjectType::Light,
			totalAreaPdf,
			vec3(real(0)),
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

		//while (depth < m_maxDepth) {
		for (;;) {
			hitrecord rec;
			Intersection isect;
			bool isHit = scene->hit(ray, AT_MATH_EPSILON, AT_MATH_INF, rec, isect);

			if (!camera->isPinhole()) {
				// The light will never hit to the pinhole camera.
				// The pihnole camera lens is tooooooo small.

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

				if (AT_MATH_EPSILON < lens_t && lens_t < isect.t) {
					// ���C�������Y�Ƀq�b�g���C���[�W�Z���T�Ƀq�b�g.

					pixelx = aten::clamp(pixelx, 0, m_width - 1);
					pixely = aten::clamp(pixely, 0, m_height - 1);

					vec3 dir = ray.org - posOnLens;
					const real dist2 = squared_length(dir);
					dir = normalize(dir);

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
						vec3(real(0)),
						nullptr,
						nullptr,
						real(0), real(0)));

					const real W_dash = camera->getWdash(
						ray.org,
						vec3(real(0), real(1), real(0)),	// pinhole�̂Ƃ��͂����ɂ��Ȃ�.�܂��Athinlens�̂Ƃ��͎g��Ȃ��̂ŁA�K���Ȓl�ł���.
						posOnImageSensor,
						posOnLens,
						posOnObjectPlane);

					const vec3 contrib = throughput * W_dash / totalAreaPdf;

					return std::move(Result(contrib, pixelx, pixely, true));
				}
			}

			if (!isHit) {
				break;
			}

			// �����ʒu�̖@��.
			// ���̂���̃��C�̓��o���l��.
			vec3 orienting_normal = dot(rec.normal, ray.dir) < 0.0 ? rec.normal : -rec.normal;

			auto mtrl = material::getMaterial(rec.mtrlid);
			auto obj = transformable::getShape(rec.objid);

			// ���V�A�����[���b�g�ɂ���āA�V�������_���u���ۂɁv�T���v�����O���A��������̂��ǂ��������肷��.
			auto rrProb = aten::russianRoulette(mtrl);
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
				const real dist2 = squared_length(toNextVtx);
				const real areaPdf = sampledPdf * (c / dist2);

				// �S�Ă̒��_���T���v�����O����m�����x�̑��v���o���B
				totalAreaPdf *= areaPdf;
			}

			{
				// �W�I���g���^�[��.
				const real c0 = dot(normalize(toNextVtx), orienting_normal);
				const real c1 = dot(normalize(-toNextVtx), prevNormal);
				const real dist2 = squared_length(toNextVtx);
				const real G = c0 * c1 / dist2;

				throughput = G * throughput;
			}

			auto sampling = mtrl->sample(ray, orienting_normal, rec.normal, sampler, rec.u, rec.v, true);

			sampledPdf = sampling.pdf;
			auto sampledBsdf = sampling.bsdf;

			if (mtrl->isSingular()) {
				// For canceling probabaility to select reflection or rafraction.
				sampledBsdf *= sampling.subpdf;

				// For canceling cosine term.
				auto costerm = dot(normalize(toNextVtx), orienting_normal);
				sampledBsdf /= costerm;
			}
			else if (mtrl->isEmissive()) {
				sampledBsdf = vec3(real(0));
			}

			// �V�������_�𒸓_���X�g�ɒǉ�����.
			vs.push_back(Vertex(
				rec.p,
				rec.normal,
				orienting_normal,
				ObjectType::Object,
				totalAreaPdf,
				throughput,
				sampledBsdf,
				obj,
				mtrl,
				rec.u, rec.v));

			throughput *= sampledBsdf;

			// refraction�̔��ˁA���܂̊m�����|�����킹��.
			// refraction�ȊO�ł� 1 �Ȃ̂ŉe���͂Ȃ�.
			totalAreaPdf *= sampling.subpdf;

			vec3 nextDir = normalize(sampling.dir);

			ray = aten::ray(rec.p + nextDir * AT_MATH_EPSILON, nextDir);

			prevNormal = orienting_normal;
			depth++;
		}

		return std::move(Result(vec3(), -1, -1, false));
	}

	// ���_from���璸�_next���T���v�����O�����Ƃ���Ƃ��A�ʐϑ��x�Ɋւ���T���v�����O�m�����x���v�Z����.
	real BDPT::computAreaPdf(
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

			if (camera->isPinhole()) {
				// TODO
				// I don't understand why.
				// But, it seems that the rendering result is correct...
				return real(1);
			}

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

				real imagesensorWidth = camera->getImageSensorWidth();
				real imagesensorHeight = camera->getImageSensorHeight();
				real pdfImage = real(1) / (imagesensorWidth * imagesensorHeight);

				// �C���[�W�Z���T��̃T���v�����O�m�����x���v�Z.
				// �C���[�W�Z���T�̖ʐϑ��x�Ɋւ���m�����x���V�[����̃T���v�����O�m�����x�i�ʐϑ��x�Ɋւ���m�����x�j�ɕϊ�����Ă���.
				const real imageSensorAreaPdf = camera->convertImageSensorPdfToScenePdf(
					pdfImage,
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

				if (curVtx.mtrl->isSingular()) {
					if (curVtx.mtrl->isTranslucent()) {
						// cur���_�̂ЂƂO�̒��_�Ɋ�Â��āA���̂ɓ��荞��ł���̂��A����Ƃ��o�čs���̂��𔻒肷��.
						const vec3 intoCurVtxDir = normalize(curVtx.pos - prevVtx->pos);

						// prevVtx ���� curVtx �ւ̃x�N�g�����A�X�y�L�������̂ɓ���̂��A����Ƃ��o��̂�.
						const bool into = dot(intoCurVtxDir, curVtx.nml) < real(0);

						const vec3 from_new_orienting_normal = into ? curVtx.nml : -curVtx.nml;

						auto sampling = refraction::check(
							curVtx.mtrl,
							intoCurVtxDir,
							curVtx.nml,
							from_new_orienting_normal);

						if (sampling.isIdealRefraction) {
							// ����.
							pdf = real(1);
						}
						else if (sampling.isRefraction) {
							// ���� or ����.
							pdf = dot(from_new_orienting_normal, normalizedTo) > real(0)
								? sampling.probReflection				// ����.
								: real(1) - sampling.probReflection;	// ����.
						}
						else {
							// �S����.
							pdf = real(1);
						}
					}
					else {
						pdf = real(1);
					}
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
		const real dist2 = squared_length(to);
		pdf *= c / dist2;

		return pdf;
	}

	real BDPT::computeMISWeight(
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
				auto rr = russianRoulette(*vtx);
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
				auto rr = russianRoulette(*vtx);
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

	void BDPT::combine(
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
				vec3 throughput = vec3(1);

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
				Intersection isect;
				bool isHit = scene->hit(r, AT_MATH_EPSILON, AT_MATH_INF, rec, isect);

				if (eye_end.objType == ObjectType::Lens) {
					if (camera->isPinhole()) {
						break;
					}
					else {
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
							&& lens_t < isect.t)
						{
							// ���C�������Y�Ƀq�b�g���C���[�W�Z���T�Ƀq�b�g.
							targetX = aten::clamp(px, 0, m_width - 1);
							targetY = aten::clamp(py, 0, m_height - 1);

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
						const real len = length(eye_end.pos - rec.p);
						if (len >= AT_MATH_EPSILON) {
							continue;
						}

						const auto& bsdf = eye_end.bsdf;
						throughput *= bsdf;
					}
				}

				if (light_end.objType == ObjectType::Lens) {
					// light�T�u�p�X�̒[�_�������Y�������ꍇ�͏d�݂��[���ɂȂ�p�X�S�̂̊�^���[���ɂȂ�̂ŁA�����I���.
					// �����Y��̓X�y�L�����Ƃ݂Ȃ�.
					continue;
				}
				else if (light_end.objType == ObjectType::Light) {
					// �����̔��˗�0�����肵�Ă��邽�߁A���C�g�g���[�V���O�̎��A�ŏ��̒��_�ȊO�͌�����ɒ��_��������Ȃ�.
					// num_light_vertex == 1�ȊO�ł����ɓ����Ă��邱�Ƃ͖���.
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

					const real dist2 = squared_length(lightEndToEyeEnd);

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

	void BDPT::render(
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

		auto threadnum = OMPUtil::getThreadNum();

		std::vector<std::vector<vec4>> image(threadnum);

		for (int i = 0; i < threadnum; i++) {
			image[i].resize(m_width * m_height);
		}

#if defined(ENABLE_OMP) && !defined(BDPT_DEBUG)
#pragma omp parallel
#endif
		{
			auto idx = OMPUtil::getThreadIdx();

			auto time = timer::getSystemTime();

#if defined(ENABLE_OMP) && !defined(BDPT_DEBUG)
#pragma omp for
#endif
			for (int y = 0; y < m_height; y++) {
				for (int x = 0; x < m_width; x++) {
					int pos = y * m_width + x;

					for (uint32_t i = 0; i < samples; i++) {
						auto scramble = aten::getRandom(pos) * 0x1fe3434f;

						XorShift rnd(scramble + time.milliSeconds);
						//Halton rnd(scramble + time.milliSeconds);
						//Sobol rnd(scramble + time.milliSeconds);
						//WangHash rnd(scramble + time.milliSeconds);

						std::vector<Result> result;

						std::vector<Vertex> eyevs;
						std::vector<Vertex> lightvs;

						auto eyeRes = genEyePath(eyevs, x, y, &rnd, scene, camera);
						
#if 0
						if (eyeRes.isTerminate) {
							int pos = eyeRes.y * m_width + eyeRes.x;
							image[idx][pos] += vec4(eyeRes.contrib, 1);
						}
#else
						auto lightNum = scene->lightNum();
						for (uint32_t n = 0; n < lightNum; n++) {
							auto light = scene->getLight(n);
							auto lightRes = genLightPath(lightvs, light, &rnd, scene, camera);

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
#endif
					}
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