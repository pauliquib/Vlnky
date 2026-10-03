#version 440
// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D waveTex;

void main()
{
    float lum = texture(waveTex, inUV).r;
    fragColor = vec4(vec3(lum), 1.0);
}
