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
    Core classes implementation
*/

#include <algorithm>
#include "core.h"


static
void PushVector(gpuart::Primitive::Data &data, const gpuart::Vec3f &v)
{
    data.push_back(v.x);
    data.push_back(v.y);
    data.push_back(v.z);
}

void gpuart::Sphere::CalcBoundingBox()
{
    Xmin = Center.x - Radius;
    Xmax = Center.x + Radius;

    Ymin = Center.y - Radius;
    Ymax = Center.y + Radius;

    Zmin = Center.z - Radius;
    Zmax = Center.z + Radius;
}

gpuart::Sphere::Sphere(): Center(0, 0, 0), Radius(1)
{
    CalcBoundingBox();
}

gpuart::Sphere::Sphere(const Vec3f &center, float radius): Center(center), Radius(radius)
{
    CalcBoundingBox();
}

/// See the base class declaration for details
void gpuart::Sphere::StoreDataIntoBVH(Data &data) const
{
    data.push_back(Center.x);
    data.push_back(Center.y);
    data.push_back(Center.z);
    data.push_back(Radius);
}

/// Prints to 'os' the data at 'it' stored previously by StoreDataIntoBVH()
void gpuart::Sphere::PrintBVH(Data::const_iterator &it, std::ostream &os)
{
    os << "{ (" << *it++; // Center.x
    os << ", " << *it++;  // Center.y
    os << ", " << *it++;  // Center.z

    os << "), " << *it++ << " }"; // Radius
}


//---------------------------------------------------------

void gpuart::Disc::CalcBoundingBox()
{
    Xmin = Center.x - Radius;
    Xmax = Center.x + Radius;

    Ymin = Center.y - Radius;
    Ymax = Center.y + Radius;

    Zmin = Center.z - Radius;
    Zmax = Center.z + Radius;
}

gpuart::Disc::Disc(const Vec3f &center, const Vec3f &normal, float radius)
    : Center(center), Normal(normal), Radius(radius)
{
    CalcBoundingBox();
}

gpuart::Disc::Disc(): Center(0, 0, 0), Normal(0, 0, 1), Radius(1)
{
    CalcBoundingBox();
}

/// See the base class declaration for details
void gpuart::Disc::StoreDataIntoBVH(Data &data) const
{
    data.push_back(Center.x);
    data.push_back(Center.y);
    data.push_back(Center.z);
    data.push_back(Radius);

    data.push_back(Normal.x);
    data.push_back(Normal.y);
    data.push_back(Normal.z);
    data.push_back(RGBA_PAD);
}

/// Prints to 'os' the data at 'it' stored previously by StoreDataIntoBVH()
void gpuart::Disc::PrintBVH(Data::const_iterator &it, std::ostream &os)
{
    os << "{ (" << *it++; // Center.x
    os << ", " << *it++;  // Center.y
    os << ", " << *it++;  // Center.z

    os << "), " << *it++; // Radius

    os << ", (" << *it++;         // Normal.x
    os << ", " << *it++;          // Normal.y
    os << ", " << *it++ << ") }"; // Normal.z

    it++; // skip RGBA padding
}


//---------------------------------------------------------

gpuart::Triangle::Triangle()
{
    Vert[0] = Vec3f(0, 0, 0); Vert[1] = Vec3f(1, 0, 0); Vert[2] = Vec3f(1, 1, 0);
    CalcBoundingBox();
}

gpuart::Triangle::Triangle(float v0x, float v0y, float v0z,
                           float v1x, float v1y, float v1z,
                           float v2x, float v2y, float v2z)
{
    Vert[0] = Vec3f(v0x, v0y, v0z);
    Vert[1] = Vec3f(v1x, v1y, v1z);
    Vert[2] = Vec3f(v2x, v2y, v2z);
    CalcBoundingBox();
}

gpuart::Triangle::Triangle(const Vec3f &v0, const Vec3f &v1, const Vec3f &v2)
{
    Vert[0] = v0; Vert[1] = v1; Vert[2] = v2;
    CalcBoundingBox();
}

