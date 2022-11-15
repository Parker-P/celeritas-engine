#pragma once
#include <filesystem>

namespace Settings
{
	class Paths
	{
		static inline auto _rootPath = []() -> std::filesystem::path { return std::filesystem::current_path(); };

	public:

		static inline auto _settings = []() -> std::filesystem::path { return _rootPath() /= L"src/engine/settings/Settings.json"; };
		static inline auto _vertexShaderPath = []() -> std::filesystem::path { return _rootPath() /= L"src/engine/shaders/VertexShader.vert"; };
		static inline auto _fragmentShaderPath = []() -> std::filesystem::path { return _rootPath() /= L"src/engine/shaders/FragmentShader.frag"; };
	};
}

