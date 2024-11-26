@echo off
set "script_dir=%~dp0"
%VULKAN_SDK%\Bin\glslc.exe "%script_dir%\graphics\VertexShader.vert" -o "%script_dir%\graphics\VertexShader.spv"
%VULKAN_SDK%\Bin\glslc.exe "%script_dir%\graphics\FragmentShader.frag" -o "%script_dir%\graphics\FragmentShader.spv"
%VULKAN_SDK%\Bin\glslc.exe "%script_dir%\graphics\NuklearUIVertexShader.vert" -o "%script_dir%\graphics\NuklearUIVertexShader.spv"
%VULKAN_SDK%\Bin\glslc.exe "%script_dir%\graphics\NuklearUIFragmentShader.frag" -o "%script_dir%\graphics\NuklearUIFragmentShader.spv"
%VULKAN_SDK%\Bin\glslc.exe "%script_dir%\compute\BoxBlur.comp" -o "%script_dir%\compute\BoxBlur.spv"
echo Shader compilation complete...