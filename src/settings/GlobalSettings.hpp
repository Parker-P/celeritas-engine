#pragma once
namespace Settings
{
	class GlobalSettings : public Singleton<GlobalSettings>
	{
	public:

		/**
		 * @brief Flag for enabling or disabling validation layers when the Vulkan instance is created.
		 */
		bool _enableValidationLayers;

		/**
		 * @brief Instance validation layers to report problems with Vulkan usage.
		 */
		std::vector<const char*> _validationLayers;

		/**
		 * @brief Window width in pixels.
		 */
		uint32_t _windowWidth;

		/**
		 * @brief Window height in pixels.
		 */
		uint32_t _windowHeight;

		/**
		 * @brief Mouse sensitivity multiplier.
		 */
		float _mouseSensitivity;

		/**
		 * @brief Loads global settings from the json file.
		 * @param absolutePathToJson
		 */
		void Load(const std::filesystem::path& absolutePathToJson);
	};
}