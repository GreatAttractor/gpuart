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
   Disc-ray intersection calculations
*/


#version 330 core


float sqrlen(in vec3 v);

void DiscIntersection(
    in vec3 rstart,  ///< Ray's origin
    in vec3 rdir,    ///< Ray's direction
    in vec3 center,  ///< Disc's center
    in float radius, ///< Disc's radius
    in vec3 dnormal, ///< Disc's unit normal

    /** Satisfies: rstart + pos*rdir = intersection.
        Receives a value <0 if there is no intersection. */
    out float pos,
    out vec3 intersection, ///< Intersection coordinates
    out vec3 normal        ///< Unit normal at intersection (facing 'rstart')
)
{
    float tmp = dot(rdir, dnormal);
    if (abs(tmp) < 1.0e-8)
    {
        pos = -1;
        return;
    }

    float k = dot(dnormal, center - rstart) / tmp;

    if (k <= 0)
    {
        pos = -1;
        return;
    }

    vec3 q = k * rdir + rstart;

    if (sqrlen(q - center) <= radius*radius)
    {
        pos = k;
        intersection = rstart + k*rdir;
        if (dot(rstart - center, dnormal) > 0)
            normal = dnormal;
        else
            normal = -dnormal;
    }
    else
        pos = -1;
}
