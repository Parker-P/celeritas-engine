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

GltfData GltfLoader::Load(std::filesystem::path filename) {
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
		json::jobject rootObj = json::jobject::parse(json.value);
		
		// To get the vertex positions you need to:
		// 1) access the "meshes" array and read the "attributes" array inside 
		//    of the mesh you want to load (a gltf file could contain multiple 
		//    meshes as it describes a scene)
		// 2) find the "POSITIONS" attribute inside of it and get its value.
		//    this value will be the value used in the next step
		// 3) access the "accessors" array and use the value you got in step 2
		//    as index. accessors[index] will contain the information needed
		//    to interpret the data you will read. You will need all the fields
		//    inside so save them all.
		// 4) With the data you got in step 3, access the "bufferViews" array
		//	  using the "bufferView" field. bufferViews[bufferView] will contain 
		//    info about where the vertex positions start ("dataOffset" field) and
		//    how big the info is in bytes so you know where to start and how many 
		//    bytes to read.
		//    
		// This approach holds in general for other info as well

		auto meshes = json::jobject::parse(rootObj.get("meshes"));
		auto primitives = meshes.array(0).get("primitives");
		auto attributes = primitives.array(0).get("attributes");
		auto POSITION = (int)attributes.get("POSITION");
		auto NORMAL = (int)attributes.get("NORMAL");
		auto TEXCOORD_0 = (int)attributes.get("TEXCOORD_0");
		auto indices = (int)primitives.array(0).get("indices");
		//auto POSITION = attributes.get("POSITION");
		/*auto attributes = primitives["attributes"];
		auto POSITION = attributes.get("POSITION");*/
		std::cout << std::endl;
	}
	return GltfData();
}
