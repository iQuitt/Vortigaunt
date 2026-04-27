@echo off
call "%DXSDK_DIR%\Utilities\bin\x86\fxc.exe" /nologo /E RigidVS /D EntryPoint_RigidVS /Zi /Od /Tvs_2_0 /Fo"media\RigidVS.xvu" %1
call "%DXSDK_DIR%\Utilities\bin\x86\fxc.exe" /nologo /E SkinVSVertexFetch /D EntryPoint_SkinVSVertexFetch /Zi /Od /Tvs_2_0 /Fo"media\SkinVSVertexFetch.xvu" %1
call "%DXSDK_DIR%\Utilities\bin\x86\fxc.exe" /nologo /E PixShader /D EntryPoint_PixShader /Zi /Od /Tps_2_0 /Fo"media\PixelShader.xpu" %1