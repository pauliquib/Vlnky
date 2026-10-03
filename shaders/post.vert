#version 440
// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

// Full-screen quad shared by all post-processing passes. uv follows the texture convention of
// the backend automatically: textures we render with this quad are sampled back with it too.
layout(location = 0) in vec2 inPos;

layout(location = 0) out vec2 vUv;

void main()
{
    vUv = inPos * 0.5 + 0.5;
    gl_Position = vec4(inPos, 0.0, 1.0);
}
