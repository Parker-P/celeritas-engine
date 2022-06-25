@echo off
%VULKAN_SDK%\Bin\glslc.exe "VertexShader.vert" -o "VertexShader.spv"
%VULKAN_SDK%\Bin\glslc.exe "FragmentShader.frag" -o "FragmentShader.spv"
echo Compilation complete...
pause