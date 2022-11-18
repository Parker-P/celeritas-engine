#pragma once
#include <filesystem>

namespace Settings
{
	/**
	 * @brief Contains all paths to files needed in the project.
	 */
	class Paths
	{
		/**
		 * @brief The root path of the project. May vary depending on C++ compiler and platform, but works for Visual Studio..
		 */
		static inline auto _rootPath = []() -> std::filesystem::path { return std::filesystem::current_path(); };

	public:

		// I.N. Always use backward slashes for Windows, otherwise file reading may fail because of inconsistent path separators.
		// i.e. don't use C:\path/to\file, always use C:\path\to\file.

		/**
		 * @brief Function returning the path to the global settings json file.
		 */
		static inline auto _settings = []() -> std::filesystem::path { return _rootPath() /= L"src\\settings\\GlobalSettings.json"; };
		
		/**
		 * @brief Function returning the path to the compiled vertex shader file.
		 */
		static inline auto _vertexShaderPath = []() -> std::filesystem::path { return _rootPath() /= L"src\\engine\\shaders\\VertexShader.spv"; };
		
		/**
		 * @brief Function returning the path to the compiled fragment shader file.
		 */
		static inline auto _fragmentShaderPath = []() -> std::filesystem::path { return _rootPath() /= L"src\\engine\\shaders\\FragmentShader.spv"; };
	};
}

