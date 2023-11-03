@echo off
%VULKAN_SDK%\Bin\glslc.exe "graphics\VertexShader.vert" -o "graphics\VertexShader.spv"
%VULKAN_SDK%\Bin\glslc.exe "graphics\FragmentShader.frag" -o "graphics\FragmentShader.spv"
%VULKAN_SDK%\Bin\glslc.exe "compute\BoxBlur.comp" -o "compute\BoxBlur.spv"
echo Compilation complete...
pause