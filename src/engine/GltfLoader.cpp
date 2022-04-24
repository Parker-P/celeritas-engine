#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

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

enum class GltfDataType { NONE, VEC3, VEC2, SCALAR };

class GltfMesh {
public:
	int index; // The index of the mesh in the "meshes" array in the GLTF file
	std::string name;

	// This struct describes where to find information about this mesh inside the GltfScene this mesh is in
	struct Primitive {
		struct {
			int positionsAccessorIndex;
			int normalsAccessorIndex;
			int uvCoordsAccessorIndex;
		} attributes;
		int indicesAccessorIndex;
	};

	std::vector<Primitive> primitives; // Describes the raw meshes that make up the mesh. For example this mesh could be made up of 2 separate cubes
};

class GltfScene {
public:

	struct Accessor {
		int bufferView;
		int	componentType;
		int	count;
		GltfDataType type;
	};

	struct BufferView {
		int byteLength;
		int	byteOffset;
	};

	std::vector<GltfMesh> meshes;			// Tells you which mehses are in the scene
	std::vector<Accessor> accessors;		// Tells you how to interpret mesh data
	std::vector<BufferView> bufferViews;	// Tells you where to find mesh data inside the raw gltf data buffer
};


GltfDataType GetDataType(std::string type) {
	if (type == "VEC3") { return GltfDataType::VEC3; }
	if (type == "VEC2") { return GltfDataType::VEC2; }
	if (type == "SCALAR") { return GltfDataType::SCALAR; }
	return GltfDataType::NONE;
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

		// Get meshes data
		GltfScene gltfScene;

		auto meshes = json::jobject::parse(rootObj.get("meshes"));
		auto meshCount = meshes.size();

		auto accessors = json::jobject::parse(rootObj.get("accessors"));
		auto accessorsCount = accessors.size();

		auto bufferViews = json::jobject::parse(rootObj.get("bufferViews"));
		auto bufferViewsCount = bufferViews.size();

		// Parse each mesh, create a GltfMesh and add each mesh to the GltfScene
		for (int i = 0; i < meshCount; ++i) {

			GltfMesh gltfMesh;

			auto name = (std::string)meshes.array(i).get("name");
			gltfMesh.name = name;
			auto primitives = meshes.array(i).get("primitives");
			auto primitivesCount = json::jobject::parse(primitives).size();

			// Get primitives data
			for (int j = 0; j < primitivesCount; ++j) {
				auto attributes = primitives.array(j).get("attributes");
				auto POSITION = (int)attributes.get("POSITION");
				auto NORMAL = (int)attributes.get("NORMAL");
				auto TEXCOORD_0 = (int)attributes.get("TEXCOORD_0");
				auto faceIndices = (int)primitives.array(j).get("indices");
				gltfMesh.primitives.push_back({ {POSITION, NORMAL, TEXCOORD_0}, faceIndices });
				gltfScene.meshes.push_back(gltfMesh);
			}
		}

		// Get accessors data
		for (int j = 0; j < accessorsCount; ++j) {
			auto accessorBufferViewIndex = (int)accessors.array(j).get("bufferView");
			auto accessorComponentType = (int)accessors.array(j).get("componentType");
			auto accessorCount = (int)accessors.array(j).get("count");
			auto accessorType = (std::string)accessors.array(j).get("type");
			gltfScene.accessors.push_back({ accessorBufferViewIndex, accessorComponentType, accessorCount, GetDataType(accessorType) });
		}

		// Get bufferviews data
		for (int j = 0; j < bufferViewsCount; ++j) {

			auto bufferViewLength = (int)bufferViews.array(j).get("byteLength");
			auto bufferViewStart = (int)bufferViews.array(j).get("byteOffset");
			gltfScene.bufferViews.push_back({ bufferViewLength, bufferViewStart });
		}

		//auto POSITION = attributes.get("POSITION");
		/*auto attributes = primitives["attributes"];
		auto POSITION = attributes.get("POSITION");*/
		std::cout << std::endl;
	}
	return GltfData();
}
