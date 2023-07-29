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
		 * @brief Identifier for debug purposes.
		 */
		std::string _name;

		/**
		 * @brief Base color texture data.
		 */
		Vulkan::Image _albedo;

		/**
		 * @brief Roughness texture data.
		 */
		Vulkan::Image _roughness;

		/**
		 * @brief Metalness texture data.
		 */
		Vulkan::Image _metalness;
	};
}