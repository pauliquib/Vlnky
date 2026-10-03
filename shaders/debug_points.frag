#version 440
// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

layout(location = 0) out vec4 fragColor;

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
    vec2 c = gl_PointCoord - vec2(0.5);
    if (dot(c, c) > 0.25)
        discard;
    fragColor = vec4(1.0, 0.85, 0.2, 1.0);
}
