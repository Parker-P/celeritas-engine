#pragma once
#include <filesystem>

namespace Settings
{
	/// <summary>
	/// Contains all paths to files needed in the project.
	/// </summary>
	class Paths
	{
		/// <summary>
		/// The root path of the project. May vary depending on C++ compiler and platform, but works for Visual Studio.
		/// </summary>
		static inline auto _rootPath = []() -> std::filesystem::path { return std::filesystem::current_path(); };

	public:

		// I.N. Always use backward slashes for Windows, otherwise file reading may file because of inconsistend path separators.
		// i.e. don't use C:\path/to\file, always use C:\path\to\file.

		/// <summary>
		/// Function returning the path to the global settings json file.
		/// </summary>
		static inline auto _settings = []() -> std::filesystem::path { return _rootPath() /= L"src\\settings\\GlobalSettings.json"; };
		
		/// <summary>
		/// Function returning the path to the vertex shader file.
		/// </summary>
		static inline auto _vertexShaderPath = []() -> std::filesystem::path { return _rootPath() /= L"src\\engine\\shaders\\VertexShader.vert"; };
		
		/// <summary>
		/// Function returning the path to the vertex fragment file.
		/// </summary>
		static inline auto _fragmentShaderPath = []() -> std::filesystem::path { return _rootPath() /= L"src\\engine\\shaders\\FragmentShader.frag"; };
	};
}

