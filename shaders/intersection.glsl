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
    Function to check ray intersection with scene and the user sphere
*/

#version 330 core


// Primitive types
#define SPHERE   0
#define DISC     1
#define TRIANGLE 2

#define PRIMITIVE_TYPE 0
#define PRIMITIVE_ADDR 1

#define VISIBILITY_OFFSET 1.0e-4


// External functions -------------------------------------

void SphereIntersection(
    in vec3 rstart,  ///< Ray's origin
    in vec3 rdir,    ///< Ray's direction
    in vec3 center,  ///< Sphere's center
    in float radius, ///< Sphere's radius

    /** Satisfies: rstart + pos*rdir = intersection.
        Receives a value <0 if there is no intersection. */
    out float pos,
    out vec3 intersection, ///< Intersection coordinates
    out vec3 normal  ///< Sphere's unit normal at intersection
);

void CheckBVHIntersection(
    in vec3 rstart,
    in vec3 rdir,
    
    in samplerBuffer bvhTree,
    
    /** Satisfies: rstart + pos*rdir = intersection.
        Receives a value <0 if there is no intersection. */
    out float pos,
    out vec3 intersection, ///< Intersection coordinates
    out vec3 normal,       ///< Unit normal at intersection (facing 'rstart')
    out int primitiveType  ///< Type of the intersected primitive
);


// --------------------------------------------------------

/// Checks intersections with all primitives and the user-controlled sphere
void CheckIntersectionInclUserSphere(
    in vec3 rstart,  ///< Ray's origin
    in vec3 rdir,    ///< Ray's direction

    in samplerBuffer bvhTree,

    in vec4 userSphere, ///< User sphere's position and radius

    /** Satisfies: rstart + pos*rdir = intersection.
        Receives a value <0 if there is no intersection. */
    out float pos,
    out vec3 intersection, ///< Intersection coordinates
    out vec3 normal,       ///< Unit normal at intersection (facing 'rstart')
    out int primitiveType,
    out bool userSphereHit
)
{
    CheckBVHIntersection(
        rstart, rdir, bvhTree,

        pos, intersection, normal, primitiveType);

    float usPos;
    vec3 usIntersection, usNormal;

    SphereIntersection(
        rstart, rdir,
        userSphere.xyz, userSphere.w,
        usPos, usIntersection, usNormal);

    if (usPos > VISIBILITY_OFFSET && (pos < 0 || usPos < pos))
    {
        userSphereHit = true;
        primitiveType = SPHERE;
        pos = usPos;
        intersection = usIntersection;
        normal = usNormal;
    }
    else
        userSphereHit = false;
}
