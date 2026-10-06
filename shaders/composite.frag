#version 440
// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D sceneTex;
layout(binding = 2) uniform sampler2D bloomTex;

layout(std140, binding = 0) uniform buf {
    vec4 bgTop;    // rgb: gradient start, or theme colour in XMB theme mode
    vec4 bgBottom; // rgb: gradient end
    vec4 bgParams; // x = mode (0 none, 1 gradient, 2 XMB theme), y = angle (rad), z = brightness, w = NDC y up
    vec4 ps2Color; // rgb = PS2 wave colour, a = 1 when the PS2 (PSX DESR) wave is drawn
    vec4 fx;       // x = bloom strength, y = vignette, z = dither, w = time (s)
    vec4 view;     // xy = output size (px), z = wave centre (0 = top, 1 = bottom), w = wave opacity
    vec4 scene;    // xy = 1 / output size, z = scene present, w = supersampling factor

    // Svec Studio wave (the "hero waves" from the Sencurio landing page as used by
    // svec-studio ui/desktop/wallpaper.js): 0 = wave rgb + enabled,
    // 1 = x custom colours flag, y band height scale (1 = landing-page default)
    vec4 svec[2];
} u;

float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 background(vec2 p, float aspect)
{
    int mode = int(u.bgParams.x + 0.5);
    if (mode == 1) {
        vec2 d = vec2(cos(u.bgParams.y), sin(u.bgParams.y));
        float extent = 0.5 * (abs(d.x) + abs(d.y));
        float t = clamp(dot(p - 0.5, d) / max(extent, 1e-4) * 0.5 + 0.5, 0.0, 1.0);
        return mix(u.bgTop.rgb, u.bgBottom.rgb, t) * u.bgParams.z;
    }
    // XMB theme: the month colour, brighter towards the top with a soft broad highlight,
    // dimmed by the time-of-day brightness like the PSP/PS3 system menu.
    vec3 c = u.bgTop.rgb;
    vec3 col = mix(c * 1.08 + 0.025, c * 0.30, smoothstep(0.0, 1.0, p.y));
    vec2 q = vec2((p.x - 0.62) * aspect, p.y + 0.10);
    col += c * 0.22 * exp(-dot(q, q) * 2.2);
    return col * u.bgParams.z;
}

// PS2 / PSX (DESR) XMB wave: two phase-shifted sine bands at the bottom of the screen, the
// front one tinted with a lighter theme colour and edged by a thin bright line. Evaluated
// analytically per pixel, so it stays razor sharp at any resolution.
void ps2Wave(vec2 p, float aspect, inout vec3 col, inout float a)
{
    float unit = 1.0 / 448.0; // PS2 screen lines
    float x = p.x * aspect * 448.0 * 0.005;
    float t = u.fx.w;
    float base = u.view.z;
    float y1 = base + sin(x + t * 1.14) * 30.0 * unit;
    float y2 = base + sin(x + t * 1.26) * 32.0 * unit;
    float aa = max(fwidth(p.y), 1e-5);
    float op = clamp(u.view.w, 0.0, 1.0);

    float m1 = smoothstep(y1 - aa, y1 + aa, p.y) * (36.0 / 128.0) * op;
    col *= 1.0 - m1;
    a = a + (1.0 - a) * m1;

    vec3 wc = u.ps2Color.rgb;
    float m2 = smoothstep(y2 - aa, y2 + aa, p.y) * (96.0 / 128.0) * op;
    col = col * (1.0 - m2) + wc * m2;
    a = a * (1.0 - m2) + m2;

    float dy = abs(p.y - y2);
    float line = (1.0 - smoothstep(unit - aa, unit + aa, dy)) * op;
    col = col * (1.0 - line) + wc * line;
    a = a * (1.0 - line) + line;

    float glow = exp(-pow(dy / (7.0 * unit), 2.0)) * 0.35 * u.fx.x * op;
    col += wc * glow;
    a = a + (1.0 - a) * clamp(glow, 0.0, 1.0);
}

// ───────────────────────────── Svec Studio wave ─────────────────────────────
// The "hero waves" from the Sencurio landing page, as used for the svec-studio
// desktop wallpaper (ui/desktop/wallpaper.js + app.css): four filled wave
// silhouettes anchored to the bottom edge. Each layer tiles one 1200x200 SVG
// path per screen width, scrolls sideways on its own period and bobs gently.

