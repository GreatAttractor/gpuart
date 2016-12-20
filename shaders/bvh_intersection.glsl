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
   Traversal of Bounding Volumes Hierarchy tree
*/

#version 330 core


#define VISIBILITY_OFFSET 1.0e-4


// Offsets below correspond with data layout produced by gpuart::BoundingVolumesHierarchy::Compile()
#define BVH_NODE_INFO_OFS  2
#define BVH_PRIM_DATA_OFS  3

/// Values (number of RGBA quads occupied) correspond with gpuart::Primitive::StoreIntoBVH()
#define SPHERE_DATA_LEN    1
#define DISC_DATA_LEN      2
#define TRIANGLE_DATA_LEN  3
#define CONE_DATA_LEN      4

#define BVH_LEAF        (1U<<31)
#define BVH_IS_LOWER    (1U<<30)
#define BVH_IS_ROOT     (1U<<29)

#define BVH_FLAGS_MASK  (BVH_LEAF | BVH_IS_LOWER | BVH_IS_ROOT)

#define NDINFO_FLAGS       0
#define NDINFO_LO_ADDR     1
#define NDINFO_HI_ADDR     2
#define NDINFO_PARENT_ADDR 3

// Primitive types
#define SPHERE   0
#define DISC     1
#define TRIANGLE 2
#define CONE     3


// External functions -------------------------------------

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
);

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
);

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
);

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
);


//---------------------------------------------------------

/// Returns address of the next primitive's data
int CheckBVHPrimitiveIntersection(
    in vec3 rstart, ///< Ray's origin
    in vec3 rdir,   ///< Ray's direction
    in int primitiveType,
    in samplerBuffer bvhTree,
    in int addr,    ///< Start of primitive data in 'bvhTree' (not including the primitive type)

    /** Satisfies: rstart + pos*rdir = intersection.
        Receives a value <0 if there is no intersection. */
    out float pos,
    out vec3 intersection, ///< Intersection coordinates
    out vec3 normal        ///< Unit normal at intersection (facing 'rstart')
)
{
    vec2 triangleUV;

    // For each primitive type its data is interpreted according to
    // gpuart::Primitive::StoreDataIntoBVH() implementations

    /* Using 'if' instead of 'switch' to work around a shader compiler bug (as of Mesa 11.1.0 (git-525f3c2)
       + Gallium 0.4 on AMD PITCAIRN (DRM 2.45.0, LLVM 3.7.0)). Otherwise, the disc's or cone's normal
       would somehow overwrite any normal returned earlier (but not those returned later).
       Perhaps the compiler outsmarts itself when handling all the "out" parameters passed around. */
       
    if (primitiveType == CONE)
    {
        vec4 centerRad1 = texelFetch(bvhTree, addr).xyzw;
        vec4 centerRad2 = texelFetch(bvhTree, addr+1).xyzw;
        vec4 unitAxisL  = texelFetch(bvhTree, addr+2).xyzw;
        vec3 params     = texelFetch(bvhTree, addr+3).xyz;
    
        ConeIntersection(
          rstart, rdir,
          
          centerRad1,
          centerRad2,
          unitAxisL,
          
          params.x, ///< Width coefficient
          params.y, ///< Cosine of base angle
          params.z, ///< Dot product of axis and first center
            
          pos, intersection, normal);

    
        if (pos < VISIBILITY_OFFSET)
            pos = -1;

        return addr + CONE_DATA_LEN;
    }
    else if (primitiveType == SPHERE)
    {
        vec4 sphere = texelFetch(bvhTree, addr).xyzw;

        SphereIntersection(
            rstart, rdir,

            sphere.xyz, // center
            sphere.w,   // radius

            pos, intersection, normal);

        if (pos < VISIBILITY_OFFSET)
            pos = -1;

        return addr + SPHERE_DATA_LEN;
    }
    else if (primitiveType == DISC)
    {
        vec4 discPosR   = texelFetch(bvhTree, addr).xyzw;
        vec3 discNormal = texelFetch(bvhTree, addr+1).xyz;

        DiscIntersection(
            rstart, rdir,
            discPosR.xyz, discPosR.w, discNormal,

            pos, intersection, normal);

        if (pos < VISIBILITY_OFFSET)
            pos = -1;

        return addr + DISC_DATA_LEN;
    }
    else if (primitiveType == TRIANGLE)
    {
        TriangleIntersection(
            rstart, rdir,
            texelFetch(bvhTree, addr).xyz,   // v0
            texelFetch(bvhTree, addr+1).xyz, // v1
            texelFetch(bvhTree, addr+2).xyz, // v2

            pos, intersection, normal, triangleUV);

        if (pos < VISIBILITY_OFFSET)
            pos = -1;

        return addr + TRIANGLE_DATA_LEN;
    }
}


//---------------------------------------------------------

