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
			x = y = z = CONST_REAL(0.0);
		}
		vec3(real _x, real _y, real _z)
			: x(_x), y(_y), z(_z)
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

	inline vec3 operator-(const vec3& v1, const vec3& v2)
	{
		vec3 ret(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
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
}