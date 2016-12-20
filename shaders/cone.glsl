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
   Cone-ray intersection calculations
*/

#version 330 core


#define VISIBILITY_OFFSET 1.0e-4
#define CONE_TOLERANCE    1.0e-7

void ConeIntersection(
    in vec3 rstart,  ///< Ray's origin
    in vec3 rdir,    ///< Ray's direction
    
    in vec4  centerRad1, ///< First center and radius
    in vec4  centerRad2, ///< Second center and radius
    in vec4  unitAxisL,  ///< Unit axis and axis length
    in float widthCoeff, ///< Width coefficient
    in float cosB,       ///< Cosine of base angle
    in float dotAxC1,    ///< Dot product of axis and first center

    /** Satisfies: rstart + pos*rdir = intersection.
        Receives a value <0 if there is no intersection. */
    out float pos,
    out vec3 intersection, ///< Intersection coordinates
    out vec3 normal  ///< Unit normal at intersection (facing 'rstart')
)
{
    vec3 D = dot(unitAxisL.xyz, rdir) * unitAxisL.xyz;
    vec3 E = -rdir;
    vec3 F = centerRad1.xyz + dot(unitAxisL.xyz, rstart) * unitAxisL.xyz - dotAxC1 * unitAxisL.xyz - rstart;
    float G = widthCoeff * dot(unitAxisL.xyz, rdir);
    float H = widthCoeff * dot(unitAxisL.xyz, rstart) - widthCoeff * dotAxC1 + centerRad1.w;
    
    // Coefficients of the quadratic equation for the unknown 'k'
    // such that s + k*r intersects the unbounded cone
    
    float A = dot(D, D) + dot(E, E) + 2*dot(D, E) - G*G;
    float B = 2 * dot(F, D+E) - 2*G*H;
    float C = dot(F, F) - H*H;
    
    if (abs(A) < CONE_TOLERANCE)
    {
        pos = -1;
        return;
    }
    
    float delta = B*B - 4*A*C;
    
    
    if (delta < CONE_TOLERANCE)
    {
        pos = -1;
        return;
    }
    
    float sqrtdelta = sqrt(delta);
    
    float k1 = (-B + sqrtdelta)/(A+A);
    float k2 = (-B - sqrtdelta)/(A+A);
    
    
    // Intersection points
    vec3 p1 = rstart + k1*rdir;
    vec3 p2 = rstart + k2*rdir;
    
    float t1 = dot(unitAxisL.xyz, p1 - centerRad1.xyz);
    float t2 = dot(unitAxisL.xyz, p2 - centerRad1.xyz);
    
    bool onaxis1 = t1 >= 0 && t1 <= unitAxisL.w;
    bool onaxis2 = t2 >= 0 && t2 <= unitAxisL.w;
    
    
    if (k1 < VISIBILITY_OFFSET && onaxis2)
    {
        pos = k2;
        intersection = p2;
    }
    else if (k2 < VISIBILITY_OFFSET && onaxis1)
    {
        pos = k1;
        intersection = p1;
    }
    else
    {
        if (k1 < k2 && onaxis1 && onaxis2
            || onaxis1 && !onaxis2)
        {
            pos = k1;
            intersection = p1;
        }
        else if (k2 < k1 && onaxis1 && onaxis2
            || !onaxis1 && onaxis2)
        {
            pos = k2;
            intersection = p2;
        }
        else
        {
            pos = -1;
            return;
        }
    }
    
    if (pos > 0)
    {
        // Intersection point projected onto the cone axis
        vec3 proj = centerRad1.xyz + dot(unitAxisL.xyz, (intersection - centerRad1.xyz)) * unitAxisL.xyz;
        
        vec3 n1 = normalize(intersection - proj); //TODO: normalization neccessary here?
        float u = cosB - dot(n1, unitAxisL.xyz);
        normal = normalize(u * unitAxisL.xyz + n1);
        if (dot(normal, rdir) > 0)
            normal = -normal;
    }
}
