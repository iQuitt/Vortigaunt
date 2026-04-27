@echo off
call "%DXSDK_DIR%\Utilities\bin\x86\fxc.exe" /nologo /E PixShader /D EntryPoint_PixShader /Zi /Od /Tps_2_0 /Fo"media\PixelShader.xpu" %1
call "%DXSDK_DIR%\Utilities\bin\x86\fxc.exe" /nologo /E MorphVS /D EntryPoint_MorphVS /Zi /Od /Tvs_2_0 /Fo"media\MorphVS.xvu" %1
