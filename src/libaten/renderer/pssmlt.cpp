#include <vector>
#include <stack>
#include "renderer/pssmlt.h"
#include "sampler/xorshift.h"
#include "misc/color.h"
#include "misc/omputil.h"
#include "misc/timer.h"

namespace aten
{
	// A Simple and Robust Mutation Strategy for the Metropolis���Q��.
	// Kelemen style MLT�p�f�[�^�\��.
	// Kelemen style�ł̓p�X�����Ɏg�������̋�Ԃŕψق������肷��.
	// ���̈��̃T���v���̃f�[�^�\��.
	struct PrimarySample {
		int modify_time{ 0 };
		real value;

		PrimarySample()
		{
			// TODO
			value = aten::drand48();
		}
	};

	// Kelemen MLT�ɂ����āA�p�X�����Ɏg���e�헐����primary space��������Ă���.
	// PrimarySample()��ʏ��rand01()�̑���Ɏg���ăp�X��������B����͕��ʂ̃p�X�g���[�X���g�����B�i�o�����p�X�g�������g����j.
	// Metropolis�@�Ȃ̂Ō��݂̏�Ԃ��玟�̏�ԂւƑJ�ڂ����邪���̑J�ڂ̋�Ԃ��]���̃��[���h��Ԃł͂Ȃ�
	// �p�X�𐶐�����̂Ɏg���闐���̋�ԂɂȂ��Ă���.
	// ������Ԃɂ�����ψفiMutate()�j�̌��ʂ��g���čĂѓ����悤�Ƀp�X�g���Ńp�X�𐶐�����Ƃ��̃p�X�͎��R�ɕψٌ�̃p�X�ɂȂ��Ă���.
	class MLTSampler : public sampler {
	public:
		MLTSampler(sampler* rnd)
		{
			m_rnd = rnd;
			u.resize(128);
		}
		~MLTSampler() {}

	public:
		void init()
		{
			usedRandCoords = 0;
		}

		virtual real nextSample() override final;

		void clearStack();

	private:
		inline real Mutate(const real x);

	public:
		sampler* m_rnd{ nullptr };

		std::vector<PrimarySample> u;
		std::stack<PrimarySample> stack;

		// accept ���ꂽ mutation �̉�.
		int globalTime{ 0 };

		int largeStep{ 0 };

		// �Ō�� large step �� accept ���ꂽ time.
		int largeStepTime{ 0 };

		int usedRandCoords{ 0 };
	};

	real MLTSampler::Mutate(const real x)
	{
		const real r = m_rnd->nextSample();

		const real s1 = real(1.0) / real(512.0);
		const real s2 = real(1.0) / real(16.0);
		const real dx = s1 / (s1 / s2 + aten::abs(real(2.0) * r - real(1.0))) - s1 / (s1 / s2 + real(1.0));

		if (r < real(0.5)) {
			real x1 = x + dx;
			x1 = (x1 < real(1.0)) ? x1 : x1 - real(1.0);
			return x1;
		}
		else {
			real x1 = x - dx;
			x1 = (x1 < real(0.0)) ? x1 + 1.f : x1;
			return x1;
		}
	}

	real MLTSampler::nextSample()
	{
		if (u.size() <= usedRandCoords) {
			// expand.
			u.resize((uint32_t)(u.size() * 1.5));
		}

		if (u[usedRandCoords].modify_time < globalTime) {
			if (largeStep > 0) {
				// large step.

				stack.push(u[usedRandCoords]);    // save state.
				u[usedRandCoords].modify_time = globalTime;
				u[usedRandCoords].value = m_rnd->nextSample();
			}
			else {
				// small step,

				if (u[usedRandCoords].modify_time < largeStepTime) {
					u[usedRandCoords].modify_time = largeStepTime;
					u[usedRandCoords].value = m_rnd->nextSample();
				}

				// lazy evaluation of mutations.
				while (u[usedRandCoords].modify_time < globalTime - 1) {
					u[usedRandCoords].value = Mutate(u[usedRandCoords].value);
					u[usedRandCoords].modify_time++;
				}

				stack.push(u[usedRandCoords]);    // save state.
				u[usedRandCoords].value = Mutate(u[usedRandCoords].value);
				u[usedRandCoords].modify_time = globalTime;
			}
		}

		usedRandCoords++;
		auto ret = u[usedRandCoords - 1].value;

		return ret;
	}

	void MLTSampler::clearStack()
	{
		// �X�^�b�N��ɂ���.
		while (!stack.empty()) {
			// std::stack doesn't have clear function...
			// ��ɂȂ�܂Ń|�b�v����.
			stack.pop();
		}
	}

	/////////////////////////////////////////////////////////////////

	PSSMLT::Path PSSMLT::genPath(
		scene* scene,
		sampler* sampler,
		int x, int y,
		int width, int height,
		camera* camera)
	{
		real weight = 1;

		if (x < 0) {
			weight *= width;
			x = (int)(sampler->nextSample() * width);
			if (x == width) {
				x = 0;
			}
		}
		if (y < 0) {
			weight *= height;
			y = (int)(sampler->nextSample() * height);
			if (y == height) {
				y = 0;
			}
		}

		real u = x / (real)width;
		real v = y / (real)height;

		auto camsample = camera->sample(u, v, sampler);

		auto path = radiance(sampler, camsample.r, camera, camsample, scene);

		auto pdfOnImageSensor = camsample.pdfOnImageSensor;
		auto pdfOnLens = camsample.pdfOnLens;

		auto s = camera->getSensitivity(
			camsample.posOnImageSensor,
			camsample.posOnLens);

		Path retPath;

		retPath.contrib = path.contrib * s / (pdfOnImageSensor * pdfOnLens);
		retPath.x = x;
		retPath.y = y;
		retPath.isTerminate = path.isTerminate;
		retPath.weight = 1 / (1 / weight);

		return std::move(retPath);
	}

