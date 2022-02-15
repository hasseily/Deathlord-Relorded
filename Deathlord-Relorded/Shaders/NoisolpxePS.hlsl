#include "SpriteShader.hlsli"

Texture2D<float4> StartTexture : register(t0);
//Texture2D<float4> EndTexture : register(t1);
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
    // float4 endF4 = EndTexture.Sample(TextureSampler, texCoord);
    float xTranslate = maxInterference * (1.f - deltaT) * sin(deltaT * yBlock * 3.145926f);
    float yTranslate = maxInterference * (1.f - deltaT) * sin(deltaT * xBlock * 3.145926f);
    float newX = texCoord.x + xTranslate;
    float newY = texCoord.y + yTranslate;
    texCoord.x = newX;
    texCoord.y = newY;
    float4 texColor = StartTexture.Sample(TextureSampler, texCoord);
    texColor[3] = texColor[3] * sin(deltaT);
    return texColor;
}