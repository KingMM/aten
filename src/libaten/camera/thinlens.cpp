#include "camera/thinlens.h"

namespace aten {
	// NOTE
	// https://www.slideshare.net/h013/edubpt-v100

	static real plane_intersection(const vec3& normal, const vec3& pos, const ray& ray)
	{
		/*
			���C�̕����� : x = p + t v
			���ʂ̕����� : (p - p0)�En = 0
			p  : ���C�̏o��_
			v  : ���C�̕���
			p0 : ���ʏ�̂P�_
			n  : �@���x�N�g��

			�A���������ʁA���C�̐i�񂾋���t��

			(p0 - p)�En
			t = ------------
			v�En

			v�En=0 �̎��́A���C�ƕ��ʂ����s�ɐi��ł���̂ŁA�����Ȃ�.
			t<0�̎��́A�����̌�둤�Ɍ�_������̂ŁA��͂����邱�Ƃ��Ȃ�.
		*/

		real vn = dot(ray.dir, normal);

		if (fabs(vn) > AT_MATH_EPSILON) {
			real xpn = dot(pos - ray.org, normal);
			real t = xpn / vn;
			return t;
		}

		return AT_MATH_INF;
	}

	// NOTE
	// xI : �C���[�W�Z���T�[��̓_.
	// x0 : �����Y��̓_.
	// xV : �I�u�W�F�N�g�v���[����̓_.
	// x1 : �V�[�����̓_.

	void ThinLensCamera::init(
		int width, int height,
		vec3 lookfrom, vec3 lookat, vec3 vup,
		real imageSensorSize,
		real imageSensorToLensDistance,
		real lensToObjectplaneDistance,	// focus length
		real lensRadius,
		real W_scale)
	{
		m_imageWidthPx = width;
		m_imageHeightPx = height;

		m_imageSensorToLensDistance = imageSensorToLensDistance;
		m_lensToObjectplaneDistance = lensToObjectplaneDistance;

		// �C���[�W�Z���T�̃T�C�Y.
		m_imagesensor.width = imageSensorSize * m_imageWidthPx / m_imageHeightPx;
		m_imagesensor.height = imageSensorSize;

		// �P�s�N�Z���̕����T�C�Y.
		m_pixelWidth = m_imagesensor.width / m_imageWidthPx;
		m_pixelHeight = m_imagesensor.height / m_imageHeightPx;

		m_imagesensor.center = lookfrom;
		m_imagesensor.dir = normalize(lookat - lookfrom);
		m_imagesensor.u = normalize(cross(m_imagesensor.dir, vup)) * m_imagesensor.width;
		m_imagesensor.v = normalize(cross(m_imagesensor.u, m_imagesensor.dir)) * m_imagesensor.height;

		m_imagesensor.lower_left = m_imagesensor.center - 0.5f * m_imagesensor.u - 0.5f * m_imagesensor.v;

		// �I�u�W�F�N�g�v���[���̓C���[�W�Z���T�[�ƕ��s�Ȃ̂ŁA�C���[�W�Z���T�[�Ɠ����������g���΂���.
		m_objectplane.center = m_imagesensor.center + (m_imageSensorToLensDistance + m_lensToObjectplaneDistance) * m_imagesensor.dir;
		m_objectplane.normal = m_imagesensor.dir;
		m_objectplane.u = m_imagesensor.u;
		m_objectplane.v = m_imagesensor.v;

		m_objectplane.lower_left = m_objectplane.center - 0.5f * m_objectplane.u - 0.5f * m_objectplane.v;

		// �����Y�̓C���[�W�Z���T�[�ƕ��s�Ȃ̂ŁA�C���[�W�Z���T�[�Ɠ����������g���΂���.
		m_lens.center = m_imagesensor.center + m_imageSensorToLensDistance * m_imagesensor.dir;
		m_lens.u = lensRadius * normalize(m_imagesensor.u);
		m_lens.v = lensRadius * normalize(m_imagesensor.v);
		m_lens.normal = m_imagesensor.dir;
		m_lens.radius = lensRadius;

		// W(xI <- x0) �Z���T�̃Z���V�e�B�r�e�B�͊ȒP�̂��ߒ萔�ɂ��Ă���.
		m_W = W_scale / (m_pixelWidth * m_pixelHeight);
	}

