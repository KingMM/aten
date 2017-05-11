#include "sampler/halton.h"
#include "math/math.h"
#include "defs.h"

namespace aten {
	std::vector<uint32_t> Halton::PrimeNumbers;

	// �f������.
	void Halton::makePrimeNumbers(uint32_t maxNumber/*= MaxPrimeNumbers*/)
	{
		// �G���g�X�e�l�X��⿁i�ӂ邢�j.
		// https://ja.wikipedia.org/wiki/%E3%82%A8%E3%83%A9%E3%83%88%E3%82%B9%E3%83%86%E3%83%8D%E3%82%B9%E3%81%AE%E7%AF%A9

		// �A���S���Y��.
		// �X�e�b�v 1.
		//		�T�����X�g��2����x�܂ł̐����������œ����.
		// �X�e�b�v 2.
		//		�T�����X�g�̐擪�̐���f�����X�g�Ɉړ����A���̔{����T�����X�g����⿂����Ƃ�.
		// �X�e�b�v 3.
		//		��L��⿂����Ƃ������T�����X�g�̐擪�l��x�̕������ɒB����܂ōs��.
		// �X�e�b�v 4.
		//		�T�����X�g�Ɏc��������f�����X�g�Ɉړ����ď����I��.

		// ��̗�@x=120 �̏ꍇ
		// �X�e�b�v 1
		//		�T�����X�g = { 2����120�܂� }�A�T�����X�g�̐擪�l = 2
		// �X�e�b�v 2 - 1
		//		�f�����X�g = { 2 }
		//		�T�����X�g = { 3����119�܂ł̊ }�A�T�����X�g�̐擪�l = 3
		// �X�e�b�v 2 - 2
		//		�f�����X�g = { 2,3 }
		//		�T�����X�g = { 5,7,11,13,17,19,23,25,29,31,35,37,41,43,47,49,53,55,59,61,65,67,71,73,77,79,83,85,89,91,95,97,101,103,107,109,113,115,119 }
		//		�T�����X�g�̐擪�l = 5
		// �X�e�b�v 2 - 3
		//		�f�����X�g = { 2,3,5 }
		//		�T�����X�g = { 7,11,13,17,19,23,29,31,37,41,43,47,49,53,59,61,67,71,73,77,79,83,89,91,97,101,103,107,109,113,119 }
		//		�T�����X�g�̐擪�l = 7
		// �X�e�b�v 2 - 4
		//		�f�����X�g = { 2,3,5,7 }
		//		�T�����X�g = { 11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113 }
		//		�T�����X�g�̐擪�l = 11
		// �X�e�b�v 3
		//		�T�����X�g�̐擪�l�� sqrt(120) = 10.954 �ɒB���Ă���̂ŃX�e�b�v4��.
		// �X�e�b�v 4
		//		�c�����T�����X�g��f�����X�g�Ɉړ�.
		//		�f�����X�g = { 2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113 }

		
		uint32_t N = aten::clamp<uint32_t>(maxNumber, 2, MaxPrimeNumbers);

		// �T�����X�g.
		//  0 : �f�����X�g�ɓ���.
		//  1 : �f�����X�g�ɓ���Ȃ�.
		// �܂��́A���ׂđf�����X�g�ɓ���\��������̂ŁA���ׂă[���ɂ��Ă���.
		std::vector<uint32_t> table;
		table.resize(N + 1, 0);

		// �A���S���Y���Ƃ��� 2 ����n�܂�.

		// �T�����X�g�̐擪�̒l�� sqrt(N) �ɒB������I��.
		// -> sqrt(N) �̔{���ɂ��ĒT��������I��.
		//    �Ȃ��Ȃ�Asqrt(N) �̔{���ɂ��ĒT���������_�ŁAsqrt(N) �̔{���͂ӂ邢�ɂ�������̂ŁA�擪�l�͕K���Asqrt(N) �̔{���𒴂���.
		// -> i �͒T�����̐��l�ɂȂ�̂ŁAi �� sqrt(N) �𒴂���܂ŒT���𑱂���΂������ƂɂȂ�.

		for (uint32_t i = 2; i * i <= N; i++) {
			if (table[i] == 0) {
				// i���f�����X�g�ɓ���.

				// i�̔{�����ӂ邢�ɂ�����.
				//  i �� i �𑫂��ƁAi �̎��� i �̔{���ƂȂ�.
				//  i �� i �𑫂�������΁Ai �̔{���ƂȂ�.
				for (uint32_t n = i + i; n <= N; n += i) {
					table[n] = 1;
				}
			}
		}

		// �ŏI�I�ȑf�����X�g�Ɋi�[.
		for (uint32_t i = 2; i <= N; i++) {
			if (table[i] == 0) {
				// 0 �Ȃ̂ŁA�f�����X�g����.
				PrimeNumbers.push_back(i);
			}
		}
	}

	real Halton::nextSample()
	{
		// NOTE
		// https://en.wikipedia.org/wiki/Halton_sequence

		if (m_param.dimension >= PrimeNumbers.size()) {
			// �����𒴂��邱�Ƃ͋����Ȃ�..
			AT_ASSERT(false);
			return aten::drand48();
		}

		real f = 1;
		real r = 0;

		const auto base = PrimeNumbers[m_param.dimension++];

		uint32_t i = m_param.idx;

		while (i > 0) {
			f = f / (real)base;
			r = r + f * (i % base);
			i = i / base;
		}

		return r;
	}
}
