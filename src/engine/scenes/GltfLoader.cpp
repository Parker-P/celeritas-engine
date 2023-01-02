#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_vec.hpp>

#include "utils/Json.h"
#include "engine/math/Transform.hpp"
#include "engine/scenes/Mesh.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Scene.hpp"
#include "utils/Utils.hpp"
#include "engine/scenes/GltfLoader.hpp"

namespace Engine::Scenes
{

#pragma region LocalUtilityFunctions
	/**
	 * @brief Converts an array of characters to an unsigned 32-bit integer.
	 * @param charArray
	 * @return
	 */
	uint32_t ToUInt32(const char* charArray)
	{
		uint32_t result = 0;
		for (int i = 0; i < sizeof(uint32_t); ++i) {
			int x = (unsigned char)charArray[i];
			result |= (unsigned char)charArray[i] << (i * 8);
		}
		return result;
	}

	GltfDataType GetDataType(std::string type)
	{
		if (type == "SCALAR") { return GltfDataType::SCALAR; }
		if (type == "VEC2") { return GltfDataType::VEC2; }
		if (type == "VEC3") { return GltfDataType::VEC3; }
		if (type == "VEC4") { return GltfDataType::VEC4; }
		if (type == "MAT2") { return GltfDataType::MAT2; }
		if (type == "MAT3") { return GltfDataType::MAT3; }
		if (type == "MAT4") { return GltfDataType::MAT4; }
		return GltfDataType::NONE;
	}

	/**
	 * @brief Returns the size of a gltf component type in bytes.
	 *
	 * @param type
	 * @return
	 */
	size_t GetComponentSize(GltfComponentType type)
	{
		if (type == GltfComponentType::FLOAT) { return 4; }
		if (type == GltfComponentType::SIGNED_BYTE) { return 1; }
		if (type == GltfComponentType::SIGNED_SHORT) { return 2; }
		if (type == GltfComponentType::UNSIGNED_BYTE) { return 1; }
		if (type == GltfComponentType::UNSIGNED_INT) { return 4; }
		if (type == GltfComponentType::UNSIGNED_SHORT) { return 2; }
		return 0;
	}
#pragma endregion

#pragma region GltfLoaderFunctionImplementations
	Scene GltfLoader::LoadScene(std::filesystem::path filename)
	{
		std::fstream file(filename, std::ios::binary | std::ios::in);
		if (file.is_open()) {
			std::cout << "Loading scene " << filename << "\n";
			GltfData gltfData{};

			// Header
			unsigned int intSize = sizeof(uint32_t);
			char* intBuffer = new char[intSize];
			file.read(intBuffer, intSize);
			gltfData.header.magic = ToUInt32(intBuffer);
			file.read(intBuffer, intSize);
			gltfData.header.version = ToUInt32(intBuffer);
			file.read(intBuffer, intSize);
			gltfData.header.fileLength = ToUInt32(intBuffer);

			// JSON
			auto jsonChunkType = 0x4E4F534A; // JSON chunk type as defined in the gltf spec, which is just JSON as a char string, but converted to an integer by concatenating the bits of each character.
			auto isJson = false;
			while (!isJson) {
				file.read(intBuffer, intSize);
				gltfData.json.chunkLength = ToUInt32(intBuffer);
				file.read(intBuffer, intSize);
				gltfData.json.chunkType = ToUInt32(intBuffer);
				if (isJson = gltfData.json.chunkType == jsonChunkType) {
					gltfData.json.data = new char[gltfData.json.chunkLength];
					file.read(gltfData.json.data, gltfData.json.chunkLength);
				}
				else {
					file.seekg(4, std::ios::end);
				}
			}


			// Binary
			file.read(intBuffer, intSize);
			gltfData.binaryBuffer.chunkLength = ToUInt32(intBuffer);
			file.read(intBuffer, intSize);
			gltfData.binaryBuffer.chunkType = ToUInt32(intBuffer);
			gltfData.binaryBuffer.data = new char[gltfData.binaryBuffer.chunkLength];
			file.read(gltfData.binaryBuffer.data, gltfData.binaryBuffer.chunkLength);

			// Parse the JSON data
			json::parsing::parse_results json = json::parsing::parse(gltfData.json.data);
			json::jobject rootObj = json::jobject::parse(json.value);

			// To get the vertex positions you need to:
			// 1) access the "meshes" array and read the "attributes" array inside 
			//    of the mesh you want to load (a gltf file could contain multiple 
			//    meshes as it describes a scene).
			// 2) find the "POSITIONS" attribute inside of it and get its value.
			//    this value will be the value used in the next step.
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
					GltfMesh::Primitive primitive{};
					auto attributes = primitives.array(j).get("attributes");

					int POSITION = (int)attributes.get("POSITION");
					primitive.vertexAttributes.positionsAccessorIndex = POSITION;

					int NORMAL = (int)attributes.get("NORMAL");
					primitive.vertexAttributes.normalsAccessorIndex = NORMAL;

					int TEXCOORD_0 = (int)attributes.get("TEXCOORD_0");
					if (((json::jobject)attributes).has_key("TEXCOORD_0")) {
						TEXCOORD_0 = (int)attributes.get("TEXCOORD_0");
					}
					primitive.vertexAttributes.uvCoordsAccessorIndex = TEXCOORD_0;

					int faceIndices = (int)primitives.array(j).get("indices");
					primitive.indicesAccessorIndex = faceIndices;

					gltfMesh.primitives.push_back(primitive);
				}
				gltfScene.meshes.push_back(gltfMesh);
			}

