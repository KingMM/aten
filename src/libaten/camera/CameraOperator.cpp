#include "camera/CameraOperator.h"
#include "math/vec4.h"
#include "math/mat4.h"

namespace aten {
    void CameraOperator::move(
        camera& camera,
        int x1, int y1,
        int x2, int y2,
        real scale/*= real(1)*/)
    {
        auto& pos = camera.getPos();
        auto& at = camera.getAt();

        real offsetX = (real)(x1 - x2);
        offsetX *= scale;

        real offsetY = (real)(y1 - y2);
        offsetY *= scale;

        // �ړ��x�N�g��.
        aten::vec3 offset(offsetX, offsetY, real(0));

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

    void CameraOperator::moveForward(
        camera& camera,
        real offset)
    {
        // �J�����̌����Ă������(Z��)�ɉ����Ĉړ�.

        auto& pos = camera.getPos();
        auto& at = camera.getAt();

        auto dir = camera.getDir();
        dir *= offset;

        pos += dir;
        at += dir;
    }

    void CameraOperator::moveRight(
        camera& camera,
        real offset)
    {
        // �J�����̌����Ă�������̉E��(-X��(�E����W�Ȃ̂�))�ɉ����Ĉړ�.
        auto vz = camera.getDir();

        vec3 vup(0, 1, 0);
        if (aten::abs(vz.x) < AT_MATH_EPSILON && aten::abs(vz.z) < AT_MATH_EPSILON) {
            // UP�x�N�g���Ƃ̊O�ς��v�Z�ł��Ȃ��̂ŁA
            // �V����UP�x�N�g�����ł���������E�E�E
            vup = vec3(real(0), real(0), -vz.y);
        }

        auto vx = cross(vup, vz);
        vx = normalize(vx);

        vx *= offset;
        vx *= real(-1);    // -X���ɕϊ�.

        auto& pos = camera.getPos();
        auto& at = camera.getAt();

        pos += vx;
        at += vx;
    }

    void CameraOperator::moveUp(
        camera& camera,
        real offset)
    {
        // �J�����̌����Ă�������̉E��(Y��)�ɉ����Ĉړ�.
        auto vz = camera.getDir();

        vec3 vup(0, 1, 0);
        if (aten::abs(vz.x) < AT_MATH_EPSILON && aten::abs(vz.z) < AT_MATH_EPSILON) {
            // UP�x�N�g���Ƃ̊O�ς��v�Z�ł��Ȃ��̂ŁA
            // �V����UP�x�N�g�����ł���������E�E�E
            vup = vec3(real(0), real(0), -vz.y);
        }

        auto vx = cross(vup, vz);
        vx = normalize(vx);

        auto vy = cross(vz, vx);

        vy *= offset;

        auto& pos = camera.getPos();
        auto& at = camera.getAt();

        pos += vy;
        at += vy;
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

    static inline real normalizeHorizontal(int x, real width)
    {
        real ret = (real(2) * x - width) / width;
        return ret;
    }

    static inline real normalizeVertical(int y, real height)
    {
        real ret = (height - real(2) * y) / height;
        return ret;
    }

    void CameraOperator::rotate(
        camera& camera,
        int width, int height,
        int _x1, int _y1,
        int _x2, int _y2)
    {
        static const real radius = real(0.8);

        real x1 = normalizeHorizontal(_x1, (real)width);
        real y1 = normalizeVertical(_y1, (real)height);

        real x2 = normalizeHorizontal(_x2, (real)width);
        real y2 = normalizeVertical(_y2, (real)height);

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