	CameraSampleResult ThinLensCamera::sample(
		real s, real t,
		sampler* sampler) const
	{
		AT_ASSERT(sampler);

		// [0, 1] -> [-0.5, 0.5]
		s -= 0.5;
		t -= 0.5;

		CameraSampleResult result;

		// �C���[�W�Z���T��̍��W�A[-0,5, 0.5]�͈̔́i0,0)���C���[�W�Z���T����������.
		result.posOnImageSensor = m_imagesensor.center + s * m_imagesensor.u + t * m_imagesensor.v;

		// �I�u�W�F�N�g�v���[����̍��W�v�Z
		// �I�u�W�F�N�g�v���[���̃T�C�Y�́A�����Y�̌����̔{���v�Z�im=b/a�j
		real ratio = m_lensToObjectplaneDistance / m_imageSensorToLensDistance;

		// �Z���T�[�ƃI�u�W�F�N�g�v���[���̌����͔��΂ɂȂ�̂ŁA�}�C�i�X����?
		real u_on_objectplane = -ratio * s;
		real v_on_objectplane = -ratio * t;
		result.posOnObjectplane = m_objectplane.center + u_on_objectplane * m_objectplane.u + v_on_objectplane * m_objectplane.v;

		// NOTE
		// lens.u�Alens.v �� lens.radius ���܂܂�Ă���̂ŁA�����Y�̔��a�ɂ��čl������K�v���Ȃ�.
		real r0 = sqrt(sampler->nextSample());
		real r1 = sampler->nextSample() * real(2.0) * AT_MATH_PI;
		real u = r0 * cos(r1);
		real v = r0 * sin(r1);
		result.posOnLens = m_lens.center + u * m_lens.u + v * m_lens.v;
		result.nmlOnLens = m_lens.normal;

		// �s�N�Z�����̈�_���T���v�����O����m�����x�֐��i�ʐϑ��x�j
		result.pdfOnImageSensor = real(1.0) / (m_pixelWidth * m_pixelHeight);

		// �����Y��̈�_���T���v�����O����m�����x�֐��i�ʐϑ��x�j
		result.pdfOnLens = real(1.0) / (AT_MATH_PI * m_lens.radius * m_lens.radius);

		result.r = ray(
			result.posOnLens,
			normalize(result.posOnObjectplane - result.posOnLens));

		return std::move(result);
	}

	real ThinLensCamera::hitOnLens(
		const ray& r,
		vec3& posOnLens,
		vec3& posOnObjectPlane,
		vec3& posOnImageSensor,
		int& x, int& y) const
	{
		// �����Y�Ɣ���
		auto lens_t = plane_intersection(m_lens.normal, m_lens.center, r);

		if (AT_MATH_EPSILON < lens_t)
		{
			posOnLens = r.org + lens_t * r.dir;
			auto l = (posOnLens - m_lens.center).length();
			auto d = dot(m_lens.normal, r.dir);

			if (l < m_lens.radius && d <= 0.0)
			{
				auto objplane_t = plane_intersection(m_objectplane.normal, m_objectplane.center, r);

				posOnObjectPlane = r.org + objplane_t * r.dir;

				auto u_on_objectplane = dot(posOnObjectPlane - m_objectplane.center, normalize(m_objectplane.u)) / m_objectplane.u.length();
				auto v_on_objectplane = dot(posOnObjectPlane - m_objectplane.center, normalize(m_objectplane.v)) / m_objectplane.v.length();

				auto ratio = m_imageSensorToLensDistance / m_lensToObjectplaneDistance;
				auto u = -ratio * u_on_objectplane;
				auto v = -ratio * v_on_objectplane;

				posOnImageSensor = m_imagesensor.center + u * m_imagesensor.u + v * m_imagesensor.v;

				if (-0.5f <= u && u < 0.5
					&& -0.5f <= v && v < 0.5)
				{
					x = (int)((u + 0.5f) * m_imageWidthPx);
					y = (int)((v + 0.5f) * m_imageHeightPx);

					return lens_t;
				}
			}
		}

		return -AT_MATH_INF;
	}

