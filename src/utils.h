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
    Utilities header
*/

#ifndef GPUART_UTILS_HEADER
#define GPUART_UTILS_HEADER

#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

#include "core.h"


namespace gpuart
{
namespace Utils
{
    struct TimeElapsed
    {
        std::chrono::high_resolution_clock::time_point tstart;
        explicit TimeElapsed(decltype(tstart) tstart): tstart(tstart) { }
    };

    std::ostream& operator <<(std::ostream &os, const TimeElapsed &te);

    /// Loads a mesh of triangles in PLY format and appends it to 'primitives'
    bool LoadMeshFromPLY(std::vector<gpuart::Primitive*> &primitives, const char *fileName,
                         float magnification = 1.0f, const Vec3f &translation = Vec3f(0, 0, 0));

    /// Loads primitives from a text file and appends them to 'primitives'
    bool LoadPrimitives(std::vector<gpuart::Primitive*> &primitives, const char *fileName,
                        float magnification = 1.0f, const Vec3f &translation = Vec3f(0, 0, 0));

    /// Returns a null-terminated string created with snprintf()
    std::unique_ptr<char[]> FormatStr(const char *format, ...);
}
}


#endif
