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
   Camera rays initialization
*/

#version 330 core


// Inputs -------------------------------------------------

in vec2 UV; ///< Texture coordinates (ray index)

uniform vec3 Pos;        ///< Camera position
uniform vec3 BottomLeft; ///< Screen bottom-left corner
uniform vec3 DeltaHorz;  ///< Screen bottom-right to bottom-left vector
uniform vec3 DeltaVert;  ///< Screen top-left to bottom-left vector


// Outputs ------------------------------------------------

layout(location = 0) out vec3 out_RStart; ///< Camera ray starting point
layout(location = 1) out vec3 out_RDir;   ///< Camera ray direction (unit)


// ---------------------------------------------------------

void main()
{
    vec3 start = BottomLeft + DeltaHorz*UV.x + DeltaVert*UV.y;
    out_RStart = start;
    out_RDir   = normalize(start - Pos);
}
