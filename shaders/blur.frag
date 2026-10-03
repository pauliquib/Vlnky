#version 440
// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D src;

layout(std140, binding = 0) uniform buf {
    vec4 texel;  // xy = 1 / source size, zw = blur direction (in texels)
    vec4 params; // unused
} u;

// 9-tap separable Gaussian using linear-filtering offsets (5 fetches).
void main()
{
    vec2 d = u.texel.zw * u.texel.xy;
    vec3 c = texture(src, vUv).rgb * 0.2270270270;
    c += texture(src, vUv + d * 1.3846153846).rgb * 0.3162162162;
    c += texture(src, vUv - d * 1.3846153846).rgb * 0.3162162162;
    c += texture(src, vUv + d * 3.2307692308).rgb * 0.0702702703;
    c += texture(src, vUv - d * 3.2307692308).rgb * 0.0702702703;
    fragColor = vec4(c, 1.0);
}