// y(t) of a cubic Bézier where x(t) = x; xs strictly increasing per segment.
float heroBez(float x, vec4 xs, vec4 ys)
{
    float t = clamp((x - xs.x) / max(xs.w - xs.x, 1e-4), 0.0, 1.0);
    for (int i = 0; i < 5; ++i) {
        float mt = 1.0 - t;
        float xt = mt * mt * mt * xs.x + 3.0 * mt * mt * t * xs.y + 3.0 * mt * t * t * xs.z + t * t * t * xs.w;
        float dx = 3.0 * mt * mt * (xs.y - xs.x) + 6.0 * mt * t * (xs.z - xs.y) + 3.0 * t * t * (xs.w - xs.z);
        t = clamp(t - (xt - x) / max(dx, 1e-3), 0.0, 1.0);
    }
    float mt = 1.0 - t;
    return mt * mt * mt * ys.x + 3.0 * mt * mt * t * ys.y + 3.0 * mt * t * t * ys.z + t * t * t * ys.w;
}

// Top edge of each layer in the 1200x200 SVG space (paths from hero-waves.php).
float heroTop(float x, int layer)
{
    bool r = x >= 600.0;
    if (layer == 0)
        return r ? heroBez(x, vec4(600.0, 880.0, 920.0, 1200.0), vec4(58.0, 102.0, 102.0, 102.0))
                 : heroBez(x, vec4(0.0, 280.0, 320.0, 600.0), vec4(102.0, 102.0, 58.0, 58.0));
    if (layer == 1)
        return r ? heroBez(x, vec4(600.0, 890.0, 900.0, 1200.0), vec4(56.0, 58.0, 96.0, 96.0))
                 : heroBez(x, vec4(0.0, 280.0, 310.0, 600.0), vec4(96.0, 96.0, 54.0, 56.0));
    if (layer == 2)
        return r ? heroBez(x, vec4(600.0, 820.0, 1000.0, 1200.0), vec4(134.0, 136.0, 112.0, 112.0))
                 : heroBez(x, vec4(0.0, 280.0, 380.0, 600.0), vec4(112.0, 112.0, 132.0, 134.0));
    return r ? heroBez(x, vec4(600.0, 800.0, 880.0, 1200.0), vec4(130.0, 134.0, 88.0, 88.0))
             : heroBez(x, vec4(0.0, 240.0, 400.0, 600.0), vec4(88.0, 88.0, 126.0, 130.0));
}

// The CSS bob keyframes, piecewise-smooth between keys (p = phase in percent).
float bobCurve(float p, float ts[7], float vs[7])
{
    for (int i = 1; i < 7; ++i) {
        if (p <= ts[i]) {
            float e = (p - ts[i - 1]) / (ts[i] - ts[i - 1]);
            e = e * e * (3.0 - 2.0 * e);
            return mix(vs[i - 1], vs[i], e);
        }
    }
    return vs[6];
}

// Vertical "top" offset in px like the CSS bob animations (negative = up).
float heroBob(int layer, float t)
{
    if (layer == 0)
        return bobCurve(fract(t / 32.0) * 100.0, float[7](0.0, 16.0, 32.0, 48.0, 64.0, 84.0, 100.0), float[7](0.0, -4.0, -10.0, -2.0, -11.0, -5.0, 0.0));
    if (layer == 1)
        return bobCurve(fract(t / 20.8) * 100.0, float[7](0.0, 11.0, 28.0, 45.0, 58.0, 76.0, 100.0), float[7](0.0, 4.0, 0.0, 6.0, 1.0, 6.0, 0.0));
    if (layer == 2)
        return bobCurve(fract(t / 26.4) * 100.0, float[7](0.0, 22.0, 40.0, 55.0, 72.0, 90.0, 100.0), float[7](0.0, -7.0, 1.0, -9.0, 2.0, -5.0, 0.0));
    return bobCurve(fract(t / 17.4) * 100.0, float[7](0.0, 19.0, 35.0, 50.0, 66.0, 81.0, 100.0), float[7](0.0, 3.0, -2.0, 4.0, -1.0, 3.0, 0.0));
}

void svecOver(vec3 c, float m, inout vec3 col, inout float a)
{
    col = mix(col, c, m);
    a += (1.0 - a) * m;
}

