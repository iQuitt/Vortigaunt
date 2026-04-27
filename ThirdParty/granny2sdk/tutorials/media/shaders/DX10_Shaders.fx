struct VS_IN
{
    float3  Position        : POSITION;
    float4  BoneWeights     : BLENDWEIGHTS;
    uint4   BoneIndices     : BLENDINDICES;
    float3  Normal          : NORMAL;
    float2  Tex0            : TEXCOORD;
};

struct VS_IN_RIGID
{
    float3  Position        : POSITION;
    float3  Normal          : NORMAL;
    float2  Tex0            : TEXCOORD;
};

struct VS_OUT
{
    float4 Position         : SV_POSITION;
    float2 Tex0             : TEXCOORD;
    float3 Diffuse          : COLOR;
};


// Must match the #defines in dx_sample.cpp
float3   DirFromLight;
float4   LightColour;
float4   AmbientColour;

float4x4 ObjToWorld;
float4x4 WorldToView;
float4x4 ViewToClip;

// Note that 64 was chosen because of the art authored for
//  this demo, not for any deep reason.
float4x3  BoneMatrices[64];


//--------------------------------------------------------------------------------------
// DepthStates
//--------------------------------------------------------------------------------------
DepthStencilState EnableDepth
{
    DepthEnable = TRUE;
    DepthWriteMask = ALL;
    DepthFunc = LESS_EQUAL;
    //DepthFunc = ALWAYS;
};


//--------------------------------------------------------------------------------------
// Diffuse Texture
//--------------------------------------------------------------------------------------
Texture2D  diffuse_texture;
SamplerState DiffuseSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};


//--------------------------------------------------------------------------------------
// Bone matrices
//
// (256 is chosen to be larger than the bone count of typical
// characters, plus it's the largest number that can be referenced by a
// UINT8, of course.)
//--------------------------------------------------------------------------------------
#define MAX_BONE_MATRICES 256
cbuffer BoneMatrices
{
    matrix g_mConstBoneWorld[MAX_BONE_MATRICES];
};


// Sealed up in a function so it's easy to switch back to constant
// registers or tbuffers.
matrix FetchMatrix( uint BoneIdx )
{
    return g_mConstBoneWorld[ BoneIdx ];
}


VS_OUT SkinnedVS( VS_IN In )
{
    VS_OUT Out;

    float4 InPos     = float4( In.Position, 1 );
    float4 InNormal  = float4( In.Normal, 0 );
    float3 WorldPos    = 0;
    float3 WorldNormal = 0;

    for( int i = 0; i < 4; ++i )
    {
        matrix BoneMatrix = FetchMatrix(In.BoneIndices[i]);

        WorldPos    += In.BoneWeights[i] * mul( InPos,    BoneMatrix );
        WorldNormal += In.BoneWeights[i] * mul( InNormal, BoneMatrix );
    }

    float3 ViewPos = mul(float4(WorldPos, 1), WorldToView);
    float4 ClipPos = mul(float4(ViewPos, 1), ViewToClip);

    Out.Position = ClipPos;
    Out.Tex0     = In.Tex0;
    Out.Diffuse  = LightColour * dot(float4(normalize(WorldNormal),0), DirFromLight) + AmbientColour;

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
    Out.Diffuse  = LightColour * dot(float4(ObjNormal,0), DirFromLight) + AmbientColour;

    return Out;
}


float4 GrannyPS ( VS_OUT In ) : SV_Target
{
    return diffuse_texture.Sample(DiffuseSampler, In.Tex0) * float4(In.Diffuse, 1);
}

technique10 Rigid
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, RigidVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, GrannyPS( ) ) );

        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 Skinned
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, SkinnedVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, GrannyPS( ) ) );

        SetDepthStencilState( EnableDepth, 0 );
    }
}
