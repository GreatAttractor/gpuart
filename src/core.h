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
    Core classes header
*/

#ifndef GPUART_CORE_HEADER
#define GPUART_CORE_HEADER

#include <nanogui/nanogui.h>
#include <iostream>
#include <vector>

#include "math_types.h"


#define RGBA_PAD    0.0f
#define RGBA_ELEMS  4

namespace gpuart
{
    /// Corresponds with primitive type in CheckIntersection() (GLSL)
    enum Primitive_t
    {
        SPHERE   = 0,
        DISC     = 1,
        TRIANGLE = 2,
        CONE     = 3
    };

    class Primitive
    {
    public:

        typedef std::vector<GLfloat> Data;

    protected:
        // Bounding box (in world space); derived classes have to calculate it on construction
        float Xmin, Xmax, Ymin, Ymax, Zmin, Zmax;

    private:

        virtual Primitive_t GetType() const = 0;

        /** Adds primitive's contents at the end of 'data' in format suitable
            for later BVH traversal in a shader. */
        virtual void StoreDataIntoBVH(Data &data) const = 0;

    public:

        virtual ~Primitive() { }

        /** Adds primitive's type and contents at the end of 'data'
            in format suitable for later BVH traversal in a shader. */
        void StoreIntoBVH(Data &data) const
        {
            uint32_t ptype = (uint32_t)GetType();
            data.push_back(*reinterpret_cast<GLfloat*>(&ptype));
            data.push_back(RGBA_PAD);
            data.push_back(RGBA_PAD);
            data.push_back(RGBA_PAD);
            StoreDataIntoBVH(data);
        }

        float GetXmin() const { return Xmin; }
        float GetXmax() const { return Xmax; }
        float GetYmin() const { return Ymin; }
        float GetYmax() const { return Ymax; }
        float GetZmin() const { return Zmin; }
        float GetZmax() const { return Zmax; }
    };

    class Sphere: public Primitive
    {
        Vec3f Center;
        float Radius;

        void CalcBoundingBox();

        /// See the base class declaration for details
        void StoreDataIntoBVH(Data &data) const override;

        Primitive_t GetType() const override { return Primitive_t::SPHERE; }

    public:

        Sphere();

        Sphere(const Vec3f &center, float radius);

        /// Prints to 'os' the data at 'it' stored previously by StoreDataIntoBVH()
        static void PrintBVH(Data::const_iterator &it, std::ostream &os);
    };

    class Disc: public Primitive
    {
        Vec3f Center, Normal;
        float Radius;

        void CalcBoundingBox();

        /// See the base class declaration for details
        void StoreDataIntoBVH(Data &data) const override;

        Primitive_t GetType() const override { return Primitive_t::DISC; }

    public:
        Disc();

        Disc(const Vec3f &center, const Vec3f &normal, float radius);

        /// Prints to 'os' the data at 'it' stored previously by StoreDataIntoBVH()
        static void PrintBVH(Data::const_iterator &it, std::ostream &os);
    };

    class Triangle: public Primitive
    {
        Vec3f Vert[3];

        void CalcBoundingBox()
        {
            Xmin = Ymin = Zmin = 9e+19f;
            Xmax = Ymax = Zmax = -9e+19f;

            for (int i = 0; i < 3; i++)
            {
                if (Vert[i].x < Xmin)
                    Xmin = Vert[i].x;

                if (Vert[i].x > Xmax)
                    Xmax = Vert[i].x;

                if (Vert[i].y < Ymin)
                    Ymin = Vert[i].y;

                if (Vert[i].y > Ymax)
                    Ymax = Vert[i].y;

                if (Vert[i].z < Zmin)
                    Zmin = Vert[i].z;

                if (Vert[i].z > Zmax)
                    Zmax = Vert[i].z;
            }
        }

        /// See the base class declaration for details
        void StoreDataIntoBVH(Data &data) const override;

        Primitive_t GetType() const override { return Primitive_t::TRIANGLE; }

    public:
        Triangle();

        Triangle(float v0x, float v0y, float v0z,
                 float v1x, float v1y, float v1z,
                 float v2x, float v2y, float v2z);

        Triangle(const Vec3f &v0, const Vec3f &v1, const Vec3f &v2);

        /// Prints to 'os' the data at 'it' stored previously by StoreDataIntoBVH()
        static void PrintBVH(Data::const_iterator &it, std::ostream &os);
    };

    class Cone: public Primitive
    {
        Vec3f Center1, Center2;
        float Radius1, Radius2;

        Vec3f UnitAxis; ///< Unit axis (Center2-Center1) vector
        float AxisLen;  ///< Length of (Center2-Center)
        float WidthCoeff; ///< Width coefficient
        float CosB;     ///< Cosine of the base angle
        float DotAxC1;  ///< Dot product of 'UnitAxis' and 'Center1'

        /** Adds primitive's contents at the end of 'data' in format suitable
            for later BVH traversal in a shader. */
        void StoreDataIntoBVH(Data &data) const override;

        Primitive_t GetType() const override { return CONE; }

    public:
        Cone(const Vec3f &center1, const Vec3f &center2, float radius1, float radius2);

        /// Prints to 'os' the data at 'it' stored previously by StoreDataIntoBVH()
        static void PrintBVH(Data::const_iterator &it, std::ostream &os);
    };
}


#endif
