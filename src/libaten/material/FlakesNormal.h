#pragma once

#include "material/material.h"

namespace AT_NAME
{
	class FlakesNormal {
	private:
		FlakesNormal();
		~FlakesNormal();

	public:
		static AT_DEVICE_MTRL_API aten::vec4 gen(
			real u, real v,
			real flake_scale = real(50.0),				// Smaller values zoom into the flake map, larger values zoom out.
			real flake_size = real(0.5),				// Relative size of the flakes
			real flake_size_variance = real(0.7),		// 0.0 makes all flakes the same size, 1.0 assigns random size between 0 and the given flake size
			real flake_normal_orientation = real(0.5));	// Blend between the flake normals (0.0) and the surface normal (1.0)

		static inline AT_DEVICE_MTRL_API real computeFlakeDensity(
			real flake_size,
			real flakeMapAspect)
		{
			// NOTE
			// size : map�T�C�Y�̒��ӂɐ�߂銄��.
			//  ex) w = 1280, h = 720, size = 0.5 -> flake radius = (w > h ? h : w) * size = 720 * 0.5 = 360
			// scale : �T�C�Y�� 1 / scale �ɂ���.

			// NOTE
			// ��ʂɐ�߂�flake�̖ʐϊ����͈ȉ��̂悤�ɂȂ�.
			//     (Pi * radius * radius) * N / (w * h)
			//       radius : flake radius
			//       N : Number of flake
			//       w : map width
			//       h : map height
			// �����ŁAw > h �Ƃ��āAradius = (w > h ? h : w) * size  / scale = size / scale * h �𓖂Ă͂߂�.
			//     Density = Pi * (size / scale * h)^2 * N / (w * h)
			//             = Pi * (size / scale)^2 * N * h / w
			// scale = 1 �̏ꍇ�A�}�b�v�S�̂�flake����P�ƂȂ�ƍl���邱�Ƃ��ł���.
			// �܂�AIf scale = 1, N = 1 �ƂȂ�.
			// scale���傫���Ȃ��Ă��A���ꂪ�c����scale�J��Ԃ���邾���Ȃ̂ŁA�}�b�v�ɐ�߂�flake�̖ʐϊ����͕ς��Ȃ�.
			//     Density = Pi * (size / scale)^2 * N * h / w
			//             = Pi * size^2 * h / w

			// TODO
			// aspect = w / h �̑O��Ȃ̂ŁAh / w ���擾���������ߋt���ɂ���.
			auto aspect = real(1) / flakeMapAspect;

			auto D = AT_MATH_PI * flake_size * flake_size * aspect;
			D = aten::cmpMin(D, real(1));

			return D;
		}
	};
}