	// �C���[�W�Z���T��̃T���v�����O�m�����x�i�C���[�W�Z���T�̖ʐϑ��x�Ɋւ���m�����x�j���V�[����̃T���v�����O�m�����x�i�ʐϑ��x�Ɋւ���m�����x�j�ɕϊ�����.
	real ThinLensCamera::convertImageSensorPdfToScenePdf(
		real pdfImage,
		const vec3& hitPoint,
		const vec3& hitpointNml,
		const vec3& posOnImageSensor,
		const vec3& posOnLens,
		const vec3& posOnObjectPlane) const
	{
		// NOTE
		// http://rayspace.xyz/CG/contents/DoF.html

		// NOTE
		// https://www.slideshare.net/h013/edubpt-v100
		// �C���[�W�Z���T��̊m�����x��ʐϑ��x�ɕϊ�.
		// p111
		// Pa = (B/A)^2 * (r''/r)^2 * (cos��'/cos��'') * Pij

		const vec3& x0 = posOnLens;
		const vec3& xI = posOnImageSensor;
		const vec3& xV = posOnObjectPlane;
		const vec3& x1 = hitPoint;

		const vec3 x0_xV = xV - x0;
		const vec3 x0_x1 = x1 - x0;

		// NOTE
		// B = imagesensor_to_lens_distance
		// A = lens_to_objectplane(focus distance)

		// (B/A)^2
		const real ba = m_imageSensorToLensDistance / m_lensToObjectplaneDistance;
		const real ba2 = ba * ba;

		// (r''/r')^2
		auto r2 = (x0_xV.squared_length() / x0_x1.squared_length());

		// (cos��'/cos��'')
		auto c2 = (dot(normalize(-x0_x1), hitpointNml) / dot(normalize(x0_x1), normalize(m_imagesensor.dir)));

		// �C���[�W�Z���T�̖ʐϑ��x�Ɋւ���m�����x.
		//auto pdfOnImageSensor = 1.0 / (m_imagesensor.width * m_imagesensor.height);

		auto pdf = pdfImage * ba2 * r2 * c2;

		return pdf;
	}

	real ThinLensCamera::getSensitivity(
		const vec3& posOnImagesensor,
		const vec3& posOnLens) const
	{
		// NOTE
		// https://www.slideshare.net/h013/edubpt-v100
		// p54 - p62

		const vec3 x0_xI = posOnImagesensor - posOnLens;

		// �Z���T��̓_�̔����ϕ����A�����Y��̓_�̐ϕ��ɕϐ��ϊ��������ɓ��������W��
		// (cos��)^2/r^2
		const real cos = dot(normalize(-m_imagesensor.dir), normalize(x0_xI));
		const real len2 = x0_xI.squared_length();
		
		const real G = (cos * cos) / len2;

		real ret = m_W * G;

		return ret;
	}

	real ThinLensCamera::getWdash(
		const vec3& hitPoint,
		const vec3& hitpointNml,
		const vec3& posOnImageSensor,
		const vec3& posOnLens,
		const vec3& posOnObjectPlane) const
	{
		// �􉽓I�ȌW���v�Z + �Z���T�[�Z���V�e�B�r�e�B�̍����v�Z����.
		// x1 -> x0�ւ̕��ˋP�x���ŏI�I�ɃC���[�W�Z���T�ɗ^�����^�x.

		const vec3& x0 = posOnLens;
		const vec3& xI = posOnImageSensor;
		const vec3& xV = posOnObjectPlane;
		const vec3& x1 = hitPoint;

		const vec3 x0_xI = xI - x0;	// lens to image sensor.
		const vec3 x0_xV = xV - x0;	// lens to object plane.
		const vec3 x0_x1 = x1 - x0;	// lens to hit point.

		// NOTE
		// B = imagesensor_to_lens_distance
		// A = lens_to_objectplane(focus distance)

#if 1
		// B/A
		const real ba2 = x0_xV.squared_length() / x0_xI.squared_length();

		const real r = m_imageSensorToLensDistance / m_lensToObjectplaneDistance;
		const real r2 = r * r;

		const real c = dot(normalize(x0_xI), normalize(-m_imagesensor.dir)) / dot(normalize(x0_x1), normalize(m_imagesensor.dir));
		const real c2 = c * c;

		real W_dash = m_W * ba2 * r2 * c2;
#else
		// B/A
		const real ba = m_imageSensorToLensDistance / m_lensToObjectplaneDistance;
		const real ba2 = ba * ba;

		const real d0 = x0_xI.length();
		const real c0 = dot(normalize(x0_xI), normalize(-m_imagesensor.dir));
		const real G0 = c0 * c0 / (d0 * d0);

		const real d1 = x0_xV.length();
		const real c1 = dot(normalize(x0_x1), normalize(m_imagesensor.dir));
		const real G1 = c1 * c1 / (d1 * d1);

		real W_dash = m_W * ba2 / G0 * G1;
#endif

		return W_dash;
	}

	void ThinLensCamera::revertRayToPixelPos(
		const ray& ray,
		int& px, int& py) const
	{
		vec3 posOnLens;
		vec3 posOnObjectPlane;
		vec3 posOnImageSensor;

		hitOnLens(
			ray,
			posOnLens,
			posOnObjectPlane,
			posOnImageSensor,
			px, py);
	}
}