			// Get accessors data
			for (int i = 0; i < accessorsCount; ++i) {
				auto accessorBufferViewIndex = (int)accessors.array(i).get("bufferView");
				auto accessorComponentType = (int)accessors.array(i).get("componentType");
				auto accessorCount = (int)accessors.array(i).get("count");
				auto accessorType = (std::string)accessors.array(i).get("type");
				gltfScene.accessors.push_back({ accessorBufferViewIndex, accessorComponentType, accessorCount, GetDataType(accessorType) });
			}

			// Get bufferviews data
			for (int i = 0; i < bufferViewsCount; ++i) {

				auto bufferViewLength = (int)bufferViews.array(i).get("byteLength");
				auto bufferViewStart = (int)bufferViews.array(i).get("byteOffset");
				gltfScene.bufferViews.push_back({ bufferViewLength, bufferViewStart });
			}

			// Create a local scene from the gltf data
			Scene scene;

			// For each mesh
			for (int i = 0; i < gltfScene.meshes.size(); ++i) {

				// For each primitive
				GameObject object;
				Mesh mesh;
				for (int j = 0; j < gltfScene.meshes[i].primitives.size(); ++j) {

					// Read positions, normals, uvs and face indices
					auto vertexPositionsAccessorIndex = gltfScene.meshes[i].primitives[j].vertexAttributes.positionsAccessorIndex;
					auto vertexNormalsAccessorIndex = gltfScene.meshes[i].primitives[j].vertexAttributes.normalsAccessorIndex;
					auto uvCoordsAccessorIndex = gltfScene.meshes[i].primitives[j].vertexAttributes.uvCoordsAccessorIndex;
					auto faceIndicesAccessorIndex = gltfScene.meshes[i].primitives[j].indicesAccessorIndex;

					auto vertexPositionsBufferViewIndex = gltfScene.accessors[vertexPositionsAccessorIndex].bufferViewIndex;
					auto vertexNormalsBufferViewIndex = gltfScene.accessors[vertexNormalsAccessorIndex].bufferViewIndex;
					auto uvCoordsBufferViewIndex = uvCoordsAccessorIndex >= 0 ? gltfScene.accessors[uvCoordsAccessorIndex].bufferViewIndex : -1;
					auto faceIndicesBufferViewIndex = gltfScene.accessors[faceIndicesAccessorIndex].bufferViewIndex;

					std::vector<glm::vec3> vertexPositions;
					std::vector<glm::vec3> vertexNormals;
					std::vector<glm::vec2> uvCoords;
					std::vector<unsigned int> faceIndices;

					if (Utils::AsInteger(GltfComponentType::FLOAT) == gltfScene.accessors[vertexPositionsAccessorIndex].componentType) {
						vertexPositions.resize(gltfScene.accessors[vertexPositionsAccessorIndex].count);
						memcpy(&vertexPositions[0], &gltfData.binaryBuffer.data[gltfScene.bufferViews[vertexPositionsBufferViewIndex].byteOffset], gltfScene.accessors[vertexPositionsAccessorIndex].count * sizeof(glm::vec3));
					}

					if (Utils::AsInteger(GltfComponentType::FLOAT) == gltfScene.accessors[vertexNormalsAccessorIndex].componentType) {
						vertexNormals.resize(gltfScene.accessors[vertexNormalsAccessorIndex].count);
						memcpy(&vertexNormals[0], &gltfData.binaryBuffer.data[gltfScene.bufferViews[vertexNormalsBufferViewIndex].byteOffset], gltfScene.accessors[vertexNormalsAccessorIndex].count * sizeof(glm::vec3));
					}

					if (uvCoordsBufferViewIndex >= 0) {
						if (Utils::AsInteger(GltfComponentType::FLOAT) == gltfScene.accessors[uvCoordsAccessorIndex].componentType) {
							uvCoords.resize(gltfScene.accessors[uvCoordsAccessorIndex].count);
							memcpy(&uvCoords[0], &gltfData.binaryBuffer.data[gltfScene.bufferViews[uvCoordsBufferViewIndex].byteOffset], gltfScene.accessors[uvCoordsBufferViewIndex].count * sizeof(glm::vec2));
						}
					}

					faceIndices.resize(gltfScene.accessors[faceIndicesAccessorIndex].count);
					auto indexSize = GetComponentSize(static_cast<GltfComponentType>(gltfScene.accessors[faceIndicesAccessorIndex].componentType));
					for (int i = 0; i < faceIndices.size(); ++i) {
						memcpy(&faceIndices[i], &gltfData.binaryBuffer.data[gltfScene.bufferViews[faceIndicesBufferViewIndex].byteOffset + (i * indexSize)], indexSize);
					}

					object._name = gltfScene.meshes[i].name;

					// This is where you will want to touch in the future to add multi-UV-map support.
					auto size = vertexPositions.size();
					if (vertexNormals.size() == size) {
						std::vector<Mesh::Vertex> vertices;
						vertices.reserve(vertexPositions.size());

						for (int i = 0; i < vertexPositions.size(); ++i) {

							// Transform vertex positions to engine-space.
							auto gltfSpaceVertexPosition = glm::vec4(vertexPositions[i], 1.0f);
							auto engineSpaceVertexPosition = Math::Transform::GltfToEngine()._matrix * gltfSpaceVertexPosition;

							// Transform normal vectors to engine-space.
							auto gltfSpaceVertexNormal = glm::vec4(vertexNormals[i], 1.0f);
							auto engineSpaceVertexNormal = Math::Transform::GltfToEngine()._matrix * gltfSpaceVertexNormal;
							vertices.emplace_back(Mesh::Vertex{ glm::vec3(engineSpaceVertexPosition), glm::vec3(engineSpaceVertexNormal), uvCoords[i] });
						}

						mesh._vertices = vertices;
						mesh._faceIndices = faceIndices;
						object._mesh = mesh;

						scene._objects.push_back(object);
					}
					else {
						Utils::Print("Size of vertex positions and vertex normals must the same");
					}
				}
			}
			return scene;
		}
		return Scene();
	}
#pragma endregion
}