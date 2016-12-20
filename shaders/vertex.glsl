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
    Vertex shader
*/
    
#version 330 core

in vec4 Position;
out vec2 UV;

void main()
{
    /* We use full-screen quads spanning (-1, -1) to (1, 1);
       convert the vertex coordinates to texture (u, v) coordinates
       spanning (0, 0) to (1, 1). */
    UV = (Position.xy + vec2(1, 1))/2;

    gl_Position = Position;
}
