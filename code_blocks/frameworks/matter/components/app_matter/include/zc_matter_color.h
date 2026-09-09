/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Colour-model conversions for Matter colour lights.
 *
 * A device_type block drives one colour model — hue/saturation, the contract
 * shared with the RainMaker and Zigbee bindings — but Matter makes XY mandatory
 * and colour temperature mandatory on an Extended Color Light. These map XY
 * and colour temperature onto hue/sat, and hue/sat back to XY. Every value
 * here is in Matter units: hue and saturation 0..254, CurrentX/CurrentY
 * 0..0xFEFF, colour temperature in mireds (1e6 / kelvin).
 *
 * xy_to_rgb follows esp-matter's device_hal/led_driver/utils/color_format.c,
 * except that out-of-range channels are scaled instead of clamped so the
 * result can be converted back to hue/sat without loss.
 */
#pragma once

#include <math.h>
#include <stdint.h>

namespace zc {

struct Hs { uint8_t hue; uint8_t sat; };

namespace color_detail {

constexpr uint16_t kXyMax = 0xFEFF;

/* sRGB companding (gamma), both directions, on 0..1 floats. */
inline float srgb_encode(float c)
{
    return c <= 0.0031308f ? 12.92f * c : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}
inline float srgb_decode(float c)
{
    return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}
inline float clamp01(float c) { return c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c); }

/* Matter xy -> sRGB 0..1 at full brightness (esp-matter xy_to_rgb, D65). */
inline void xy_to_rgb(uint16_t x16, uint16_t y16, float *r, float *g, float *b)
{
    float x = x16 / 65536.0f, y = y16 / 65536.0f, z = 1.0f - x - y;
    float Y = 1.0f, X = 0.0f, Z = 0.0f;
    if (y > 0.0f) { X = (Y / y) * x; Z = (Y / y) * z; }
    float lr = fmaxf(0.0f,  X * 3.240479f - Y * 1.537150f - Z * 0.498535f);
    float lg = fmaxf(0.0f, -X * 0.969256f + Y * 1.875992f + Z * 0.041556f);
    float lb = fmaxf(0.0f,  X * 0.055648f - Y * 0.204043f + Z * 1.057311f);
    /* Y=1 puts the dominant channel above 1. Scale the triple down rather than
       clamp it: clamping flattens the channel that carries the hue and pulls
       saturation towards white, so hue/sat could not be recovered. */
    float mx = fmaxf(lr, fmaxf(lg, lb));
    if (mx > 1.0f) { lr /= mx; lg /= mx; lb /= mx; }
    *r = clamp01(srgb_encode(lr));
    *g = clamp01(srgb_encode(lg));
    *b = clamp01(srgb_encode(lb));
}

/* Matter hue/sat -> sRGB 0..1 at full brightness. */
inline void hs_to_rgb(uint8_t hue, uint8_t sat, float *r, float *g, float *b)
{
    float h = hue * 6.0f / 254.0f, s = sat / 254.0f;
    int i = (int)h; float f = h - i;
    float p = 1.0f - s, q = 1.0f - s * f, t = 1.0f - s * (1.0f - f);
    switch (i % 6) {
        case 0: *r = 1; *g = t; *b = p; break;
        case 1: *r = q; *g = 1; *b = p; break;
        case 2: *r = p; *g = 1; *b = t; break;
        case 3: *r = p; *g = q; *b = 1; break;
        case 4: *r = t; *g = p; *b = 1; break;
        default: *r = 1; *g = p; *b = q; break;
    }
}

/* sRGB 0..1 -> Matter hue/sat (brightness is a separate param and dropped). */
inline Hs rgb_to_hs(float r, float g, float b)
{
    float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn;
    Hs out = { 0, 0 };
    if (mx <= 0.0f || d <= 0.0f) return out;
    float h;
    if (mx == r)      h = fmodf((g - b) / d, 6.0f);
    else if (mx == g) h = (b - r) / d + 2.0f;
    else              h = (r - g) / d + 4.0f;
    if (h < 0.0f) h += 6.0f;
    out.hue = (uint8_t)(h * 254.0f / 6.0f + 0.5f);
    out.sat = (uint8_t)(d / mx * 254.0f + 0.5f);
    return out;
}


/* Kelvin -> CIE xy on the Planckian locus (Kim et al. 2002), valid 1667..25000 K. */
inline void kelvin_to_xy(float T, float *x, float *y)
{
    T = T < 1667.0f ? 1667.0f : (T > 25000.0f ? 25000.0f : T);
    float t = 1000.0f / T, t2 = t * t, t3 = t2 * t;
    float cx = T <= 4000.0f
        ? -0.2661239f * t3 - 0.2343589f * t2 + 0.8776956f * t + 0.179910f
        : -3.0258469f * t3 + 2.1070379f * t2 + 0.2226347f * t + 0.240390f;
    float cx2 = cx * cx, cx3 = cx2 * cx;
    float cy = T <= 2222.0f ? -1.1063814f * cx3 - 1.34811020f * cx2 + 2.18555832f * cx - 0.20219683f
             : T <= 4000.0f ? -0.9549476f * cx3 - 1.37418593f * cx2 + 2.09137015f * cx - 0.16748867f
             :                 3.0817580f * cx3 - 5.87338670f * cx2 + 3.75112997f * cx - 0.37001483f;
    *x = cx; *y = cy;
}

} // namespace color_detail

/* Controller wrote CurrentX/CurrentY -> hue/sat for the driver. */
inline Hs xy_to_hs(uint16_t x, uint16_t y)
{
    float r, g, b;
    color_detail::xy_to_rgb(x, y, &r, &g, &b);
    return color_detail::rgb_to_hs(r, g, b);
}

/* Controller wrote ColorTemperatureMireds -> hue/sat for the driver. An RGB
   light has no white channel, so white is mixed: warm ends up a low-saturation
   orange, cool a faint blue. */
inline Hs ct_to_hs(uint16_t mireds)
{
    float x, y;
    color_detail::kelvin_to_xy(1000000.0f / (mireds ? mireds : 1), &x, &y);
    return xy_to_hs((uint16_t)(x * 65536.0f + 0.5f), (uint16_t)(y * 65536.0f + 0.5f));
}

/* Driver hue/sat changed -> CurrentX/CurrentY to report alongside it. */
inline void hs_to_xy(uint8_t hue, uint8_t sat, uint16_t *x, uint16_t *y)
{
    float r, g, b;
    color_detail::hs_to_rgb(hue, sat, &r, &g, &b);
    r = color_detail::srgb_decode(r); g = color_detail::srgb_decode(g); b = color_detail::srgb_decode(b);
    float X = 0.4124564f * r + 0.3575761f * g + 0.1804375f * b;
    float Y = 0.2126729f * r + 0.7151522f * g + 0.0721750f * b;
    float Z = 0.0193339f * r + 0.1191920f * g + 0.9503041f * b;
    float sum = X + Y + Z;
    float cx = sum > 0.0f ? X / sum : 0.3127f, cy = sum > 0.0f ? Y / sum : 0.3290f;
    uint32_t xi = (uint32_t)(cx * 65536.0f + 0.5f), yi = (uint32_t)(cy * 65536.0f + 0.5f);
    *x = xi > color_detail::kXyMax ? color_detail::kXyMax : (uint16_t)xi;
    *y = yi > color_detail::kXyMax ? color_detail::kXyMax : (uint16_t)yi;
}

} // namespace zc
