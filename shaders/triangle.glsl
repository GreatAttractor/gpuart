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
   Triangle-ray intersection calculations
*/

#version 330 core


#define TRIANGLE_SURFACE_TOLERANCE 1.0e-10

/*
    Code based on "Fast, Minimum Storage Ray/Triangle Intersection"
    by T. Moeller & B. Trumbore
*/
void TriangleIntersection(
    in vec3 rstart, ///< Ray's origin
    in vec3 rdir,   ///< Ray's direction

    in vec3 v0,
    in vec3 v1,
    in vec3 v2,

    /** Satisfies: rstart + pos*rdir = intersection.
        Receives a value <0 if there is no intersection. */
    out float pos,
    out vec3 intersection, ///< Intersection coordinates
    out vec3 normal,       ///< Unit normal at intersection (facing 'rstart')
    out vec2 uv            ///< Barycentric coordinates of intersection
)
{
	vec3 edge1, edge2, tvec, pvec, qvec;
	float det, invDet;
    edge1 = v1 - v0;
	edge2 = v2 - v0;
	pvec = cross(rdir, edge2);
	det = dot(edge1, pvec);
	if (abs(det) < TRIANGLE_SURFACE_TOLERANCE)
    {
		pos = -1;
		return;
    }

	invDet = 1/det;
	tvec = rstart - v0;

	uv.x = dot(tvec, pvec) * invDet;
	if (uv.x < 0 || uv.x > 1)
    {
        pos = -1;
		return;
    }
	qvec = cross(tvec, edge1);
	uv.y = dot(rdir, qvec) * invDet;
	if (uv.y < 0 || uv.x + uv.y > 1)
        pos = -1;
    else
    {
        pos = dot(edge2, qvec) * invDet;
        intersection = rstart + rdir * pos;
        normal = normalize(cross(edge1, edge2));
        if (dot(rstart - intersection, normal) < 0)
            normal = -normal;
    }
}
