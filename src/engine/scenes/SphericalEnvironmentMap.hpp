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
		 * @brief Colors of the pixels.
		 */
		std::vector<int> _pixelColors;

		/**
		 * @brief World space coordinates of the pixels. By using this in unison with _pixelColors,
		 * the fragment shader is able to know which direction the light emitted from the image
		 * is coming from and what color it is.
		 */
		//std::vector<float> _pixelCoordinatesWorldSpace;
		std::vector<glm::vec3> _pixelCoordinatesWorldSpace;

		/**
		 * @brief The RGBA image that encodes color data.
		 */
		Vulkan::Image _color;

		/**
		 * @brief The image that encodes world space spherical positions of the pixels of the environment map's image
		 * as if it was wrapped into a sphere.
		 */
		Vulkan::Buffer _positions;

		/**
		 * @brief Loads an environment map from file, calculates.
		 */
		void LoadFromFile(std::filesystem::path imageFilePath);
	};
}