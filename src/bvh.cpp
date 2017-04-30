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
    Bounding Volumes Hierarchy implementation
*/

#include <algorithm>
#include <cassert>
#include "bvh.h"


using gpuart::Primitive;

static_assert(sizeof(GLfloat) == sizeof(uint32_t), "BVH code requires the sizes of GLfloat and uint32_t to be the same.");

/**  Divides the 'primitives' with indices between 'from' (incl.) and 'two' (excl.)
     along the longest spanned axis. */
void gpuart::BoundingVolumesHierarchy::Subdivide(gpuart::BoundingBox *node, std::vector<Primitive*> &primitives, size_t from, size_t to,
                                                 unsigned currentLevel, unsigned maxNumLevels, unsigned minPrimitivesPerNode)
{
    if (!node)
        return;

    // Bounding box of all primitives from the range [from, to)
    float xminall = 99.0e+29f, xmaxall = -99.0e+29f,
          yminall = 99.0e+29f, ymaxall = -99.0e+29f,
          zminall = 99.0e+29f, zmaxall = -99.0e+29f;

    // Find the largest-extent axis of the primitives
    for (size_t i = from; i < to; i++)
    {
        float xmin = primitives[i]->GetXmin(), xmax = primitives[i]->GetXmax(),
            ymin = primitives[i]->GetYmin(), ymax = primitives[i]->GetYmax(),
            zmin = primitives[i]->GetZmin(), zmax = primitives[i]->GetZmax();

        if (xmin < xminall) xminall = xmin;
        if (xmax > xmaxall) xmaxall = xmax;

        if (ymin < yminall) yminall = ymin;
        if (ymax > ymaxall) ymaxall = ymax;

        if (zmin < zminall) zminall = zmin;
        if (zmax > zmaxall) zmaxall = zmax;
    }

    float xrange = xmaxall - xminall;
    float yrange = ymaxall - yminall;
    float zrange = zmaxall - zminall;

    node->xmin = xminall;
    node->xmax = xmaxall;
    node->ymin = yminall;
    node->ymax = ymaxall;
    node->zmin = zminall;
    node->zmax = zmaxall;

    if (to - from <= minPrimitivesPerNode || currentLevel == maxNumLevels-1)
    {
        for (auto i = primitives.begin() + from; i != primitives.begin() + to; ++i)
            node->primitives.push_back(*i);

        return;
    }

    // Subdivide along the longest spanned axis --------

    auto iterTo = primitives.begin() + to;

    /* All primitives in range [from, subdivisionIndex) will be fed to the lower (left) child node,
       and those from [subdivisionIndex, iterTo) to the higher (right) node. */
    size_t subdivisionIndex = from;

    if (xrange >= yrange && xrange >= zrange)
    {
        // Sort by the position of primitive's center
        std::sort(primitives.begin() + from, iterTo,

                  [](const Primitive *p1, const Primitive *p2)
                  { return (p1->GetXmin() + p1->GetXmax())*0.5 < (p2->GetXmin() + p2->GetXmax())*0.5; }
        );

        // Find the first primitive whose middle point is higher than the total BV's middle point
        while (subdivisionIndex < to &&
            0.5 * (primitives[subdivisionIndex]->GetXmin() + primitives[subdivisionIndex]->GetXmax()) <= xminall + 0.5*xrange)
        {
            subdivisionIndex++;
        }
    }
    else if (yrange >= xrange && yrange >= zrange)
    {
        std::sort(primitives.begin() + from, iterTo,

                  [](const Primitive *p1, const Primitive *p2)
                  { return (p1->GetYmin() + p1->GetYmax())*0.5 < (p2->GetYmin() + p2->GetYmax())*0.5; }
        );

        // Find first primitive which is higher than the middle point
        while (subdivisionIndex < to &&
            0.5 * (primitives[subdivisionIndex]->GetYmin() + primitives[subdivisionIndex]->GetYmax()) <= yminall + 0.5*yrange)
        {
            subdivisionIndex++;
        }
    }
    else if (zrange >= xrange && zrange >= yrange)
    {
        std::sort(primitives.begin() + from, iterTo,

                  [](const Primitive *p1, const Primitive *p2)
                  { return (p1->GetZmin() + p1->GetZmax())*0.5 < (p2->GetZmin() + p2->GetZmax())*0.5; }
        );

        // Find the first primitive which is higher than the middle point
        while (subdivisionIndex < to &&
            0.5 * (primitives[subdivisionIndex]->GetZmin() + primitives[subdivisionIndex]->GetZmax()) <= zminall + 0.5*zrange)
        {
            subdivisionIndex++;
        }
    }

    /* Avoid an infinite recursion in case when there is a dominating bounding box
       which always gets sorted as last and 'subdivisionIndex' points to it in every subsequent call. */
    if (to - from > 2)
    {
        if (subdivisionIndex == from)
            subdivisionIndex++;
        else if (subdivisionIndex == to)
            subdivisionIndex--;
    }

    node->higher.reset(new BoundingBox());
    node->lower.reset(new BoundingBox());

    Subdivide(node->lower.get(), primitives, from, subdivisionIndex, currentLevel + 1, maxNumLevels, minPrimitivesPerNode);
    Subdivide(node->higher.get(), primitives, subdivisionIndex, to, currentLevel + 1, maxNumLevels, minPrimitivesPerNode);
}

static void PushElements(gpuart::Primitive::Data &vec, const std::initializer_list<GLfloat> &list)
{
    for (auto elem: list)
        vec.push_back(elem);
}

