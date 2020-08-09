#pragma once

#ifdef __INSIDE_WINDOWS
constexpr char errorPixelShaderString[] = "";
constexpr char retroPixelShaderString[] = "";
constexpr char retroIIPixelShaderString[] = "";
#else
constexpr char errorPixelShaderString[] = R"(
// Shader used to indicate something went wrong during shader loading
Texture2D shaderTexture;
SamplerState samplerState;

cbuffer PixelShaderSettings {
  float  Time;
  float  Scale;
  float2 Resolution;
  float4 Background;
};

float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
    float4 color = shaderTexture.Sample(samplerState, tex);
    float bars = 0.5+0.5*sin(tex.y*100);
    color.x += pow(bars, 20.0);
    return color;
}
)";

constexpr char retroPixelShaderString[] = R"(
// The original retro pixel shader
Texture2D shaderTexture;
SamplerState samplerState;

cbuffer PixelShaderSettings {
  float  Time;
  float  Scale;
  float2 Resolution;
  float4 Background;
};

#define SCANLINE_FACTOR 0.5
#define SCALED_SCANLINE_PERIOD Scale
#define SCALED_GAUSSIAN_SIGMA (2.0*Scale)

static const float M_PI = 3.14159265f;

float Gaussian2D(float x, float y, float sigma)
{
    return 1/(sigma*sqrt(2*M_PI)) * exp(-0.5*(x*x + y*y)/sigma/sigma);
}

float4 Blur(Texture2D input, float2 tex_coord, float sigma)
{
    uint width, height;
    shaderTexture.GetDimensions(width, height);

    float texelWidth = 1.0f/width;
    float texelHeight = 1.0f/height;

    float4 color = { 0, 0, 0, 0 };

    int sampleCount = 13;

    for (int x = 0; x < sampleCount; x++)
    {
        float2 samplePos = { 0, 0 };

        samplePos.x = tex_coord.x + (x - sampleCount/2) * texelWidth;
        for (int y = 0; y < sampleCount; y++)
        {
            samplePos.y = tex_coord.y + (y - sampleCount/2) * texelHeight;
            if (samplePos.x <= 0 || samplePos.y <= 0 || samplePos.x >= width || samplePos.y >= height) continue;

            color += input.Sample(samplerState, samplePos) * Gaussian2D((x - sampleCount/2), (y - sampleCount/2), sigma);
        }
    }

    return color;
}

float SquareWave(float y)
{
    return 1 - (floor(y / SCALED_SCANLINE_PERIOD) % 2) * SCANLINE_FACTOR;
}

float4 Scanline(float4 color, float4 pos)
{
    float wave = SquareWave(pos.y);

    // TODO:GH#3929 make this configurable.
    // Remove the && false to draw scanlines everywhere.
    if (length(color.rgb) < 0.2 && false)
    {
        return color + wave*0.1;
    }
    else
    {
        return color * wave;
    }
}

float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
    Texture2D input = shaderTexture;

    // TODO:GH#3930 Make these configurable in some way.
    float4 color = input.Sample(samplerState, tex);
    color += Blur(input, tex, SCALED_GAUSSIAN_SIGMA)*0.3;
    color = Scanline(color, pos);

    return color;
}
)";

constexpr char retroIIPixelShaderString[] = R"(
// In order to us Kodelife (a shader IDE), uncomment the following line:
// #define KODELIFE

#ifdef KODELIFE
#define DOWNSCALE   3.0
#define MAIN        ps_main
#else
#define DOWNSCALE   (2.0*Scale)
#define MAIN        main
#endif

#define PI          3.141592654
#define TAU         (2.0*PI)

Texture2D shaderTexture;
SamplerState samplerState;

cbuffer PixelShaderSettings {
  float  Time;
  float  Scale;
  float2 Resolution;
  float4 Background;
};

