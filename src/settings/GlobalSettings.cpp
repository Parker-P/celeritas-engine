#include <string>
#include <filesystem>

#include "utils/Json.h"
#include "structural/Singleton.hpp"
#include "utils/Utils.hpp"
#include "settings/GlobalSettings.hpp"

namespace Settings
{
	/// <summary>
	/// Trims the ends of a string by removing the first and last characters from it.
	/// </summary>
	/// <param name="quotedString"></param>
	/// <returns>A new string with the 2 ends trimmed by one character.</returns>
	std::string TrimEnds(std::string quotedString)
	{
		auto length = quotedString.length();
		quotedString = quotedString.substr(1, length - 2);
		return quotedString;
	}

	void GlobalSettings::Load(const std::filesystem::path& pathToJson)
	{
		// Read the json file and parse it.
		auto text = Utils::File::ReadAllText(pathToJson);

		json::parsing::parse_results json = json::parsing::parse(text.data());
		json::jobject rootObj = json::jobject::parse(json.value);

		// Get the values from the parsed result.
		auto evlJson = rootObj.get("EnableValidationLayers");
		_enableValidationLayers = Utils::Converter::Convert<std::string, bool>(TrimEnds(evlJson));

		auto validationLayers = json::jobject::parse(rootObj.get("ValidationLayers"));
		_validationLayers.reserve(validationLayers.size());

		for (int i = 0; i < validationLayers.size(); ++i) {
			std::string str = validationLayers.array(i).as_string();
			auto cpy = str.c_str();
			_validationLayers.emplace_back(cpy);
		}

		auto windowSize = json::jobject::parse(rootObj.get("WindowSize"));
		auto width = windowSize.get("Width");
		auto height = windowSize.get("Height");
		_windowWidth = Utils::Converter::Convert<std::string, int>(width);
		_windowHeight = Utils::Converter::Convert<std::string, int>(height);

		auto input = json::jobject::parse(rootObj.get("Input"));
		auto sens = input.get("MouseSensitivity");
		_mouseSensitivity = Utils::Converter::Convert<std::string, float>(sens);
	}
}
