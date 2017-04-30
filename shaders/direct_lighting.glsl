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
   Rendering stage: calculate direct lighting
*/

#version 330 core


// Primitive types
#define SPHERE   0
#define DISC     1
#define TRIANGLE 2
#define CONE     3

#define MAX_REFLECTIONS 1

// External functions -------------------------------------

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
);

vec3 GetSkyColor(
    /// Direction towards the sky to return the sky color for
    in vec3 dir,

    /// Direction towards the Sun (unit) and its altitude
    in vec4 sunDirAlt
);


// ---------------------------------------------------------

const float lightIntensity[] = float[](1.0);
const float AMBIENT_INTENSITY = 0.15;

/// Indexed by primitive type
const vec3 PRIMITIVE_COLOR[] = vec3[](vec3(0.65, 0.4, 0.35), // sphere
                                      vec3(0.1, 0.2, 0.1),   // disc
                                      vec3(0.3, 0.3, 0.3),  // triangle
                                      vec3(0.3, 0.3, 0.3)); // cone

vec3 GetLambertShadedDiffuseColor(
    in vec3 lightDir, ///< Unit vector
    in vec3 normal,   ///< Unit vector
    in vec3 diffuseColor,
    in float lightIntensity)
{
    float dotp = dot(lightDir, normal);
    if (dotp > 0)
        return diffuseColor * lightIntensity * dotp;
    else
        return vec3(0, 0, 0);
}

// Inputs -------------------------------------------------

in vec2 UV; ///< Texture coordinates (ray index) in the input 2D samplers

uniform sampler2D RStart;  ///< Ray's starting point
uniform sampler2D RDir;    ///< Ray's direction

uniform vec4 SunDirAlt;    ///< Direction towards the Sun (unit) and its altitude (radians)
uniform int SunDirectLightingEnabled;

uniform samplerBuffer BVH;



uniform vec4 UserSphere;   ///< Center and radius of the user-controlled sphere
uniform vec3 UserSphereEm; ///< User sphere's emissivity (may be all zeros)

// Values correspond with UserSphereFlags (gpuart::Renderer)
#define USPH_EM_NONZERO   (1U<<0)
#define USPH_SPECULAR     (1U<<1)
#define USPH_FUZZY        (1U<<2)

uniform uint UserSphereFlags;


// Outputs -------------------------------------------------

layout(location = 0) out vec3 out_Irradiance;


// ---------------------------------------------------------

void main()
{
    float pos;
    vec3 intersection, normal;
    int primitiveType;
    bool userSphereHit;

    vec3 rdir = texture(RDir, UV).xyz,
         rstart = texture(RStart, UV).xyz;

    vec3 colorWeight = vec3(1, 1, 1);
    out_Irradiance = vec3(0, 0, 0);

    for (int i = 0; i <= MAX_REFLECTIONS; i++)
    {
    
        CheckIntersectionInclUserSphere(
            rstart, rdir,
            BVH,
            UserSphere,

            pos, intersection, normal, primitiveType, userSphereHit);

        if ((UserSphereFlags & USPH_SPECULAR) != 0U && userSphereHit)
        {
            rstart = intersection;
            rdir = reflect(rdir, normal);
            colorWeight *= PRIMITIVE_COLOR[SPHERE];
        }
        else if ((UserSphereFlags & USPH_EM_NONZERO) != 0U && userSphereHit)
        {
            out_Irradiance = vec3(1, 1, 1);
        }
        else
        {
            if (primitiveType != -1)
            {
                vec3 diffuseColor = PRIMITIVE_COLOR[primitiveType] * colorWeight;
                vec3 dummy1, dummy2;

                if (SunDirectLightingEnabled == 1)
                {
                    CheckIntersectionInclUserSphere(
                        intersection, SunDirAlt.xyz,
                        BVH,
                        UserSphere,

                        pos, dummy1, dummy2, primitiveType, userSphereHit);

                    if (primitiveType == -1)
                        out_Irradiance += GetLambertShadedDiffuseColor(SunDirAlt.xyz, normal, diffuseColor, lightIntensity[0]);
                }

                if ((UserSphereFlags & USPH_EM_NONZERO) != 0U)
                {
                    vec3 dirToSphere = UserSphere.xyz - intersection;
                    float dist = length(dirToSphere);
                    
                    CheckBVHIntersection(intersection, dirToSphere/dist, BVH,
                                        pos, dummy1, dummy2, primitiveType);
                                        
                    if (primitiveType == -1 || pos > dist)
                        out_Irradiance += GetLambertShadedDiffuseColor(dirToSphere/dist, normal, diffuseColor, 1) / (dist*dist);
                }

                out_Irradiance += AMBIENT_INTENSITY * diffuseColor;
            }
            else
                out_Irradiance = colorWeight * GetSkyColor(rdir, SunDirAlt);

            break;
        }
    }
}
