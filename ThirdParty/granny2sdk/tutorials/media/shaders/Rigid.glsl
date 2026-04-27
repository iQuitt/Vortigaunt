uniform vec3 DirFromLight;
uniform vec4 LightColor;
uniform vec4 AmbientLightColor;

void main()
{
    vec3 XFormNormal = vec3(gl_ModelViewMatrix * vec4(gl_Normal,0));

    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_Position = ftransform();
    gl_FrontColor = LightColor * dot(XFormNormal, DirFromLight) + AmbientLightColor;
}
