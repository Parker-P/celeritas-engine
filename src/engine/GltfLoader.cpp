#include <filesystem>
#include <fstream>
#include <iostream>

#include "GltfLoader.hpp"

GltfData GltfLoader::Load(std::filesystem::path filename)
{
	GltfData data;
	std::fstream file(filename, std::ios::binary | std::ios::in);
	if (file.is_open()) {
		std::cout << "File was opened" << std::endl;
	}
	return data;
}
