#pragma once

namespace Engine::Scenes
{
	/**
	 * @brief Represents a scene-level PBR material.
	 */
	class Material
	{

	public:

		/**
		 * @brief Represents a texture for a PBR material.
		 */
		class Texture
		{

		public:

			/**
			 * @brief How the data is stored in memory. VK_FORMAT_R8G8B8A8_SRGB for example, means 8 bits for red, followed by 8 bits
			 * for green, then 8 bits for blue, and finally 8 bits for alpha.
			 */
			VkFormat _format;

			/**
			 * @brief The data for the pixels of the texture.
			 */
			std::vector<unsigned char> _data;

			/**
			 * @brief Width and height in pixels.
			 */
			VkExtent2D _sizePixels;

			/**
			 * @brief Default constructor.
			 */
			Texture() = default;

			/**
			 * @brief .
			 * @param format How the data is stored in memory. VK_FORMAT_R8G8B8A8_SRGB for example, means 8 bits for red, followed by 8 bits
			 * for green, then 8 bits for blue, and finally 8 bits for alpha.
			 * @param data The data for the pixels of the texture.
			 * @param sizePixels Width and height in pixels.
			 */
			Texture(VkFormat format, const std::vector<unsigned char>& data, const VkExtent2D& sizePixels);
		};

		/**
		 * @brief Base color texture data.
		 */
		Texture _baseColor;

		/**
		 * @brief Identifier for debug purposes.
		 */
		std::string _name;
	};
}