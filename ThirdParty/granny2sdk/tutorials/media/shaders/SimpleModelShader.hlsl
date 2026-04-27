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
    float3 Diffuse          : COLOR0;
};

struct PS_INPUT
{
    float3 Diffuse : COLOR0;
    float2 Texture : TEXCOORD0;
};


// Must match the #defines in dx_sample.cpp
float4x3 ObjToWorld;
float4x3 WorldToView;
float4x4 ViewToClip;
float3   DirFromLight;
float4   LightColour;
float4   AmbientColour;

// Note that 64 was chosen because of the art authored for
//  this demo, not for any deep reason.
float4x3  BoneMatrices[80];


//#define TRILINEAR_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; }
texture diffuse_texture;
sampler2D diffuse_sampler = sampler_state
{
    Texture = <diffuse_texture>;
    MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP;
};

VS_OUT SkinnedVS( VS_IN In )
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
    Out.Diffuse = LightColour * dot(float4(normalize(WorldNormal),0), DirFromLight) + AmbientColour;

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


float4 GrannyPS ( PS_INPUT In ) : COLOR
{
    float4 DiffuseSamp = tex2D( diffuse_sampler, In.Texture );
    DiffuseSamp *= float4(In.Diffuse,1);
    return DiffuseSamp;
}

technique Rigid
{
    pass Base
    {
        VertexShader = compile vs_2_0 RigidVS();
        PixelShader = compile ps_2_0 GrannyPS();
    }
}

technique Skinned
{
    pass Base
    {
        VertexShader = compile vs_2_0 SkinnedVS();
        PixelShader = compile ps_2_0 GrannyPS();
    }
}
