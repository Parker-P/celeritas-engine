#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "GltfLoader.hpp"
#include "Json.h"


//uint32_t GltfLoader::GetIntValue(const char* string)
//{
//	for(size_t i=0; )
//	return uint32_t();
//}

/// <summary>
/// Converts an array of characters to an unsigned 32-bit integer
/// </summary>
/// <returns>The converted number</returns>
uint32_t ToUInt32(const char* charArray) {
	uint32_t result = 0;
	for (int i = 0; i < sizeof(uint32_t); ++i) {
		result |= (int)charArray[i] << (i * 8);
	}
	return result;
}

GltfData GltfLoader::Load(std::filesystem::path filename)
{
	//GltfData data = ;
	std::fstream file(filename, std::ios::binary | std::ios::in);
	if (file.is_open()) {
		std::cout << "Reading file " << filename << "\n";
		GltfData data{};

		// Header
		unsigned int intSize = sizeof(uint32_t);
		char* intBuffer = new char[intSize];
		file.read(intBuffer, intSize);
		data.header.magic = ToUInt32(intBuffer);
		file.read(intBuffer, intSize);
		data.header.version = ToUInt32(intBuffer);
		file.read(intBuffer, intSize);
		data.header.fileLength = ToUInt32(intBuffer);

		// JSON
		file.read(intBuffer, intSize);
		data.json.chunkLength = ToUInt32(intBuffer);
		file.read(intBuffer, intSize);
		data.json.chunkType = ToUInt32(intBuffer);
		data.json.data = new char[data.json.chunkLength];
		file.read(data.json.data, data.json.chunkLength);
		
		// Binary
		file.read(intBuffer, intSize);
		data.binaryBuffer.chunkLength = ToUInt32(intBuffer);
		file.read(intBuffer, intSize);
		data.binaryBuffer.chunkType = ToUInt32(intBuffer);
		data.binaryBuffer.data = new char[data.binaryBuffer.chunkLength];
		file.read(data.binaryBuffer.data, data.binaryBuffer.chunkLength);

		// Parse the JSON data
		json::parsing::parse_results json = json::parsing::parse(data.json.data);
		json::jobject obj = json::jobject::parse(json.value);
		std::cout << std::endl;
	}
	return GltfData();
}
