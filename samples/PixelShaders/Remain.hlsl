// Does nothing, serves as an example of a minimal pixel shader
SamplerState samplerState;
Texture2D shaderTexture : register(t0);
Texture2D prevShaderTexture : register(t1);

cbuffer PixelShaderSettings {
  float  Time;
  float  Scale;
  float2 Resolution;
  float4 Background;
};

float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_TARGET
{
    float4 color = shaderTexture.Sample(samplerState, tex);
    float4 prev = prevShaderTexture.Sample(samplerState, tex);
    float4 diff = max(prev-color, 0.0);
    float4 final = color + diff*0.75;
    return clamp(final, 0.0, 1.0);
}