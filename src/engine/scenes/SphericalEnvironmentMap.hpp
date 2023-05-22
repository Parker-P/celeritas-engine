#pragma once

namespace Engine::Scenes
{
	/**
	 * @brief Represents an spherical environment map, used as an image-based light source
	 * in the shaders.
	 */
	class SphericalEnvironmentMap
	{
	public:

		/**
		 * @brief Width of the image used for the environment map.
		 */
		int _width;

		/**
		 * @brief Height of the image used for the environment map.
		 */
		int _height;

		/**
		 * @brief Colors of the pixels as RGB floats with range from 0.0f to 1.0f. The colors of the pixels are stored this way
		 * because this is the way GLSL handles pixel colors.
		 */
		std::vector<glm::vec3> _pixelColors;

		/**
		 * @brief World space coordinates of the pixels. By using this in unison with _pixelColors,
		 * the fragment shader is able to know which direction the light emitted from the image
		 * is coming from and what color it is.
		 */
		 //std::vector<float> _pixelCoordinatesWorldSpace;
		std::vector<glm::vec3> _pixelCoordinatesWorldSpace;

		/**
		 * @brief The buffer that encodes RGBA colors from the environment map.
		 */
		Vulkan::Buffer _environmentColorsBuffer;

		/**
		 * @brief The buffer that encodes world space spherical positions of the pixels of the environment map's image
		 * as if it was wrapped into a sphere.
		 */
		Vulkan::Buffer _environmentPositionsBuffer;

		/**
		 * @brief Buffer that contains the variable defined below.
		 */
		Vulkan::Buffer _entryCountBuffer;

		struct {
			
			/**
			 * @brief Count of the entries in the vectors put in the two buffers defined above.
			 */
			int _environmentDataEntryCount;

		} _envSize;

		/**
		 * @brief Loads an environment map from file, calculates.
		 */
		void LoadFromFile(std::filesystem::path imageFilePath);
	};
}
