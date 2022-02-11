#include "SpriteShader.hlsli"

// Standard spritesheet vertex shader
// Used when recreating a default root signature

cbuffer Parameters : register(b0)
{
    row_major float4x4 MatrixTransform;
};

[RootSignature(MainRS)]
void main(
    inout float4 color : COLOR0,
    inout float2 texCoord : TEXCOORD0,
    inout float4 position : SV_Position) : SV_Position
{
    position = mul(position, MatrixTransform);
}