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
    Bounding Volumes Hierarchy header
*/

#ifndef GPUART_BOUNDING_VOLUMES_HIERARCHY_HEADER
#define GPUART_BOUNDING_VOLUMES_HIERARCHY_HEADER

#include <cstdint>
#include <nanogui/nanogui.h>
#include <iostream>
#include <memory>
#include <vector>

#include "core.h"


namespace gpuart
{

    struct BoundingBox
    {
        float xmin, xmax, ymin, ymax, zmin, zmax;
        std::unique_ptr<BoundingBox> lower, higher;
        std::vector<Primitive*> primitives;
    };

    class BoundingVolumesHierarchy
    {
        static const uint32_t LEAF     = 1UL << 31;
        static const uint32_t IS_LOWER = 1UL << 30;
        static const uint32_t IS_ROOT  = 1UL << 29;

        static const uint32_t FLAGS_MASK = LEAF | IS_LOWER | IS_ROOT;

        std::unique_ptr<BoundingBox> Root;

        /**  Divides the 'primitives' with indices between 'from' (incl.) and 'two' (excl.)
             along the longest spanned axis. */
        void Subdivide(BoundingBox *node, std::vector<Primitive*> &primitives, size_t from, size_t to,
                       unsigned currentLevel,
                       unsigned maxNumLevels,
                       unsigned minPrimitivesPerNode);

        /// Compiles BVH tree starting at 'node' and appends results at the back of 'compiledTree'
        void CompileFrom(const BoundingBox &node, Primitive::Data &compiledTree,
                         uint32_t parentAddr, bool isLower) const;

    public:

        BoundingVolumesHierarchy() = default;

        BoundingVolumesHierarchy(const BoundingVolumesHierarchy &)             = delete;
        BoundingVolumesHierarchy & operator=(const BoundingVolumesHierarchy &) = delete;
        BoundingVolumesHierarchy(BoundingVolumesHierarchy &&)                  = default;
        BoundingVolumesHierarchy & operator=(BoundingVolumesHierarchy &&)      = default;


        /// Order of elements in 'primitives' may change
        BoundingVolumesHierarchy(std::vector<Primitive*> &primitives, unsigned maxNumLevels, unsigned minPrimitivesPerNode)
        {
            Root.reset(new BoundingBox());
            Subdivide(Root.get(), primitives, 0, primitives.size(), 0, maxNumLevels, minPrimitivesPerNode);
        }

        /// Compiles the BVH tree and appends results at the back of 'compiledTree'
        void Compile(Primitive::Data &compiledTree) const
        {
            CompileFrom(*Root.get(), compiledTree, 0, false);
        }

        /** Prints contents of a compiled BVH tree, interpreting it
            in the same manner as the BVH-traversal shader. */
        static void Print(const Primitive::Data &compiledTree, std::ostream &s);
    };

}

#endif