/// See the base class declaration for details
void gpuart::Triangle::StoreDataIntoBVH(Data &data) const
{
    for (int i = 0; i < 3; i++)
    {
        data.push_back(Vert[i].x);
        data.push_back(Vert[i].y);
        data.push_back(Vert[i].z);
        data.push_back(RGBA_PAD);
    }
}

/// Prints to 'os' the data at 'it' stored previously by StoreDataIntoBVH()
void gpuart::Triangle::PrintBVH(Data::const_iterator &it, std::ostream &os)
{
    os << "{ ";
    for (int i = 0; i < 3; i++)
    {
        os << "(" << *it++;          // v0.x
        os << ", " << *it++;         // v0.y
        os << ", " << *it++ << ")";  // v0.z

        if (i < 2)
            os << ", ";

        it++; // skip RGBA padding
    }
    os << " }";
}


//---------------------------------------------------------

gpuart::Cone::Cone(const Vec3f &center1, const Vec3f &center2, float radius1, float radius2)
:  Center1(center1), Center2(center2),
   Radius1(radius1), Radius2(radius2)
{
    Vec3d vc1(center1), vc2(center2);

    AxisLen = (float)(vc2 - vc1).length();
    Vec3d vd = (vc2 - vc1) / AxisLen;
    UnitAxis = vd;
    WidthCoeff = (Radius2 - Radius1) / AxisLen;

    if (fabs(Radius1 - Radius2) < 1.0e-7)
        CosB = 0.0f;
    else if (Radius1 > Radius2)
    {
        float h = Radius1 * AxisLen / (Radius1 - Radius2);
        CosB = (float)(Radius1 / sqrt((double)h*h + (double)Radius1 * Radius1));
    }
    else
    {
        float h = Radius2 * AxisLen / (Radius2 - Radius1);
        CosB = (float)(-Radius2 / sqrt((double)h*h + (double)Radius2*Radius2));
    }

    DotAxC1 = (float)(vd*vc1);

    // This bounding box is slightly bigger than necessary (as if the cone had hemispherical caps)
    Xmin = std::min(Center1.x - Radius1, Center2.x - Radius2);
    Xmax = std::max(Center1.x + Radius1, Center2.x + Radius2);

    Ymin = std::min(Center1.y - Radius1, Center2.y - Radius2);
    Ymax = std::max(Center1.y + Radius1, Center2.y + Radius2);

    Zmin = std::min(Center1.z - Radius1, Center2.z - Radius2);
    Zmax = std::max(Center1.z + Radius1, Center2.z + Radius2);
}

/** Adds primitive's contents at the end of 'data' in format suitable
    for later BVH traversal in a shader. */
void gpuart::Cone::StoreDataIntoBVH(Data &data) const
{
    PushVector(data, Center1);
    data.push_back(Radius1);

    PushVector(data, Center2);
    data.push_back(Radius2);

    PushVector(data, UnitAxis);
    data.push_back(AxisLen);

    data.push_back(WidthCoeff);
    data.push_back(CosB);
    data.push_back(DotAxC1);
    data.push_back(RGBA_PAD);
}

/// Prints to 'os' the data at 'it' stored previously by StoreDataIntoBVH()
void gpuart::Cone::PrintBVH(Data::const_iterator &it, std::ostream &os)
{
    os << "{ ";

    // Center1
    os << "(" << *it++;
    os << ", " << *it++;
    os << ", " << *it++ << "), ";
    it++; // skip RGBA padding

    // Center2
    os << "(" << *it++;
    os << ", " << *it++;
    os << ", " << *it++ << "), ";
    it++; // skip RGBA padding

    // UnitAxis
    os << "(" << *it++;
    os << ", " << *it++;
    os << ", " << *it++ << "), ";
    it++; // skip RGBA padding

    os << *it++ << ", "; // Radius1
    os << *it++ << ", "; // Radius2
    os << *it++ << ", "; // AxisLen
    os << *it++ << ", "; // A

    os << *it++ << ", "; // CosB
    os << *it++; // DotAxC1

    it+=2; // skip RGBA padding

    os << " }";
}
