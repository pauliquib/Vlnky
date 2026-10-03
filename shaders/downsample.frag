#version 440
// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D src;

layout(std140, binding = 0) uniform buf {
    vec4 texel;  // xy = 1 / source size, zw = footprint of one destination pixel in source texels
    vec4 params; // unused
} u;

// Box-filtered reduction for the bloom chain: 4x4 bilinear taps spread over the destination
// pixel footprint, so thin bright wave strands do not flicker when they move.
void main()
{
    vec3 acc = vec3(0.0);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            vec2 o = (vec2(float(x), float(y)) + 0.5) / 4.0 - 0.5;
            acc += texture(src, vUv + o * u.texel.zw * u.texel.xy).rgb;
        }
    }
    fragColor = vec4(acc / 16.0, 1.0);
}
