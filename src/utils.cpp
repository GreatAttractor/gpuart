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
    Utilities implementation
*/


#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

#include "utils.h"


std::ostream& gpuart::Utils::operator <<(std::ostream &os, const gpuart::Utils::TimeElapsed &te)
{
    os << std::fixed << std::setprecision(1);
    os << std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - te.tstart).count() << " ms";

    return os;
}

/// Loads a mesh of triangles in PLY format and appends it to 'primitives'
bool gpuart::Utils::LoadMeshFromPLY(std::vector<gpuart::Primitive*> &primitives, const char *fileName,
                                    float magnification, const Vec3f &translation)
{
    std::ifstream fs(fileName);

    if (fs.fail())
        return false;

    std::stringstream ss;
    std::string line, token;

    std::getline(fs, line);
    if (line != "ply")
        return false;

    size_t numVertices = 0;
    size_t numFaces = 0;

    auto tstart = std::chrono::high_resolution_clock::now();
    std::cout << "Loading mesh from \"" << fileName << "\"... "; std::cout.flush();

    while (!fs.eof() && line != "end_header")
    {
        std::getline(fs, line);
        if (0 == line.find("element vertex"))
        {
            ss.clear();
            ss.str(line);
            ss.ignore(std::numeric_limits<std::streamsize>::max(), ' '); // "element"
            ss.ignore(std::numeric_limits<std::streamsize>::max(), ' '); // "vertex"
            ss >> numVertices;
            if (ss.fail())
                return false;
        }
        else if (0 == line.find("element face"))
        {
            ss.clear();
            ss.str(line);
            ss.ignore(std::numeric_limits<std::streamsize>::max(), ' '); // "element"
            ss.ignore(std::numeric_limits<std::streamsize>::max(), ' '); // "face"
            ss >> numFaces;
            if (ss.fail())
                return false;
        }
    }

    std::vector<gpuart::Vec3f> vertices;

    for (size_t i = 0; i < numVertices; i++)
    {
        std::getline(fs, line);
        if (line.empty())
            continue;

        ss.clear();
        ss.str(line);

        float x, y, z;
        ss >> x;
        ss >> y;
        ss >> z;
        if (ss.fail())
            return false;

        vertices.push_back(translation + magnification * gpuart::Vec3f(x, y, z));
    }

    for (size_t i = 0; i < numFaces && !fs.eof(); i++)
    {
        std::getline(fs, line);
        if (line.empty())
            continue;

        ss.clear();
        ss.str(line);

        ss << line;

        int verts, v0, v1, v2;
        ss >> verts;
        ss >> v0;
        ss >> v1;
        ss >> v2;
        if (verts != 3 || ss.fail())
            return false;

        primitives.push_back(new gpuart::Triangle(vertices[v0], vertices[v1], vertices[v2]));
    }

    std::cout << " done (" << TimeElapsed(tstart) << "), "
              << "faces: " << numFaces << ", vertices: " << numVertices << "." << std::endl;


    return true;
}

/// Loads primitives from a text file and appends them to 'primitives'
bool gpuart::Utils::LoadPrimitives(std::vector<gpuart::Primitive*> &primitives, const char *fileName,
                                   float magnification, const gpuart::Vec3f &translation)
{
    std::cout << "Loading primitives from \"" << fileName << "\"..."; std::cout.flush();
    auto tstart = std::chrono::high_resolution_clock::now();

    std::ifstream fs(fileName);

    if (fs.fail())
        return false;

    std::stringstream ss;
    std::string line, token;

    while (!fs.eof())
    {
        std::getline(fs, line);
        if (line.empty() || line[0] == '#')
            continue;

        ss.clear();
        ss.str(line);

        ss >> token;
        if (token == "sphere")
        {
            float x, y, z, r;
            ss >> x >> y >> z;
            if (ss.fail())
                return false;

            ss >> r;
            if (ss.fail())
                r = 4.0f;

            primitives.push_back(new gpuart::Sphere(translation + magnification * Vec3f(x, y, z), magnification * r));
        }
        else if (token == "cone")
        {
            Vec3f center1, center2;
            float radius1, radius2;

            ss >> center1.x >> center1.y >> center1.z;
            ss >> center2.x >> center2.y >> center2.z;
            ss >> radius1 >> radius2;

            if (ss.fail())
                return false;

            primitives.push_back(new gpuart::Cone(translation + magnification * center1,
                                                  translation + magnification * center2,
                                                  magnification * radius1,
                                                  magnification * radius2));
        }
    }

    std::cout << " done (" << TimeElapsed(tstart) << ")." << std::endl;

    return true;
}

/// Returns a null-terminated string created with snprintf()
std::unique_ptr<char[]> gpuart::Utils::FormatStr(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    size_t outputLen = vsnprintf(nullptr, 0, format, args);
    va_end(args);

    std::unique_ptr<char[]> result(new char[outputLen+1]);

    va_start(args, format);
    vsnprintf(result.get(), outputLen+1, format, args);
    va_end(args);

    return result;
}
