#pragma once
#include <filesystem>

namespace Settings
{
	/**
	 * @brief Contains all paths to files needed in the project.
	 */
	class Paths
	{
	public:
		
		/**
		 * @brief Every executable is passed a "working directory" parameter when starting it. This function returns the
		 * current working directory supplied to the executable that is running the code in this function, if provided,
		 * otherwise returns the folder, on disk, where the executable resides.
		 */
		static inline auto CurrentWorkingDirectory = []() -> std::filesystem::path { return std::filesystem::current_path(); };

		// I.N. Always use backward slashes for Windows, otherwise file reading may fail because of inconsistent path separators.
		// i.e. don't use C:\path/to\file, always use C:\path\to\file.

		/**
		 * @brief Function returning the path to the global settings json file.
		 */
		static inline auto Settings = []() -> std::filesystem::path { return CurrentWorkingDirectory() /= L"src\\settings\\GlobalSettings.json"; };
		
		/**
		 * @brief Function returning the path to the compiled vertex shader file.
		 */
		static inline auto VertexShaderPath = []() -> std::filesystem::path { return CurrentWorkingDirectory() /= L"src\\engine\\shaders\\VertexShader.spv"; };
		
		/**
		 * @brief Function returning the path to the compiled fragment shader file.
		 */
		static inline auto FragmentShaderPath = []() -> std::filesystem::path { return CurrentWorkingDirectory() /= L"src\\engine\\shaders\\FragmentShader.spv"; };

		/**
		 * @brief Function returning the path to the textures folder.
		 */
		static inline auto TexturesPath = []() -> std::filesystem::path { return CurrentWorkingDirectory() /= L"textures"; };

		/**
		 * @brief Function returning the path to the models folder.
		 */
		static inline auto ModelsPath = []() -> std::filesystem::path { return CurrentWorkingDirectory() /= L"models"; };
	};
}

