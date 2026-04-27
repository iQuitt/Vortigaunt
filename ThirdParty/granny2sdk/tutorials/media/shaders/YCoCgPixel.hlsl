struct PS_INPUT
{
    float3 Diffuse : COLOR0;
    float2 Texture : TEXCOORD0;
};

texture diffuse_texture;
sampler2D diffuse_sampler = sampler_state
{
    Texture = <diffuse_texture>;
    MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP;
};

float4 YCoCgPS ( PS_INPUT In ) : COLOR
{
    float4 ycocg = tex2D( diffuse_sampler, In.Texture );

    float y  = ycocg.a;
    float co = ycocg.r - 0.5;
    float cg = ycocg.g - 0.5;

    return float4(y + co - cg,
                  y + cg, 
                 y - co - cg,
                  1);
}

