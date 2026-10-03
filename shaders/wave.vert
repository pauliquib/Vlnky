#version 440
// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 vNormal;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 color;
    vec4 params; // x = debug mode
} u;

void main()
{
    // The camera looks straight down -Z without rotation, so world normals are view normals.
    vNormal = inNormal;
    gl_Position = u.mvp * vec4(inPos, 1.0);
}
