#include "SpriteShader.hlsli"

Texture2D<float4> StartTexture : register(t0);
sampler TextureSampler : register(s0);

[RootSignature(SinglePostProcessRS)]
float4 main(float4 color : COLOR0, float2 texCoord : TEXCOORD0) : SV_Target0
{
    if (maxInterference == 0)
    {
        float4 tex0Color = StartTexture.Sample(TextureSampler, texCoord);
        return (tex0Color * color);
    }
    int xBlock = texCoord.x / barThickness;
    int yBlock = texCoord.y / (barThickness * 1.6f);
    float xTranslate = maxInterference * (1.f - deltaT) * sin(deltaT * yBlock * 3.145926f);
    float newX = texCoord.x + xTranslate;
    texCoord.x = newX;
    float4 texColor = StartTexture.Sample(TextureSampler, texCoord);
    texColor[3] = texColor[3] * sin(deltaT);
    return texColor;
}