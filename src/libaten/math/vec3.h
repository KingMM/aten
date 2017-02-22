#pragma once

#include "defs.h"
#include "math.h"

namespace aten {
	class vec3 {
	public:
		union {
			struct {
				real x, y, z;
			};
			struct {
				real r, g, b;
			};
			real a[3];
		};

		vec3()
		{
			x = y = z = real(0);
		}
		vec3(real _x, real _y, real _z)
			: x(_x), y(_y), z(_z)
		{
		}
		vec3(real f)
			: x(f), y(f), z(f)
		{
		}

		inline const vec3& operator+() const
		{
			return *this;
		}
		inline vec3 operator-() const
		{
			return vec3(-x, -y, -z);
		}
		inline real operator[](int i) const
		{
			return a[i];
		}
		inline real& operator[](int i)
		{
			return a[i];
		};

		inline vec3& operator+=(const vec3& v)
		{
			x += v.x;
			y += v.y;
			z += v.z;
			return *this;
		}
		inline vec3& operator-=(const vec3& v)
		{
			x -= v.x;
			y -= v.y;
			z -= v.z;
			return *this;
		}
		inline vec3& operator*=(const vec3& v)
		{
			x *= v.x;
			y *= v.y;
			z *= v.z;
			return *this;
		}
		inline vec3& operator/=(const vec3& v)
		{
			x /= v.x;
			y /= v.y;
			z /= v.z;
			return *this;
		}
		inline vec3& operator*=(const real t)
		{
			x *= t;
			y *= t;
			z *= t;
			return *this;
		}
		inline vec3& operator/=(const real t)
		{
			x /= t;
			y /= t;
			z /= t;
			return *this;
		}

		inline real length() const
		{
			auto ret = aten::sqrt(x * x + y * y + z * z);
			return ret;
		}
		inline real squared_length() const
		{
			auto ret = x * x + y * y + z * z;
			return ret;
		}

		void normalize()
		{
			auto l = length();
			*this /= l;
		}
	};

	inline vec3 operator+(const vec3& v1, const vec3& v2)
	{
		vec3 ret(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z);
		return std::move(ret);
	}

	inline vec3 operator+(const vec3& v1, real f)
	{
		vec3 ret(v1.x + f, v1.y + f, v1.z + f);
		return std::move(ret);
	}

	inline vec3 operator-(const vec3& v1, const vec3& v2)
	{
		vec3 ret(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
		return std::move(ret);
	}

	inline vec3 operator-(const vec3& v1, real f)
	{
		vec3 ret(v1.x - f, v1.y - f, v1.z - f);
		return std::move(ret);
	}

	inline vec3 operator*(const vec3& v1, const vec3& v2)
	{
		vec3 ret(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z);
		return std::move(ret);
	}

	inline vec3 operator/(const vec3& v1, const vec3& v2)
	{
		vec3 ret(v1.x / v2.x, v1.y / v2.y, v1.z / v2.z);
		return std::move(ret);
	}

	inline vec3 operator*(real t, const vec3& v)
	{
		vec3 ret(t * v.x, t * v.y, t * v.z);
		return std::move(ret);
	}

	inline vec3 operator*(const vec3& v, real t) {
		vec3 ret(t * v.x, t * v.y, t * v.z);
		return std::move(ret);
	}

	inline vec3 operator/(const vec3& v, real t)
	{
		vec3 ret(v.x / t, v.y / t, v.z / t);
		return std::move(ret);
	}

	inline real dot(const vec3& v1, const vec3& v2)
	{
		auto ret = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
		return ret;
	}

	inline vec3 cross(const vec3& v1, const vec3& v2)
	{
		vec3 ret(
			v1.a[1] * v2.a[2] - v1.a[2] * v2.a[1],
			v1.a[2] * v2.a[0] - v1.a[0] * v2.a[2],
			v1.a[0] * v2.a[1] - v1.a[1] * v2.a[0]);

		return std::move(ret);
	}

	inline vec3 normalize(const vec3& v)
	{
		auto ret = v / v.length();
		return std::move(ret);
	}

	// ���s�x�N�g�����v�Z.
	inline vec3 getOrthoVector(const vec3& n)
	{
		vec3 p;

		// NOTE
		// dot���v�Z�����Ƃ��Ƀ[���ɂȂ�悤�ȃx�N�g��.
		// k �� normalize �v�Z�p.

		if (aten::abs(n.z) > real(0)) {
			real k = aten::sqrt(n.y * n.y + n.z * n.z);
			p.x = 0; 
			p.y = -n.z / k; 
			p.z = n.y / k;
		}
		else {
			real k = aten::sqrt(n.x * n.x + n.y * n.y);
			p.x = n.y / k; 
			p.y = -n.x / k; 
			p.z = 0;
		}

		return normalize(p);
	}

	inline bool isInvalid(const vec3& v)
	{
		bool b0 = isInvalid(v.x);
		bool b1 = isInvalid(v.y);
		bool b2 = isInvalid(v.z);

		return b0 || b1 || b2;
	}
}
