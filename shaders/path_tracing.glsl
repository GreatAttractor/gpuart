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
   Rendering stage: path tracing
*/

#version 330 core


// Primitive types
#define SPHERE   0
#define DISC     1
#define TRIANGLE 2
#define CONE     3


// External functions -------------------------------------

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

/// Returns a random unit (TODO: are we sure?) direction within a hemisphere around the unit vector 'v'
vec3 GetRandomHemisphereDirection(in vec3 v, in vec3 randInput);

/** Returns a random vector inside a cone around 'v' with the specified apex angle,
    but not intersecting the surface given by 'normal', i.e. the returned vector
    is at less than 90 degrees to 'normal'. */ 
vec3 GetRandomDirectionInsideCone(
    in vec3 v,          ///< Cone axis
    in vec3 normal,     ///< Surface normal (unit)
    in float halfAngle, ///< 1/2 of cone's apex angle  
    in vec3 randInput
);

vec3 GetSkyColor(
    /// Direction towards the sky to return the sky color for
    in vec3 dir,

    /// Direction towards the Sun (unit) and its altitude
    in vec4 sunDirAlt
);

// Pseudo-random value in half-open range [0:1]
float random(in float x);


// ---------------------------------------------------------

//TESTING: hard-coded light properties
const float skyIntensity = 1.0;


// Inputs -------------------------------------------------

in vec2 UV; ///< Texture coordinates (ray index) in the input 2D samplers

uniform sampler2D RStart;  ///< Ray's starting point
uniform sampler2D RDir;    ///< Ray's direction
uniform vec4 SunDirAlt;    ///< Direction towards the Sun (unit) and its altitude (radians)
uniform int SunDirectLightingEnabled;

uniform vec4 UserSphere;   ///< Center and radius of the user-controlled sphere
uniform vec3 UserSphereEm; ///< User sphere's emittance (may be all zeros)
uniform int  UserSphereEmNonZero;

uniform vec4 RandSeed; ///< Differs for every pass, used for random-seeding
uniform int NumPathsPerPixel; ///< Paths/pixel to trace in this pass
uniform float PixelSize; ///< Pixel size in logical (world) coordinate system
uniform vec3 CameraPos;

uniform samplerBuffer BVH; ///< Bounding Volumes Hierarchy tree with the scene's primitives

uniform sampler2D PrevRadiance; ///< Radiance calculated in previous passes

// Values correspond with UserSphereFlags (gpuart::Renderer)
#define USPH_EM_NONZERO   (1U<<0)
#define USPH_SPECULAR     (1U<<1)
#define USPH_FUZZY        (1U<<2)

uniform uint UserSphereFlags;


// Outputs -------------------------------------------------

layout(location = 0) out vec3 out_Radiance;


// ---------------------------------------------------------

/// Indexed by primitive type
const vec3 PRIMITIVE_COLOR[] = vec3[](vec3(0.65, 0.4, 0.35), // sphere
                                      vec3(0.1, 0.2, 0.1),   // disc
                                      vec3(0.3, 0.3, 0.3),  // triangle
                                      vec3(0.3, 0.3, 0.3)); // cone



const vec3 SKY_LIGHT_INTENSITY = 2*vec3(1, 1, 1);
const float FUZZY_ANGLE = 10 * 3.14159/180;

void main()
{
    float pos;
    vec3 intersection, normal;

    const int MAX_PATH_SEGMENTS = 5;
    const vec3 MIN_WEIGHT = vec3(0.01, 0.01, 0.01);

    vec3 rstart0 = texture(RStart, UV).xyz;
    vec3 rdir0   = texture(RDir, UV).xyz;

    vec3 rdirOrtho1;
    if (any(greaterThan(abs(rdir0.xy), vec2(1.0e-5, 1.0e-5))))
        rdirOrtho1 = normalize(vec3(rdir0.y, -rdir0.x, 0));
    else
        rdirOrtho1 = normalize(vec3(0, -rdir0.z, rdir0.y));

    vec3 rdirOrtho2 = cross(normalize(rdir0), rdirOrtho1);

    vec3 color = vec3(0, 0, 0);
    for (int j = 0; j < NumPathsPerPixel; j++)
    {
        /* Dither the camera ray's starting point and direction in order to:

           - provide anti-aliasing

           - ensure the random directions of subsequent path segments
             differ between paths: we use the intersection location as the
             pseudo-random generator's input for the
             GetRandomHemisphereDirection() call
        */
        float rand1 = random(RandSeed.x+j);
        float rand2 = random(RandSeed.y+j);

        vec3 rstart = rstart0 + (rand1 - 0.5) * rdirOrtho1 * PixelSize
                              + (rand2 - 0.5) * rdirOrtho2 * PixelSize;

        vec3 rdir = rstart - CameraPos;

        vec3 pathColor = vec3(0, 0, 0);
        vec3 colorWeight = vec3(1, 1, 1);
        bool userSphereHit = false;
        bool specularReflection;
        int i;
        for (i = 0; i < MAX_PATH_SEGMENTS && all(greaterThan(colorWeight, MIN_WEIGHT)); i++)
        {
            int ptype;


            CheckIntersectionInclUserSphere(
                rstart, rdir,
                BVH, UserSphere,

                pos, intersection, normal, ptype, userSphereHit);

            if (userSphereHit)
            {
                if ((UserSphereFlags & USPH_EM_NONZERO) != 0U)
                {
                    pathColor += UserSphereEm * colorWeight;
                    break;
                }
                else
                    ptype = SPHERE;
            }
            else if (ptype == -1) // ray hits the background
            {
                pathColor += SKY_LIGHT_INTENSITY * GetSkyColor(rdir, SunDirAlt) * colorWeight;
                break;
            }

            colorWeight *= PRIMITIVE_COLOR[ptype];

            rstart = intersection;

            if (userSphereHit && (UserSphereFlags & USPH_SPECULAR) != 0U)
            {
                if ((UserSphereFlags & USPH_FUZZY) == 0U)
                    rdir = reflect(rdir, normal);
                else                    
                    rdir = GetRandomDirectionInsideCone(reflect(rdir, normal), normal, FUZZY_ANGLE,
                                                        intersection + RandSeed.xyz);
                
                specularReflection = true;
            }
            else
            {
                rdir = GetRandomHemisphereDirection(normal, intersection + RandSeed.xyz);
                specularReflection = false;
            }

            // Sun's direct lighting contribution ----------------
            if (SunDirectLightingEnabled == 1 && !specularReflection)
            {
                float sunPos;
                vec3 sunIntersection, dummy;
                int sunPType;

                CheckIntersectionInclUserSphere(
                    intersection,
                    SunDirAlt.xyz,

                    BVH, UserSphere,

                    sunPos, sunIntersection, dummy, sunPType, userSphereHit);

                if (sunPType == -1)
                {
                    float dotp = dot(SunDirAlt.xyz, normal);
                    if (dotp > 0)
                        pathColor += dotp * PRIMITIVE_COLOR[ptype];
                }
            }
        }
        if (i == 0 && !userSphereHit) // ray hits the background directly
            pathColor = GetSkyColor(rdir0, SunDirAlt);
        else if (i == 0 && userSphereHit && !specularReflection)
            pathColor = vec3(1, 1, 1);

        color += pathColor;
    }

    out_Radiance = texture(PrevRadiance, UV).rgb + color;
}