// ----------------------------------------------------------------------------
// HSV to RGB and back
//  From: https://stackoverflow.com/questions/15095909/from-rgb-to-hsv-in-opengl-glsl
//  Blog: http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
//  Note: Stackoverflow TOS in 2013 required all user content to be licensed under CC BY-SA 3.0.
//  Copyright (C) 2013 Sam Hocevar. This work is licensed under a CC BY-SA 3.0 license.
float3 hsv2rgb(float3 c) {
  const float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float3 rgb2hsv(float3 c) {
  const float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
  float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));
  float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));

  float d = q.x - min(q.w, q.y);
  float e = 1.0e-10;
  return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Ray sphere intersection
//  From: https://www.shadertoy.com/view/4d2XWV
//  Blog: https://www.iquilezles.org/www/articles/intersectors/intersectors.htm
// The MIT License
// Copyright (C) 2014 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
float raySphere(float3 ro, float3 rd, float4 sph) {
  float3 oc = ro - sph.xyz;
  float b = dot(oc, rd);
  float c = dot(oc, oc) - sph.w*sph.w;
  float h = b*b - c;
  if (h<0.0) return -1.0;
  h = sqrt(h);
  return -b - h;
}
// ----------------------------------------------------------------------------

float psin(float a) {
  return 0.5+0.5*sin(a);
}

float3 sampleHSV(float2 p) {
  float2 cp = abs(p - 0.5);
  float4 s = shaderTexture.Sample(samplerState, p);
  float3 col = lerp(Background.xyz, s.xyz, s.w);
  return rgb2hsv(col)*step(cp.x, 0.5)*step(cp.y, 0.5);
}

float3 screen(float2 reso, float2 p, float diff, float spe) {
  float sr = reso.y/reso.x;
  float res=reso.y/DOWNSCALE;

  // Lots of experimentation lead to these rows

  float2 ap = p;
  ap.x *= sr;

  // tanh is such a great function!

  // Viginetting
  float2 vp = ap + 0.5;
  float vig = tanh(pow(max(100.0*vp.x*vp.y*(1.0-vp.x)*(1.0-vp.y), 0.0), 0.35));

  ap *= 1.025;

  // Screen at coord
  float2 sp = ap;
  sp += 0.5;
  float3 shsv = sampleHSV(sp);

  // Scan line brightness
  float scanbri = lerp(0.25, 2.0, psin(PI*res*p.y));

  shsv.z *= scanbri;
  shsv.z = tanh(1.5*shsv.z);
  shsv.z *= vig;

  // Simulate bad CRT screen
  float dist = (p.x+p.y)*0.05;
  shsv.x += dist;


  float3 col = float3(0.0, 0.0, 0.0);
  col += hsv2rgb(shsv);
  col += (0.35*spe+0.25*diff)*vig;

  return col;
}

// Computes the color given the ray origin and texture coord p [-1, 1]
float3 color(float2 reso, float3 ro, float2 p) {
  // Quick n dirty way to get ray direction
  float3 rd = normalize(float3(p, 2.0));

  // The screen is imagined to be projected on a large sphere to give it a curve
  const float radius = 20.0;
  const float4 center = float4(0.0, 0.0, radius, radius-1.0);
  float3 lightPos = 0.95*float3(-1.0, -1.0, 0.0);

  // Find the ray sphere intersection, basically a single ray tracing step
  float sd = raySphere(ro, rd, center);

  if (sd > 0.0) {
    // sp is the point on sphere where the ray intersected
    float3 sp = ro + sd*rd;
    // Normal of the sphere allows to compute lighting
    float3 nor = normalize(sp - center.xyz);
    float3 ld = normalize(lightPos - sp);

    // Diffuse lighting
    float diff = max(dot(ld, nor), 0.0);
    // Specular lighting
    float spe = pow(max(dot(reflect(rd, nor), ld), 0.0),30.0);

    // Due to how the scene is setup up we cheat and use sp.xy as the screen coord
    return screen(reso, sp.xy, diff, spe);
  } else {
    return float3(0.0, 0.0, 0.0);
  }
}

float4 MAIN(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET {
  float2 reso = Resolution;
  float2 q = tex;
  float2 p = -1. + 2. * q;
  p.x *= reso.x/reso.y;

  float3 ro = float3(0.0, 0.0, 0.0);
  float3 col = color(reso, ro, p);

  return float4(col, 1.0);
}
)";
#endif
