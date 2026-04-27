struct VS_IN_RIGID
{
    float3  Position0 : POSITION0;
    float3  Normal    : NORMAL0;
    float2  Tex0      : TEXCOORD0;
    float3  Position1 : POSITION1;
    float3  Normal1   : NORMAL1;
    float3  Position2 : POSITION2;
    float3  Normal2   : NORMAL2;
    float3  Position3 : POSITION3;
    float3  Normal3   : NORMAL3;
};

struct VS_OUT
{
    float4 Position   : POSITION;
    float4 Diffuse    : COLOR0;
};

struct PS_INPUT
{
    float4 Diffuse : COLOR0;
};


// Must match the #defines in dx_sample.cpp
float4x4 ObjToWorld  : register(c0);
float4x3 WorldToView : register(c4);
float4x4 ViewToClip  : register(c8);
float3 DirFromLight  : register(c12);
float4 LightColour   : register(c13);
float4 AmbientColour : register(c14);
float3 MorphWeights  : register(c15);

VS_OUT MorphVS( VS_IN_RIGID In )
{
    float3 ObjPos = In.Position0;
    ObjPos += In.Position1 * MorphWeights.x;
    ObjPos += In.Position2 * MorphWeights.y;
    ObjPos += In.Position3 * MorphWeights.z;

    VS_OUT Out;
    float4 WorldPos = mul(float4(ObjPos, 1), ObjToWorld);
    float4 ViewPos  = float4(mul(WorldPos, WorldToView), 1);
    float4 ClipPos  = mul(ViewPos, ViewToClip);

    float3 ObjNormal = In.Normal;
    ObjNormal += In.Normal1 * MorphWeights.x;
    ObjNormal += In.Normal2 * MorphWeights.y;
    ObjNormal += In.Normal3 * MorphWeights.z;
    ObjNormal = normalize(ObjNormal);
    float3 Normal = (float3)mul(float4(ObjNormal, 0), ObjToWorld);

    Out.Position = ClipPos;
    Out.Diffuse  = LightColour * dot(normalize(Normal), DirFromLight) + AmbientColour;

    return Out;
}


float4 PixShader ( PS_INPUT In ) : COLOR
{
    return In.Diffuse;
}