/// Checks for a ray-AABB (axis-aligned bounding box) intersection
bool IntersectsAABB(
    in vec3 rstart, ///< Ray's starting point
    in vec3 rdir,   ///< Ray's direction
    in vec3 rdiv,   ///< 1/rdir (component-wise)
    in samplerBuffer bvhTree, ///< BVH tree
    in int addr,    ///< Address in 'bvhTree' of the AABB's definition
    
    /** Receives position of the closest (entering) intersection,
    such that rstart + pos*rdir gives its location.
    Receives -1 if 'rstart' lies inside the AABB. */ 
    out float pos
)
{
    vec3 bbmin = texelFetch(bvhTree, addr).xyz;
    vec3 bbmax = texelFetch(bvhTree, addr+1).xyz;

    if (all(greaterThanEqual(rstart, bbmin))
        && all(lessThanEqual(rstart, bbmax)))
    {
        pos = -1;
        return true;
    }
    else
    {
        float kx, ky, kz, x, y, z;
        bool intersected = false;

        pos = 1.0e+19;

        if (rdir.x != 0)
        {
            kx = (bbmin.x - rstart.x) * rdiv.x;
            if (kx >= 0)
            {
                y = rstart.y + kx*rdir.y;
                z = rstart.z + kx*rdir.z;

                if (y >= bbmin.y && y <= bbmax.y &&
                    z >= bbmin.z && z <= bbmax.z)
                {
                    intersected = true;
                    if (kx < pos)
                        pos = kx;
                }
            }

            kx = (bbmax.x - rstart.x) * rdiv.x;
            if (kx >= 0)
            {
                y = rstart.y + kx*rdir.y;
                z = rstart.z + kx*rdir.z;
                if (y >= bbmin.y && y <= bbmax.y &&
                    z >= bbmin.z && z <= bbmax.z)
                {
                    intersected = true;
                    if (kx < pos)
                        pos = kx;
                }
            }
        }

        if (rdir.y != 0)
        {
            ky = (bbmin.y - rstart.y) * rdiv.y;
            if (ky >= 0)
            {
                x = rstart.x + ky*rdir.x;
                z = rstart.z + ky*rdir.z;
                if (x >= bbmin.x && x <= bbmax.x &&
                    z >= bbmin.z && z <= bbmax.z)
                {
                    intersected = true;
                    if (ky < pos)
                        pos = ky;
                }
            }

            ky = (bbmax.y - rstart.y) * rdiv.y;
            if (ky >= 0)
            {
                x = rstart.x + ky*rdir.x;
                z = rstart.z + ky*rdir.z;
                if (x >= bbmin.x && x <= bbmax.x &&
                    z >= bbmin.z && z <= bbmax.z)
                {
                    intersected = true;
                    if (ky < pos)
                        pos = ky;
                }
            }
        }

        if (rdir.z != 0)
        {
            kz = (bbmin.z - rstart.z) * rdiv.z;
            if (kz >= 0)
            {
                x = rstart.x + kz*rdir.x;
                y = rstart.y + kz*rdir.y;
                if (x >= bbmin.x && x <= bbmax.x &&
                    y >= bbmin.y && y <= bbmax.y)
                {
                    intersected = true;
                    if (kz < pos)
                        pos = kz;
                }
            }

            kz = (bbmax.z - rstart.z) * rdiv.z;
            if (kz >= 0)
            {
                x = rstart.x + kz*rdir.x;
                y = rstart.y + kz*rdir.y;
                if (x >= bbmin.x && x <= bbmax.x &&
                    y >= bbmin.y && y <= bbmax.y)
                {
                    intersected = true;
                    if (kz < pos)
                        pos = kz;
                }
            }
        }

        return intersected;
    }
}

#define FROM_NONE 0
#define FROM_LO   1
#define FROM_HI   2

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
)
{
    int bvhIdx = 0; // Current node's address (index) in 'bvhTree'
    bool recursionReturn = false;
    int returningFrom = FROM_NONE;

    vec3 rdiv = 1/rdir;

    pos = -1;
    primitiveType = -1;
    float closestPos = 1e+19;

    vec4 nodeInfo;
    uint flags = 0U;

    do
    {
        nodeInfo = texelFetch(bvhTree, bvhIdx + BVH_NODE_INFO_OFS).rgba;
        flags = floatBitsToUint(nodeInfo[NDINFO_FLAGS]);

        if (recursionReturn && returningFrom == FROM_HI && (flags & BVH_IS_ROOT) == BVH_IS_ROOT)
            break;

        float bboxIntrPos;
        if (IntersectsAABB(rstart, rdir, rdiv, bvhTree, bvhIdx, bboxIntrPos))
        {
            if (bboxIntrPos > closestPos)
                recursionReturn = true;
            else if ((flags & BVH_LEAF) == BVH_LEAF)
            {
                uint numPrimitives = (flags & ~BVH_FLAGS_MASK);
                int primAddr = bvhIdx + BVH_PRIM_DATA_OFS;

                for (uint i = 0U; i < numPrimitives; i++)
                {
                    float currPos;
                    vec3 currIntersection, currNormal;
                    int ptype = int(floatBitsToUint(texelFetch(bvhTree, primAddr).r));

                    primAddr = CheckBVHPrimitiveIntersection(
									rstart, rdir, ptype,
									bvhTree, primAddr + 1, // +1 skips the stored 'ptype'
									currPos, currIntersection, currNormal);

                    if (currPos > 0 && currPos < closestPos)
                    {
                        closestPos = pos = currPos;
                        intersection = currIntersection;
                        normal = currNormal;
                        primitiveType = ptype;
                    }
                }

                recursionReturn = true;
            }
            else
            {
                recursionReturn = false;

                if (returningFrom == FROM_NONE)
                    bvhIdx = int(floatBitsToUint(nodeInfo[NDINFO_LO_ADDR]));
                else if (returningFrom == FROM_LO)
                {
                    returningFrom = FROM_NONE;
                    bvhIdx = int(floatBitsToUint(nodeInfo[NDINFO_HI_ADDR]));
                }
                else
                    recursionReturn = true;
            }
        }
        else
            recursionReturn = true;


        if (recursionReturn)
        {
            if ((flags & BVH_IS_LOWER) == BVH_IS_LOWER)
                returningFrom = FROM_LO;
            else // this also executes for root, which does not have BVH_IS_LOWER set; in effect, we break at the next iteration's start
                returningFrom = FROM_HI;

            bvhIdx = int(floatBitsToUint(nodeInfo[NDINFO_PARENT_ADDR]));
        }

    } while (true);
}
