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
   Sky color calculations
*/

#version 330 core


const vec3 SKY_ZENITH_COLOR_SUN_HI = vec3(0.2, 0.6, 1);
const vec3 SKY_ZENITH_COLOR_SUN_LO = vec3(0, 0.2, 0.5);

const vec3 SKY_HORIZON_COLOR_SUN_HI = vec3(1, 1, 1);
const vec3 SKY_HORIZON_COLOR_SUN_LO = vec3(1, 0.647, 0.367);


vec3 GetSkyColor(
    /// Direction towards the sky to return the sky color for
    in vec3 dir,

    /// Direction towards the Sun (unit) and its altitude
    in vec4 sunDirAlt)
{
    vec3 dirHorzProj = cross(cross(vec3(0, 0, 1), normalize(dir)), vec3(0, 0, 1));
    float weight;

    if (dir.z >=0 )
        weight = dot(normalize(dir), normalize(dirHorzProj));
    else
        weight = 1;

    float sunWeight = 1.0 - sunDirAlt.w / (3.1415926/2);

    vec3 colZenith = mix(SKY_ZENITH_COLOR_SUN_HI,
                         SKY_ZENITH_COLOR_SUN_LO,
                         sunWeight);

    vec3 colHorizon = mix(SKY_HORIZON_COLOR_SUN_HI,
                          SKY_HORIZON_COLOR_SUN_LO,
                          sunWeight);

    return mix(colZenith, colHorizon, pow(weight, 16));
}
