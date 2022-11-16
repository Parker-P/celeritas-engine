#pragma once
namespace Settings
{
	class GlobalSettings : public Singleton<GlobalSettings>
	{
	public:
		bool _enableValidationLayers;
		std::vector<const char*> _validationLayers;
		uint32_t _windowWidth;
		uint32_t _windowHeight;
		float _mouseSensitivity;

		/// <summary>
		/// Loads global settings from the json file.
		/// </summary>
		void Load(const std::filesystem::path& pathToJson);
	};
}