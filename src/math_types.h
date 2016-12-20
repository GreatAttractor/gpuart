/*
GPU-Assisted Ray Tracer
Copyright (C) 2016 Filip Szczerek <ga.software@yahoo.com>

This file is part of gpuart.

Gpuart is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Gpuart is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with gpuart.  If not, see <http://www.gnu.org/licenses/>.

File description:
    3D vector class
*/

#ifndef GPUART_MATH_HEADER
#define GPUART_MATH_HEADER

#include <initializer_list>
#include <iostream>
#include <math.h>


namespace gpuart
{

template<typename T>
class Vec3
{
  public:

    T x, y, z;

    Vec3(): x(0), y(0), z(0) { }
    Vec3(T x, T y, T z): x(x), y(y), z(z) { }
    Vec3(const float xyz[3])  { x = xyz[0]; y = xyz[1]; z = xyz[2]; }
    Vec3(const double xyz[3]) { x = xyz[0]; y = xyz[1]; z = xyz[2]; }
    Vec3(const std::initializer_list<T> &xyz): x(xyz[0]), y(xyz[1]), z(xyz[2]) { }

    Vec3(const Vec3<float> &v): x(v.x), y(v.y), z(v.z) { }
    Vec3(const Vec3<double> &v): x(v.x), y(v.y), z(v.z) { }

    Vec3 &operator =(const std::initializer_list<T> &xyz)
    {
        x = xyz[0]; y = xyz[1]; z = xyz[2];
        return *this;
    }

    void storeIn(float xyz[3])
    {
        xyz[0] = x;
        xyz[1] = y;
        xyz[2] = z;
    }

    T length() const { return sqrt(sqrlength()); }

    T sqrlength() const { return x*x + y*y + z*z; }

	Vec3 normalized() const { return (*this)/this->length(); }

    Vec3 & operator +=(const Vec3 &v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    Vec3 & operator -=(const Vec3 &v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }

    /// Cross product
    Vec3 & operator ^=(const Vec3 &v)
    {
        T _x = x;
        T _y = y;
        T _z = z;
        x = _y*v.z - _z*v.y;
        y = _z*v.x - _x*v.z;
        z = _x*v.y - _y*v.x;
        return *this;
    }

    Vec3 & operator *=(const T a)
    {
        x *= a;
        y *= a;
        z *= a;
        return *this;
    }

    Vec3 & operator /=(const T a)
    {
        const T a_inv = 1/a;
        x *= a_inv;
        y *= a_inv;
        z *= a_inv;
        return *this;
    }

    Vec3 operator -() { return Vec3(-x, -y, -z); }

    Vec3 & operator +() { return *this; }

    bool operator ==(const Vec3 &v)
      {return x == v.x && y == v.y && z == v.z;}

    bool operator !=(const Vec3 &v)
      {return x != v.x || y != v.y || z != v.z;}

    friend Vec3 operator +(const Vec3 &v, const Vec3 &w)
    {
        Vec3 result = v;
        result += w;
        return result;
    }

    friend Vec3 operator -(const Vec3 &v, const Vec3 &w)
    {
        Vec3 result = v;
        result -= w;
        return result;
    }

    /// Dot product
    friend T operator *(const Vec3 &v, const Vec3 &w)
    {
        return (v.x * w.x + v.y * w.y + v.z * w.z);
    }

    /// Cross product
    friend Vec3 operator ^(const Vec3 &v, const Vec3 &w)
    {
        return Vec3(v.y*w.z - v.z*w.y,
                    v.z*w.x - v.x*w.z,
                    v.x*w.y - v.y*w.x);
    }

    friend Vec3 operator *(const Vec3 &v, const T a)
    {
        Vec3 result=v;
        result *= a;
        return result;
    }

    friend Vec3 operator *(const T a, const Vec3 &v)
    {
        Vec3 result = v;
        result *= a;
        return result;
    }

    friend Vec3 operator /(const Vec3 &v, const T a)
    {
        Vec3 result = v;
        result /= a;
        return result;
    }

    /// Returns vector rotated by 'angle' around X axis (right-handed coord. system)
    Vec3 vrotx(T angle) const
    {
        T cosine = cos(angle);
        T sine = sin(angle);

        return Vec3(
            x,
            y*cosine - z*sine,
            y*sine + z*cosine);
    }

    /// Returns vector rotated by angle specified by sine and cosine around X axis (right-handed coord. system)
    Vec3 vrotx(T sine, T cosine) const
    {
        return Vec3(
            x,
            y*cosine - z*sine,
            y*sine + z*cosine);
    }

    /// Returns vector rotated by 'angle' around Y axis (right-handed coord. system)
    Vec3 vroty(T angle) const
    {
        T cosine = cos(angle);
        T sine = sin(angle);
        return Vec3(
            z*sine + x*cosine,
            y,
            z*cosine - x*sine);
    }

    /// Returns vector rotated by angle specified by sine and cosine around Y axis (right-handed coord. system)
    Vec3 vroty(T sine, T cosine) const
    {
        return Vec3(
            z*sine + x*cosine,
            y,
            z*cosine - x*sine);
    }

    /// Returns vector rotated by 'angle' around Z axis (right-handed coord. system)
    Vec3 vrotz(T angle) const
    {
        T cosine = cos(angle);
        T sine = sin(angle);
        return Vec3(
            x*cosine - y*sine,
            x*sine + y*cosine,
            z);
    }

    /// Returns vector rotated by angle specified by sine and cosine around Z axis (right-handed coord. system)
    Vec3 vrotz(T sine, T cosine) const
    {
        return Vec3(
            x*cosine - y*sine,
            x*sine + y*cosine,
            z);
    }

    /// Returns 'v' rotated around 'axis' (unit vector) by an angle with the specified sine and cosine
    static Vec3 rotate(const Vec3 v, const Vec3 axis, T sine, T cosine)
    {
        Vec3 result;
        result.x = v.x * (axis.x*axis.x+(1-axis.x*axis.x)*cosine) + v.y *(axis.x*axis.y*(1-cosine)-axis.z*sine) + v.z * (axis.x*axis.z*(1-cosine)+axis.y*sine);
        result.y = v.x * (axis.x*axis.y*(1-cosine)+axis.z*sine) + v.y * (axis.y*axis.y+(1-axis.y*axis.y)*cosine) + v.z * (axis.y*axis.z*(1-cosine)-axis.x*sine);
        result.z = v.x * (axis.x*axis.z*(1-cosine)-axis.y*sine) + v.y *(axis.y*axis.z*(1-cosine)+axis.x*sine) + v.z * (axis.z*axis.z+(1-axis.z*axis.z)*cosine);
        return result;
    }

    friend std::ostream & operator <<(std::ostream &os, const gpuart::Vec3<T> &v)
    {
        os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
        return os;
    }
};

typedef Vec3<double> Vec3d;
typedef Vec3<float> Vec3f;

}

#endif
