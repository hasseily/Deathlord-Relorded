#include "SpriteShader.hlsli"

Texture2D<float4> StartTexture : register(t0);
//Texture2D<float4> EndTexture : register(t1);
sampler TextureSampler : register(s0);

[RootSignature(SinglePostProcessRS)]
float4 main(float4 color : COLOR0, float2 texCoord : TEXCOORD0) : SV_Target0
{
    int yBlock = texCoord.y / barThickness;
    // float4 endF4 = EndTexture.Sample(TextureSampler, texCoord);
    float xTranslate = maxInterference * sin(yBlock*10) * ((deltaT * 10000) % 100) / 100;
    float newX = texCoord.x + xTranslate;
    float4 startF4 = StartTexture.Sample(TextureSampler, texCoord);
    if (abs(newX) < (1.f - texCoord.x))
    {
        return startF4;
    }
    return float4(0.f,0.f,0.f,0.f);
}