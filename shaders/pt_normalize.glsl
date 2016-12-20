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
   Rendering stage: normalization of path tracing results
*/

#version 330 core


// Inputs -------------------------------------------------

in vec2 UV; ///< Texture coordinates (ray index)

/// Pixel radiance accumulated in previous path tracing passes
uniform sampler2D Radiance;

uniform int NumPathsPerPixel;


// Outputs ------------------------------------------------

layout(location = 0) out vec3 out_Color;


// ---------------------------------------------------------

void main()
{
    out_Color = texture(Radiance, UV).rgb / NumPathsPerPixel;
}
