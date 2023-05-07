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
		 * @brief Colors of the pixels.
		 */
		std::vector<glm::vec4> _pixelColors;

		/**
		 * @brief World space coordinates of the pixels. By using this in unison with _pixelColors,
		 * the fragment shader is able to know which direction the light emitted from the image
		 * is coming from and what color it is.
		 */
		std::vector<glm::vec3> _pixelCoordinatesWorldSpace;

		/**
		 * @brief Structure used for sending data to the shaders.
		 */
		struct
		{
			glm::vec4* pixelColors;
			glm::vec3* pixelCoordinatesWorldSpace;
			int sizePixels;
		} _environmentMap;

		/**
		 * @brief Loads.
		 */
		bool LoadFromFile(std::filesystem::path imagePath);
	};
}