/// Compiles BVH tree starting at 'node' and appends results at the back of 'compiledTree'
void gpuart::BoundingVolumesHierarchy::CompileFrom(const BoundingBox &node, Primitive::Data &compiledTree,
                                                   uint32_t parentAddr, bool isLower) const
{
    /*
    Layout of a node in a compiled tree:

                      node_BB (2 x RGBA32F)                                node_info (1 x RGBA32F)
        { xmin, ymin, zmin, PAD }, { xmax, ymax, zmax, PAD },  { flags|num_primitives, lo_addr, hi_addr, parent_addr },

    If (flags | LEAF): followed by compiled primitive data produced by gpuart::Primitive::StoreIntoBVH().

    The ###_addr fields indicate element index (in terms of RGBA quads) of the "lower"/"higher" child and the parent node.

    NOTE: 'flags|num_primitives' and node addresses are uint32_t (shader reinterprets them via floatBitsToUint()).
    */

    uint32_t nodeAddr = (uint32_t)(compiledTree.size() / RGBA_ELEMS);

    PushElements(compiledTree, { node.xmin, node.ymin, node.zmin, RGBA_PAD,
                                 node.xmax, node.ymax, node.zmax, RGBA_PAD });

    uint32_t flags = (isLower ? IS_LOWER : 0);
    if (&node == Root.get())
        flags |= IS_ROOT;

    if (node.primitives.empty())
    {
        assert(compiledTree.size() <= (uint32_t)1<<31);

        compiledTree.push_back(*reinterpret_cast<GLfloat*>(&flags));

        uint32_t lowerAddrLoc = (uint32_t)compiledTree.size();
        compiledTree.push_back(0); // placeholder for 'lowerAddr'

        uint32_t higherAddrLoc = (uint32_t)compiledTree.size();
        compiledTree.push_back(0); // placeholder for 'higherAddr'

        compiledTree.push_back(*reinterpret_cast<GLfloat*>(&parentAddr));

        uint32_t lowerAddr = (uint32_t)(compiledTree.size() / RGBA_ELEMS);
        compiledTree[lowerAddrLoc] = *reinterpret_cast<GLfloat*>(&lowerAddr);

        CompileFrom(*node.lower, compiledTree, nodeAddr, true);

        uint32_t higherAddr = (uint32_t)(compiledTree.size() / RGBA_ELEMS);
        compiledTree[higherAddrLoc] = *reinterpret_cast<GLfloat*>(&higherAddr);

        CompileFrom(*node.higher, compiledTree, nodeAddr, false);
    }
    else
    {
        flags |= LEAF;
        flags |= ((uint32_t)node.primitives.size() & ~FLAGS_MASK);
        compiledTree.push_back(*reinterpret_cast<GLfloat*>(&flags));

        PushElements(compiledTree, { RGBA_PAD, RGBA_PAD }); // no children addresses
        compiledTree.push_back(*reinterpret_cast<GLfloat*>(&parentAddr));

        for (gpuart::Primitive *p: node.primitives)
            p->StoreIntoBVH(compiledTree);
    }
}

/** Prints contents of a compiled BVH tree, interpreting it
    in the same manner as the BVH-traversal shader. */
void gpuart::BoundingVolumesHierarchy::Print(const gpuart::Primitive::Data &compiledTree, std::ostream &s)
{
    auto pos = compiledTree.begin();

    while (pos != compiledTree.end())
    {
        s << "Node at " << (pos - compiledTree.begin()) / RGBA_ELEMS;

        s << ": [" << *pos++ << "; "; // xmin
        s << *pos++ << "; ";          // ymin
        s << *pos++ << "]<->[";       // zmin
        pos++; // skip RGBA padding

        s << *pos++ << "; ";   // xmax
        s << *pos++ << "; ";          // ymax
        s << *pos++ << "], ";         // zmax
        pos++; // skip RGBA padding

        uint32_t flags = *reinterpret_cast<const uint32_t*>(&*pos++);

        if (flags & IS_LOWER)
            s << "IS_LOWER ";

        if (flags & IS_ROOT)
        {
            if (flags & IS_LOWER) s << "| ";
            s << "ROOT ";
        }

        if (flags & LEAF)
        {
            if (flags & (IS_LOWER | IS_ROOT)) s << "| ";
            s << "LEAF, ";

            pos += 2; // skip unused lo/hi children addresses

            uint32_t parentAddr = *reinterpret_cast<const uint32_t*>(&*pos++);
            s << "parent at " << parentAddr << ", ";

            size_t numPrimitives = flags & ~FLAGS_MASK;
            s << numPrimitives << (numPrimitives == 1 ? " primitive: " : " primitives: ");

            for (size_t i = 0; i < numPrimitives; i++)
            {

                Primitive_t ptype = (Primitive_t)*reinterpret_cast<const uint32_t*>(&*pos++);
                pos +=3; // skip RGBA padding

                switch (ptype)
                {
                case SPHERE:
                    s << "sphere ";
                    gpuart::Sphere::PrintBVH(pos, s);
                    break;

                case DISC:
                    s << "disc ";
                    gpuart::Disc::PrintBVH(pos, s);
                    break;

                case TRIANGLE:
                    s << "triangle ";
                    gpuart::Triangle::PrintBVH(pos, s);
                    break;

                case CONE:
                    s << "cone ";
                    gpuart::Cone::PrintBVH(pos, s);
                }

                s << ", ";
            }

            s << std::endl;
        }
        else
        {
            uint32_t lowerAddr = *reinterpret_cast<const uint32_t*>(&*pos++);
            s << "lo_child at " << lowerAddr << ", ";

            uint32_t higherAddr = *reinterpret_cast<const uint32_t*>(&*pos++);
            s << "hi_child at " << higherAddr << ", ";

            uint32_t parentAddr = *reinterpret_cast<const uint32_t*>(&*pos++);
            s << "parent at " << parentAddr << std::endl;
        }

    }
}