	void PSSMLT::render(
		Destination& dst,
		scene* scene,
		camera* camera)
	{
		int width = dst.width;
		int height = dst.height;
		uint32_t samples = dst.sample;

		int mltNum = dst.mltNum;

		// �ψى�.
		// MLT�̓s�N�Z�����ł͉񂳂Ȃ��̂ŁA�Œ�ł���f���ȏ�͉��悤�ɂ��Ȃ��Ɖ悪���܂�Ȃ�.
		int mutation = samples * width * height;

		m_maxDepth = dst.maxDepth;
		m_rrDepth = dst.russianRouletteDepth;

		if (m_rrDepth > m_maxDepth) {
			m_rrDepth = m_maxDepth - 1;
		}

		auto threadNum = OMPUtil::getThreadNum();

		std::vector<std::vector<vec3>> acuumImage(threadNum);

		auto time = timer::getSystemTime();

#ifdef ENABLE_OMP
#pragma omp parallel for
#endif
		for (int mi = 0; mi < mltNum; mi++) {
			auto idx = OMPUtil::getThreadIdx();

			auto& image = acuumImage[idx];
			if (image.empty()) {
				image.resize(width * height);
				memset(&image[0], 0, sizeof(vec3) * width * height);
			}

			// TODO
			// sobol �� halton sequence �̓X�e�b�v�����������ăI�[�o�[�t���[���Ă��܂�...
			XorShift rnd(4 * mltNum + mi + 1 + time.milliSeconds);
			MLTSampler mlt(&rnd);

			// ��������p�X�𐶐�����.
			// ���̃p�X����MLT�Ŏg���ŏ��̃p�X�𓾂�B(Markov Chain Monte Carlo�ł������j.

			// �K���ɑ��߂̐�.
			int seedPathMax = width * height;
			if (seedPathMax <= 0) {
				seedPathMax = 1;
			}

			std::vector<Path> seedPaths(seedPathMax);

			real sumI = 0.0;
			mlt.largeStep = 1;

			for (int i = 0; i < seedPathMax; i++) {
				mlt.init();

				// gen path.
				seedPaths[i] = genPath(scene, &mlt, -1, -1, width, height, camera);
				const auto& sample = seedPaths[i];

				// �܂��͐������邾���Ȃ̂ŁA���ׂ�accept����.
				mlt.globalTime++;

				// �����݂̂Ȃ̂ŃX�^�b�N����ɂ���?
				mlt.clearStack();

				// sum I.
				sumI += color::luminance(sample.contrib);
			}

			// �ŏ��̃p�X�����߂�.
			// �P�x�l�Ɋ�Â��d�_�T���v�����O�ɂ���đI��ł���.
			int selecetdPath = 0;
			{
				auto cost = rnd.nextSample() * sumI;
				real accumlatedImportance = 0;

				for (int i = 0; i < seedPathMax; i++) {
					const auto& path = seedPaths[i];
					accumlatedImportance += color::luminance(path.contrib);

					if (accumlatedImportance >= cost) {
						selecetdPath = i;
						break;
					}
				}
			}

			const real b = sumI / seedPathMax;
			const real p_large = 0.5;
			const int M = mutation;
			int accept = 0;
			int reject = 0;

			Path oldPath = seedPaths[selecetdPath];

			for (int i = 0; i < M; i++) {
				mlt.largeStep = rnd.nextSample() < p_large ? 1 : 0;

				mlt.init();

				// gen new path
				Path newPath = genPath(scene, &mlt, -1, -1, width, height, camera);

				real I = color::luminance(newPath.contrib);
				real oldI = color::luminance(oldPath.contrib);

				real a = std::min(real(1.0), I / oldI);

				const real newPath_W = (a + mlt.largeStep) / (I / b + p_large) / M;
				const real oldPath_W = (real(1.0) - a) / (oldI / b + p_large) / M;

				int newPos = newPath.y * width + newPath.x;
				vec3 newV = newPath_W * newPath.contrib * newPath.weight;
				image[newPos] += newV;

				int oldPos = oldPath.y * width + oldPath.x;
				vec3 oldV = oldPath_W * oldPath.contrib * oldPath.weight;
				image[oldPos] += oldV;

				auto r = rnd.nextSample();

				if (r < a) {
					// accept.
					accept++;

					// �ψق���.
					oldPath = newPath;

					if (mlt.largeStep) {
						mlt.largeStepTime = mlt.globalTime;
					}
					mlt.globalTime++;

					// no state resoration.
					mlt.clearStack();
				}
				else {
					// reject.
					reject++;

					// restore state.
					int idx = mlt.usedRandCoords - 1;
					while (!mlt.stack.empty()) {
						mlt.u[idx--] = mlt.stack.top();
						mlt.stack.pop();
					}
				}
			}
		}

		for (uint32_t n = 0; n < threadNum; n++) {
			auto& image = acuumImage[n];
			for (int i = 0; i < width * height; i++) {
				dst.buffer->add(i, vec4(image[i] / real(mltNum), real(1)));
			}
		}
	}
}
