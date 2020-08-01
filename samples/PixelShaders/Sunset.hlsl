// In order to run in KodeLife (great Shader IDE and free with some nagging),
//  define the following:
// #define KODELIFE

#ifdef KODELIFE
#define MAIN        ps_main
#else
#define MAIN        main
#endif

#define TIME        Time
#define RESOLUTION  Resolution
#define SCALE       Scale

#define PI          3.141592654
#define TAU         (2.0*PI)
#define MROT(a)     float2x2(cos(a), sin(a), -sin(a), cos(a))

Texture2D shaderTexture;
SamplerState samplerState;

cbuffer PixelShaderSettings {
  float  Time;
  float  Scale;
  float2 Resolution;
  float4 Background;
};

static const float  gravity = 1.0;
static const float3 sunDir  = normalize(float3(0, 0.06, 1));
static const float3 skyCol1 = float3(0.6, 0.35, 0.3);
static const float3 skyCol2 = float3(1.0, 0.3, 0.3);
static const float3 sunCol1 = float3(1.0,0.5,0.4);
static const float3 sunCol2 = float3(1.0,0.8,0.7);
static const float3 seaCol1 = float3(0.1,0.2,0.2);
static const float3 seaCol2 = float3(0.8,0.9,0.6);

static const float2x2 rotNone = MROT(0.0);
static const float2x2 rotSome = MROT(1.0);

float2 wave(in float t, in float a, in float w, in float p) {
  float x = t;
  float y = a*sin(t*w + p);
  return float2(x, y);
}

float2 dwave(in float t, in float a, in float w, in float p) {
  float dx = 1.0;
  float dy = a*w*cos(t*w + p);
  return float2(dx, dy);
}

float2 gravityWave(in float t, in float a, in float k, in float h) {
  float w = sqrt(gravity*k*tanh(k*h));
  return wave(t, a, k, w*TIME);
}

float2 gravityWaveD(in float t, in float a, in float k, in float h) {
  float w = sqrt(gravity*k*tanh(k*h));
  return dwave(t, a, k, w*TIME);
}

float4 sea(in float2 p, in float ia) {
  float y = 0.0;
  float3 d = float3(0.0, 0.0, 0.0);

  const int maxIter = 7;

  const float kk = 1.225;
  const float aa = 1.0/(kk*kk);
  const float pp = 0.0;
  float k = 1.0*pow(kk, pp);
  float a = 1.0*pow(aa, pp)*0.075;

  float h = 10.0;
  p *= 0.45;

  float2 waveDir = float2(0.0, 1.0);

  for (int n = 0; n < maxIter; ++n) {
    float t = dot(waveDir, p) + float(n);
    y += gravityWave(t, a, k, h).y;
    float2 dw = gravityWaveD(t, a, k, h);

    float2 d2 = float2(0.0, dw.x);

    d += float3(waveDir.x, dw.y, waveDir.y);

    float2x2 rot = n > 1 ? rotSome : rotNone;

    waveDir = mul(rot, waveDir);

    k *= kk;
    a *= aa;
  }

  float3 t = normalize(d);
  float3 nxz = normalize(float3(t.z, 0.0, -t.x));
  float3 nor = cross(t, nxz);

  return float4(y, nor);
}

float3 skyColor(in float3 rd) {
  float sunDot = max(dot(rd, sunDir), 0.0);
  float3 final = float3(0.0, 0.0, 0.0);
  final += lerp(skyCol1, skyCol2, rd.y);
  final += 0.5*sunCol1*pow(sunDot, 90.0);
  final += 4.0*sunCol2*pow(sunDot, 900.0);
  return final;
}

// Computes the color given the ray origin and texture coord p [-1, 1]
float3 color(float3 ro, float3 rd) {
  float dsea = (0.0 - ro.y)/rd.y;
  float3 sky = skyColor(rd);

  float3 col = float3(0.0, 0.0, 0.0);

  if (dsea > 0.0) {
    float3 p = ro + dsea*rd;
    float4 s = sea(p.xz, 1.0);
    float h = s.x;
    float3 nor = s.yzw;
    nor = lerp(nor, float3(0.0, 1.0, 0.0), smoothstep(0.0, 200.0, dsea));

    float fre = clamp(1.0 - dot(-nor,rd), 0.0, 1.0);
    fre = pow(fre, 3.0);
    float dif = lerp(0.25, 1.0, max(dot(nor,sunDir), 0.0));

    float3 refl = skyColor(reflect(rd, nor));
    float3 refr = seaCol1 + dif*sunCol1*seaCol2*0.1;
    col = lerp(refr, refl, fre);

    float atten = max(1.0 - dot(dsea,dsea) * 0.001, 0.0);
    col += seaCol2*(p.y - h) * 2.0 * atten;

    col = lerp(col, sky, 1.0 - exp(-0.01*dsea));
  } else {
    col = sky;
  }

  return col;
}

float4 MAIN(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET {
  float2 reso = RESOLUTION;
  float2 q = tex;
  float2 p = -1. + 2. * q;
  p.y *= -1;
  p.x *= reso.x/reso.y;

  float3 ro = float3(0.0, 10.0, 0.0);
  float3 ww = normalize(float3(0.0, -0.125, 1.0));
  float3 uu = normalize(cross(float3(0.0,1.0,0.0), ww));
  float3 vv = normalize(cross(ww,uu));
  float3 rd = normalize(p.x*uu + p.y*vv + 2.5*ww);

  float3 col = color(ro, rd);

  float4 s = shaderTexture.Sample(samplerState, q);
  float4 ss = shaderTexture.Sample(samplerState, q+2.0*Scale*float2(-1.0, -1.0)/RESOLUTION.y);

  col = lerp(col, float(0.0), ss.w);
  col = lerp(col, s.xyz, s.w);


  return float4(col, 1.0);
}