void svecWave(vec2 p, inout vec3 col, inout float a)
{
    float W = max(u.view.x, 1.0);
    float H = max(u.view.y, 1.0);
    float t = u.fx.w;
    float op = clamp(u.view.w, 0.0, 1.0);
    vec3 wave = u.svec[0].rgb;
    vec3 bgc = u.bgBottom.rgb; // colour behind the waves, at the bottom edge
    bool custom = u.svec[1].x > 0.5;

    // Aurora glows drifting over the background (svec-studio desk-wave-aurora).
    {
        vec2 c1 = vec2(0.18 + 0.04 * sin(t * 0.24), 0.08 + 0.03 * cos(t * 0.19));
        vec2 c2 = vec2(0.78 + 0.05 * sin(t * 0.13 + 1.7), 0.82 + 0.04 * cos(t * 0.17 + 0.6));
        float g1 = clamp(1.0 - length(vec2((p.x - c1.x) / 0.9, (p.y - c1.y) / 0.7)) / 0.52, 0.0, 1.0);
        float g2 = clamp(1.0 - length(vec2((p.x - c2.x) / 0.5, (p.y - c2.y) / 0.44)) / 0.56, 0.0, 1.0);
        svecOver(mix(wave, bgc, 0.58), g1 * 0.9 * op, col, a);
        svecOver(mix(wave, bgc, 0.72), g2 * 0.8 * op, col, a);
    }

    // Wave band: clamp(140px, 22vmin+24px, 280px) anchored to the bottom, scaled by
    // the user height factor (the CSS clamp bounds the default, not the user pick).
    float bandPx = clamp(0.22 * min(W, H) + 24.0, 140.0, 280.0) * clamp(u.svec[1].y, 0.05, 4.0);
    float v = (1.0 - p.y) * H / bandPx; // 0 at the bottom edge, 1 at the band top
    if (v >= 1.0)
        return;
    // CSS mask: opaque up to 48 % of the band, fading out at the top.
    float mask = clamp((1.0 - v) / 0.52, 0.0, 1.0);

    const float scrollT[4] = float[4](70.0, 58.0, 48.0, 38.0);
    const float mixB[4] = float[4](0.42, 0.28, 0.14, 0.0);    // custom colours
    const float mixA[4] = float[4](0.68, 0.58, 0.50, 0.42);   // theme colours
    const float alphaB[4] = float[4](0.48, 0.56, 0.64, 0.78);
    const float alphaA[4] = float[4](0.32, 0.36, 0.40, 0.44);

    float aa = max(fwidth(v) * 200.0, 0.5);
    for (int i = 0; i < 4; ++i) {
        // Layer 2 and 4 scroll the other way like on the landing page.
        float dir = (i == 1 || i == 3) ? -1.0 : 1.0;
        float sx = fract(p.x + dir * t / scrollT[i]) * 1200.0;
        float sy = (1.0 - v) * 200.0 - heroBob(i, t) * (200.0 / bandPx);
        float fill = smoothstep(-aa, aa, sy - heroTop(sx, i));
        svecOver(mix(wave, bgc, custom ? mixB[i] : mixA[i]), fill * (custom ? alphaB[i] : alphaA[i]) * mask * op, col, a);
    }
}

void main()
{
    float aspect = u.view.x / max(u.view.y, 1.0);
    // Screen position, y growing downwards on every backend.
    vec2 p = vec2(vUv.x, u.bgParams.w > 0.5 ? 1.0 - vUv.y : vUv.y);

    vec3 col = vec3(0.0);
    float a = 0.0;
    if (u.bgParams.x > 0.5) {
        col = background(p, aspect);
        a = 1.0;
    }
    if (u.ps2Color.a > 0.5)
        ps2Wave(p, aspect, col, a);
    if (u.svec[0].a > 0.5)
        svecWave(p, col, a);

    if (u.scene.z > 0.5) {
        vec3 s;
        if (u.scene.w > 1.01) {
            // Resolve the supersampled wave with a 2x2 tent around the output pixel.
            vec2 o = 0.25 * u.scene.xy;
            s = 0.25 * (texture(sceneTex, vUv + vec2(-o.x, -o.y)).rgb + texture(sceneTex, vUv + vec2(o.x, -o.y)).rgb
                        + texture(sceneTex, vUv + vec2(-o.x, o.y)).rgb + texture(sceneTex, vUv + vec2(o.x, o.y)).rgb);
        } else {
            s = texture(sceneTex, vUv).rgb;
        }
        col += s;
        if (u.fx.x > 0.001)
            col += texture(bloomTex, vUv).rgb * u.fx.x;
    }

    if (u.fx.y > 0.001) {
        vec2 v = (p - 0.5) * vec2(aspect, 1.0);
        float r = length(v) / (0.5 * sqrt(aspect * aspect + 1.0));
        col *= 1.0 - u.fx.y * smoothstep(0.35, 1.05, r);
    }

    // Triangular dither: hides 8-bit banding of dark gradients and the soft wave glow.
    if (u.fx.z > 0.001 && (a > 0.0 || max(col.r, max(col.g, col.b)) > 0.002)) {
        vec2 px = gl_FragCoord.xy;
        float n = hash12(px) + hash12(px + vec2(17.31, 91.7)) - 1.0;
        col += vec3(n / 255.0) * u.fx.z;
    }

    col = max(col, vec3(0.0));
    fragColor = vec4(min(col, vec3(max(a, 1.0))), a);
}
