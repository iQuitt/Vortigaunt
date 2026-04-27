// =====================================================================
//  Simple utility to easily paste the text of a shader together
// =====================================================================
#include "shader_assemble.h"
#include "crowd_scene.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <windows.h>


// Thats a Big Shader!
static const int MaxShaderLength = 10 * 1024 - 1;

ShaderAssembler::ShaderAssembler()
    : ShaderText(new char[MaxShaderLength + 1]),
      CurrentShaderEnd(0)
{
    ClearShader();
}

ShaderAssembler::~ShaderAssembler()
{
    delete [] ShaderText;
    ShaderText = 0;

    CurrentShaderEnd = 0;
}


void ShaderAssembler::ClearShader()
{
    ASSERT(ShaderText != 0);

    CurrentShaderEnd = ShaderText;
    *CurrentShaderEnd = '\0';
}


void ShaderAssembler::AddLine(const char* Fmt, ...)
{
    ASSERT(Fmt);

    va_list marker;
    va_start ( marker, Fmt );

    vsprintf ( CurrentShaderEnd, Fmt, marker );
    CurrentShaderEnd += strlen ( CurrentShaderEnd );
    *CurrentShaderEnd++ = '\n';
    *CurrentShaderEnd = '\0';

    // TODO properly nprintf this...
    // A bit late now - you've already scribbled. But at least we'll know.
    ASSERT ( ( CurrentShaderEnd - ShaderText ) < MaxShaderLength );

    va_end ( marker );
}


const char* ShaderAssembler::GetShaderText()
{
    return ShaderText;
}
