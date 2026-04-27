attribute vec3 MyNormal;
attribute vec4 BlendWeights;
attribute vec4 BlendIndices;

uniform mat4 BoneMatrices[32];

uniform vec3 DirFromLight;
uniform vec4 LightColor;
uniform vec4 AmbientLightColor;


void main()
{
    vec4 summedPos = vec4(0, 0, 0, 0);
    vec3 summedNorm = vec3(0, 0, 0);

    // We're going to assume that we can transform the normal by the matrix, which is not
    // technically correct unless we're positive there's not a scaling factor involved.
    for (int i = 0; i < 4; ++i)
    {
        mat4 BoneMatrix = BoneMatrices[int(BlendIndices[i])];
        summedPos  += (BoneMatrix * gl_Vertex) * BlendWeights[i];
        summedNorm += vec3((BoneMatrix * vec4(MyNormal, 0.0)) * BlendWeights[i]);
    }

    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_Position    = gl_ModelViewProjectionMatrix * summedPos;
    gl_FrontColor  = LightColor * dot(normalize(summedNorm), DirFromLight) + AmbientLightColor;
}
