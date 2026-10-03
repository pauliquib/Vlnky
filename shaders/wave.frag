#version 440
// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

layout(location = 0) in vec3 vNormal;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D reflectionMap;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 color;
    vec4 params; // x = debug mode, y = bicubic sphere-map filtering, zw = texel size
} u;

// Cubic B-spline weights.
vec4 cubicWeights(float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    return vec4(1.0 - 3.0 * t + 3.0 * t2 - t3,
                4.0 - 6.0 * t2 + 3.0 * t3,
                1.0 + 3.0 * t + 3.0 * t2 - 3.0 * t3,
                t3) / 6.0;
}

// Smooth B-spline magnification from 4 bilinear taps. The PSP reflection map is only 128x128,
// stretched over the whole screen width: plain bilinear shows diamond artefacts and hard
// gradient steps at 1080p and above.
vec3 sampleBicubic(vec2 uv)
{
    vec2 texel = u.params.zw;
    vec2 st = uv / texel - 0.5;
    vec2 i = floor(st);
    vec2 f = st - i;
    vec4 wx = cubicWeights(f.x);
    vec4 wy = cubicWeights(f.y);
    vec2 s0 = vec2(wx.x + wx.y, wy.x + wy.y);
    vec2 s1 = vec2(wx.z + wx.w, wy.z + wy.w);
    vec2 o0 = (i + vec2(-0.5) + vec2(wx.y, wy.y) / s0) * texel;
    vec2 o1 = (i + vec2(1.5) + vec2(wx.w, wy.w) / s1) * texel;
    vec3 a = texture(reflectionMap, vec2(o0.x, o0.y)).rgb;
    vec3 b = texture(reflectionMap, vec2(o1.x, o0.y)).rgb;
    vec3 c = texture(reflectionMap, vec2(o0.x, o1.y)).rgb;
    vec3 d = texture(reflectionMap, vec2(o1.x, o1.y)).rgb;
    float sx = s0.x / (s0.x + s1.x);
    float sy = s0.y / (s0.y + s1.y);
    return mix(mix(d, c, sx), mix(b, a, sx), sy);
}

void main()
{
    int mode = int(u.params.x + 0.5);
    if (mode == 1) {
        fragColor = vec4(0.35, 0.8, 1.0, 0.0);
        return;
    }
    // Environment (sphere) mapping, as the PSP GE does for the XMB wave's reflection map:
    // the view-space normal selects the texel, so only surface facing the viewer lights up.
    vec3 n = normalize(vNormal);
    vec2 uv = vec2(n.x, -n.y) * 0.5 + 0.5;
    vec3 tex = u.params.y > 0.5 ? sampleBicubic(uv) : texture(reflectionMap, uv).rgb;
    fragColor = vec4(tex * u.color.rgb, 0.0);
}
