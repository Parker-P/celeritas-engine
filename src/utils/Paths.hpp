#pragma once
#include <filesystem>

namespace Utils
{
	class Paths
	{
		static std::filesystem::path _gameSettings;
		static std::filesystem::path _engineSettings;
		static std::filesystem::path _vertexShaderPath;
		static std::filesystem::path _fragmentShaderPath;
	};
}

