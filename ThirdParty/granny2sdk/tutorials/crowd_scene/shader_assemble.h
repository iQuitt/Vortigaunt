#pragma once
// =====================================================================
//  Simple utility to easily paste the text of a shader together
// =====================================================================

// Badly named, I know, but hey, it "assembles" the pieces of text
class ShaderAssembler
{
public:
    ShaderAssembler();
    ~ShaderAssembler();

    void AddLine(const char* Fmt, ...);
    const char* GetShaderText();

    void ClearShader();

private:
    char* ShaderText;
    char* CurrentShaderEnd;
};
