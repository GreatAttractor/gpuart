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
   Sphere-ray intersection calculations
*/

#version 330 core


#define VISIBILITY_OFFSET 1.0e-4

float sqrlen(in vec3 v);

void SphereIntersection(
    in vec3 rstart,  ///< Ray's origin
    in vec3 rdir,    ///< Ray's direction
    in vec3 center,  ///< Sphere's center
    in float radius, ///< Sphere's radius

    /** Satisfies: rstart + pos*rdir = intersection.
        Receives a value <0 if there is no intersection. */
    out float pos,
    out vec3 intersection, ///< Intersection coordinates
    out vec3 normal  ///< Unit normal at intersection (facing 'rstart')
)
{
    float a = dot(rdir, rdir);
    float b = 2 * dot(rdir, (rstart - center));
    float c = sqrlen(rstart - center) - radius*radius;

    float delta = b*b - 4*a*c;

    if (delta >= 0)
    {
        float sd = sqrt(delta);
        float k1 = (-b + sd) / (a + a);
        float k2 = (-b - sd) / (a + a);

        if (k1 < VISIBILITY_OFFSET)
            pos = k2;
        else if (k2 < VISIBILITY_OFFSET)
            pos = k1;
        else
            pos = (k1 < k2 ? k1 : k2);

        intersection = rstart + rdir * pos;
        normal = normalize(intersection - center);
        if (dot(rstart - intersection, normal) < 0)
            normal = -normal;
    }
    else
        pos = -1;
}
