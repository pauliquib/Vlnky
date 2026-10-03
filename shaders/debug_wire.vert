#version 440
// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

layout(location = 0) in vec3 inPos;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    float phase;
    float tessBlend;
    int cols;
    int rows;
    int tessLevel;
    float opacity;
    int debugMode;
    float pointSize;
    vec4 sceneTint;
} u;

void main()
{
    gl_Position = u.mvp * vec4(inPos, 1.0);
}
