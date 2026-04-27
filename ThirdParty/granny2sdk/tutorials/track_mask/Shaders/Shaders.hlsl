struct VS_IN
{
    float3  Position        : POSITION0;
    float4  BoneWeights     : BLENDWEIGHT;
    float4  BoneIndices     : BLENDINDICES;
    float3  Normal          : NORMAL;
    float2  Tex0            : TEXCOORD0;
};

struct VS_IN_RIGID
{
    float3  Position        : POSITION0;
    float3  Normal          : NORMAL;
    float2  Tex0            : TEXCOORD0;
};

struct VS_OUT
{
    float4 Position         : POSITION;
    float2 Tex0             : TEXCOORD0;
    float4 Diffuse          : COLOR0;
};

struct PS_INPUT
{
    float4 Diffuse : COLOR0;
    float2 Texture : TEXCOORD0;
};


// Must match the #defines in dx_sample.cpp
float4x3 ObjToWorld  : register(c0);
float4x3 WorldToView : register(c4);
float4x4 ViewToClip  : register(c8);
float3 DirFromLight  : register(c12);
float4 LightColour   : register(c13);
float4 AmbientColour : register(c14);

// Note that 64 was chosen because of the art authored for
//  this demo, not for any deep reason.
float4x3 BoneMatrices[64] : register(c15);


#define TRILINEAR_SAMPLER sampler_state { MipFilter = NEAREST; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; }
sampler2D diffuse_texture : register(s0) = TRILINEAR_SAMPLER;

VS_OUT SkinVSVertexFetch( VS_IN In )
{
    VS_OUT Out;

    float BoneWeights[4] = (float[4])In.BoneWeights;
    int BoneIndices[4]   = (int[4])In.BoneIndices;

    float4 InPos     = float4( In.Position, 1 );
    float4 InNormal  = float4( In.Normal, 0 );
    float3 WorldPos    = 0;
    float3 WorldNormal = 0;

    for( int i = 0; i < 4; ++i )
    {
        float4x3 BoneMatrix = BoneMatrices[BoneIndices[i]];
        WorldPos    += BoneWeights[i] * mul( InPos, BoneMatrix );
        WorldNormal += BoneWeights[i] * mul( InNormal, BoneMatrix );
    }

    float3 ViewPos = mul(float4(WorldPos, 1), WorldToView);
    float4 ClipPos = mul(float4(ViewPos, 1), ViewToClip);
    Out.Position   = ClipPos;

    Out.Tex0    = In.Tex0;
    Out.Diffuse = LightColour * dot(normalize(WorldNormal), DirFromLight) + AmbientColour;

    return Out;
}

VS_OUT RigidVS( VS_IN_RIGID In )
{
    VS_OUT Out;
    float3 WorldPos = mul(float4(In.Position, 1), ObjToWorld);
    float3 ViewPos  = mul(float4(WorldPos, 1), WorldToView);
    float4 ClipPos  = mul(float4(ViewPos, 1), ViewToClip);

    float4 InNormal  = float4( In.Normal, 0);
    float3 ObjNormal = mul(InNormal, ObjToWorld);

    Out.Position = ClipPos;
    Out.Tex0     = In.Tex0;
    Out.Diffuse  = LightColour * dot(ObjNormal, DirFromLight) + AmbientColour;

    return Out;
}


float4 PixShader ( PS_INPUT In ) : COLOR
{
    float4 DiffuseSamp = tex2D( diffuse_texture, In.Texture );
    DiffuseSamp *= In.Diffuse;

    return DiffuseSamp;
}
