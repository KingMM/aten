#include "camera/CameraOperator.h"
#include "math/vec4.h"
#include "math/mat4.h"

#pragma optimize( "", off)

namespace aten {
	void CameraOperator::move(
		camera& camera,
		real x, real y)
	{
		auto& pos = camera.getPos();
		auto& at = camera.getAt();

		// �ړ��x�N�g��.
		aten::vec3 offset(x, y, real(0));

		// �J�����̉�]���l������.
		aten::vec3 dir = at - pos;
		dir = normalize(dir);
		dir.y = real(0);

		aten::mat4 mtxRot;
		mtxRot.asRotateFromVector(dir, aten::vec3(0, 1, 0));
		
		mtxRot.applyXYZ(offset);

		pos += offset;
		at += offset;
	}

	void CameraOperator::dolly(
		camera& camera,
		real scale)
	{
		auto& pos = camera.getPos();
		auto& at = camera.getAt();

		// ���_�ƒ����_�̋���.
		real len = length(pos - at);

		// ���_���璍���_�ւ̕���.
		auto dir = pos - at;
		dir = normalize(dir);

		// �X�P�[�����O.
		// �����_�܂ł̋����ɉ�����.
		auto distScale = scale * len * 0.01f;
		dir *= distScale;

		// �V�������_.
		pos += dir;
	}

	static inline real projectionToSphere(
		real radius,
		real x,
		real y)
	{
		real z = real(0);
		real dist = aten::sqrt(x * x + y * y);

		// r * 1/��2 �̓_�őo�Ȑ��Ɛڂ�������ƊO��

		if (dist < radius * real(0.70710678118654752440)) {
			// ����

			// NOTE
			// r * r = x * x + y * y + z * z
			// <=> z * z = r * r - (x * x + y * y)
			z = aten::sqrt(radius * radius - dist * dist);
		}
		else {
			// �O��
			real t = radius * radius * 0.5f;
			z = t / dist;
		}

		return z;
	}

	void CameraOperator::rotate(
		camera& camera,
		real x1, real y1,
		real x2, real y2)
	{
		static const real radius = real(0.8);

		// �X�N���[����̂Q�_����g���b�N�{�[����̓_���v�Z����.
		// GLUT�Ɠ������@.

		aten::vec3 v1(
			x1, y1,
			projectionToSphere(radius, x1, y1));
		v1 = normalize(v1);

		aten::vec3 v2(
			x2, y2,
			projectionToSphere(radius, x2, y2));
		v2 = normalize(v2);

		// ��]��.
		auto axis = cross(v1, v2);
		axis = normalize(axis);

		const auto dir = camera.getDir();
		aten::mat4 transform;
		transform.asRotateFromVector(dir, aten::vec3(0, 1, 0));

		// �J�����̉�]��Ԃɍ��킹�Ď�����].
		transform.applyXYZ(axis);

		// ��]�̊p�x
		// NOTE
		// V1�EV2 = |V1||V2|cos�� = cos�� (|V1| = |V2| = 1)
		// �� = acos(cos��)
		// => �� = acos(cos��) = acos(V1�EV2)
		real theta = aten::acos(dot(v1, v2));

		// ��].
		aten::mat4 mtxRot;
		mtxRot.asRotateByAxis(theta, axis);

		auto& pos = camera.getPos();
		auto& at = camera.getAt();

		pos -= at;
		pos = mtxRot.applyXYZ(pos);
		pos += at;
	}
}