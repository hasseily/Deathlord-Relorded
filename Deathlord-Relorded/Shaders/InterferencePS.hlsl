#include "SpriteShader.hlsli"

Texture2D<float4> StartTexture : register(t0);
//Texture2D<float4> EndTexture : register(t1);
sampler TextureSampler : register(s0);

[RootSignature(SinglePostProcessRS)]
float4 main(float4 color : COLOR0, float2 texCoord : TEXCOORD0) : SV_Target0
{
    if (maxInterference == 0)
    {
        float4 color = StartTexture.Sample(TextureSampler, texCoord);
        return color;
    }
    int yBlock = texCoord.y / barThickness;
    // float4 endF4 = EndTexture.Sample(TextureSampler, texCoord);
    float xTranslate = maxInterference * sin(yBlock * 10) * (1.f - deltaT);
    float newX = texCoord.x + xTranslate;
    float4 startF4 = StartTexture.Sample(TextureSampler, texCoord);
    texCoord.x = newX;
    return StartTexture.Sample(TextureSampler, texCoord);
}