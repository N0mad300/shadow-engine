@echo off

if not exist "bin" mkdir "bin"

:: --------------------------
:: Shader Compilation Settings
:: --------------------------

:: Vertex Shader Compilation
set VS_SHADER_INPUT=src/nuklear_d3d11.hlsl
set VS_SHADER_OUTPUT=src/nuklear_d3d11_vertex_shader.h
set VS_HEADER_VAR=nk_d3d11_vertex_shader
set VS_ENTRY_POINT=vs
set VS_SHADER_TARGET=vs_5_0
set VS_FLAGS=/nologo /T %VS_SHADER_TARGET% /E %VS_ENTRY_POINT% /O3 /Zpc /Ges /Fh %VS_SHADER_OUTPUT% /Vn %VS_HEADER_VAR% /Qstrip_reflect /Qstrip_debug /Qstrip_priv

fxc.exe %VS_FLAGS% %VS_SHADER_INPUT%

:: Pixel Shader Compilation
set PS_SHADER_INPUT=src/nuklear_d3d11.hlsl
set PS_SHADER_OUTPUT=src/nuklear_d3d11_pixel_shader.h
set PS_HEADER_VAR=nk_d3d11_pixel_shader
set PS_ENTRY_POINT=ps
set PS_SHADER_TARGET=ps_5_0
set PS_FLAGS=/nologo /T %PS_SHADER_TARGET% /E %PS_ENTRY_POINT% /O3 /Zpc /Ges /Fh %PS_SHADER_OUTPUT% /Vn %PS_HEADER_VAR% /Qstrip_reflect /Qstrip_debug /Qstrip_priv

fxc.exe %PS_FLAGS% %PS_SHADER_INPUT%

:: -------------------------------
:: Compilation of Main Program
:: -------------------------------

:: Compiler Flags for Main Program
set CL_FLAGS=/nologo /W4 /O2 /fp:precise /Gm-
set CL_INPUT=src/main.c
set CL_OUTPUT="bin/Shadow Engine.exe"
set CL_LIBS=user32.lib dxguid.lib d3d11.lib

:: Compile and Link
cl %CL_FLAGS% /Fe%CL_OUTPUT% /Fo"bin/" %CL_INPUT% %CL_LIBS% /link /incremental:no