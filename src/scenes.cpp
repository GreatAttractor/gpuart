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
    Scene initialization implementation
*/

#include "math_types.h"
#include "scenes.h"
#include "utils.h"

using gpuart::Vec3f;


bool InitDragon(gpuart::Renderer &renderer, const char *meshFName)
{
    std::vector<gpuart::Primitive*> primitives;

    if (!gpuart::Utils::LoadMeshFromPLY(primitives, meshFName, 10, Vec3f(0, 0, -0.5)))
    {
        std::cerr << "Failed to load mesh from \"" << meshFName << "\"." << std::endl;
        return false;
    }
    primitives.push_back(new gpuart::Disc(Vec3f(0, 0, 0), Vec3f(0, 0, 1), 5));

    renderer.SetPrimitives(primitives, true);
    for (auto *p: primitives)
        delete p;

    std::cout << std::endl;
    return true;
}


void InitBox(gpuart::Renderer &renderer)
{
    std::vector<gpuart::Primitive*> primitives;

    primitives.push_back(new gpuart::Sphere(Vec3f(0, 0, 0.3f), 0.3f));
    primitives.push_back(new gpuart::Disc(Vec3f(0, 0, 0), Vec3f(0, 0, 1), 6));
    primitives.push_back(new gpuart::Triangle(1, -1, 0,  1, 1, 0,  1, 1, 1));
    primitives.push_back(new gpuart::Triangle(1, -1, 0,  1, 1, 1,  1, -1, 1));
    primitives.push_back(new gpuart::Cone(Vec3f(0.5f, -0.7, 0), Vec3f(0.5f, -0.7f, 0.35f), 0.2f, 0.2f));
    primitives.push_back(new gpuart::Triangle(1, 1, 0, 1, 1, 1,  -1, 1, 1));
    primitives.push_back(new gpuart::Triangle(-1, 1, 1,  -1, 1, 0,  1, 1, 0));
    primitives.push_back(new gpuart::Triangle(-1, -1, 0, -1, 1, 0,  -1, 1, 1));
    primitives.push_back(new gpuart::Triangle(-1, -1, 0,  -1, 1, 1,  -1, -1, 1));

    renderer.SetPrimitives(primitives, true);
    std::cout << std::endl;
    for (auto *p: primitives)
        delete p;
}

bool InitCluster(gpuart::Renderer &renderer)
{
    std::vector<gpuart::Primitive*> primitives;

    if (!gpuart::Utils::LoadPrimitives(primitives, "data/cluster_100k.dat", 0.01f, Vec3f(0, 0, 2.5f)))
        return false;

    primitives.push_back(new gpuart::Disc(Vec3f(1, 0, 0), Vec3f(0, 0, 1), 6));

    renderer.SetPrimitives(primitives, true);
    std::cout << std::endl;
    for (auto *p: primitives)
        delete p;

    return true;
}

bool InitTree(gpuart::Renderer &renderer)
{
    std::vector<gpuart::Primitive*> primitives;

    if (!gpuart::Utils::LoadPrimitives(primitives, "data/tree1_21k.dat", 0.3f))
        return false;

    primitives.push_back(new gpuart::Disc(Vec3f(1, 0, 0), Vec3f(0, 0, 1), 6));

    renderer.SetPrimitives(primitives, true);
    std::cout << std::endl;
    for (auto *p: primitives)
        delete p;

    return true;
}
