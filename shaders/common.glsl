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
   Common utilities
*/

#version 330 core


// External functions -------------------------------------

// Pseudo-random value in half-open range [0:1].
float random(in float x);
float random(in vec2  v);
float random(in vec3  v);
float random(in vec4  v);

// --------------------------------------------------------


float sqrlen(in vec3 v) { return dot(v, v); }

vec3 GetOrthogonal(vec3 v)
{
    if (all(lessThan(abs(v.xy), vec2(1.0e-6, 1.0e-6))))
        return vec3(1, 0, 0);
    else
        return normalize(vec3(v.y, -v.x, 0));
}

/// Returns a random unit (TODO: are we sure?) direction within a hemisphere around the unit vector 'v'
vec3 GetRandomHemisphereDirection(in vec3 v, in vec3 randInput)
{
    // Uses cosine-weighted distribution
    const float PIDBL = 3.1415926*2;

    float _2pr1 = PIDBL * random(randInput.xyz);
    float r2 = random(randInput.zxy);
    float sr2 = sqrt(1.0 - r2);

    float x = cos(_2pr1)*sr2,
          y = sin(_2pr1)*sr2,
          z = sqrt(r2);


    vec3 tangent = GetOrthogonal(v);

    return tangent * x + cross(v, tangent) * y + (v*z);
}

/// Returns 'v' rotated around 'axis' (unit) by an angle with the specified sine and cosine
vec3 rotate(in vec3 v, in vec3 axis, in float sine, in float cosine)
{
    mat3 M = mat3((axis.x*axis.x+(1-axis.x*axis.x)*cosine), (axis.x*axis.y*(1-cosine)-axis.z*sine),   (axis.x*axis.z*(1-cosine)+axis.y*sine),
                  (axis.x*axis.y*(1-cosine)+axis.z*sine),   (axis.y*axis.y+(1-axis.y*axis.y)*cosine), (axis.y*axis.z*(1-cosine)-axis.x*sine),
                  (axis.x*axis.z*(1-cosine)-axis.y*sine),   (axis.y*axis.z*(1-cosine)+axis.x*sine),   (axis.z*axis.z+(1-axis.z*axis.z)*cosine));
                  
    return M*v;
}

/** Returns a random vector inside a cone around 'v' with the specified apex angle,
    but not intersecting the surface given by 'normal', i.e. the returned vector
    is at less than 90 degrees to 'normal'. */ 
vec3 GetRandomDirectionInsideCone(
    in vec3 v,          ///< Cone axis
    in vec3 normal,     ///< Surface normal (unit)
    in float halfAngle, ///< 1/2 of cone's apex angle  
    in vec3 randInput
)
{
    float a = random(randInput.xy) * halfAngle;
    float b = random(randInput.yz) * 2 * 3.14159;
    
    float sina = sin(a), cosa = cos(a),
          sinb = sin(b), cosb = cos(b);
          
    // sine of the angle between 'normal' and reflected 'v'
    float sinc = length(cross(-v, normal))/length(v);
    if (cosa < sinc)
    {
        cosa = sinc;
        sina = 1-cosa*cosa;
    } 

    vec3 vOrtho = GetOrthogonal(v);
     
    vec3 w = normalize(rotate(v, vOrtho, sina, cosa));
    return rotate(w, v, sinb, cosb);
}
