#include <iostream>
#include <vulkan/vulkan.h>
#include <assimp/BaseImporter.h>
#include <assimp/Importer.hpp> 
#include <assimp/scene.h>
#include <assimp/postprocess.h>


#include "../src/engine/utils/patterns/singleton.h"
#include "../core/renderer/custom_entities/mesh.h"
#include "asset_importer.h"

using Mesh = Engine::Core::Renderer::CustomEntities::Mesh;
using Vertex = Engine::Core::Renderer::CustomEntities::Vertex;

namespace Engine::Core::Utils {

	//Gets the mesh information from "scene_to_fetch" and initializes "destination_mesh" with that data
	void FetchMeshInfo(const aiScene* scene_to_fetch, Mesh& destination_mesh) {

		auto vertices = destination_mesh.GetVertices();
		for (int mesh_index = 0; mesh_index < scene_to_fetch->mNumMeshes; ++mesh_index) {
			destination_mesh.GetVertices().reserve(scene_to_fetch->mMeshes[mesh_index]->mVertices->Length());
			for (int vert_index = 0; vert_index < scene_to_fetch->mMeshes[mesh_index]->mNumVertices; ++vert_index) {
				auto pos_x = scene_to_fetch->mMeshes[mesh_index]->mVertices[vert_index].x;
				auto pos_y = scene_to_fetch->mMeshes[mesh_index]->mVertices[vert_index].y;
				auto pos_z = scene_to_fetch->mMeshes[mesh_index]->mVertices[vert_index].z;
				Vertex vertex{ { pos_x, pos_y, pos_z } };
				vertices.emplace_back(vertex);
			}
		}

		auto faces = destination_mesh.GetFaces();
		for (int mesh_index = 0; mesh_index < scene_to_fetch->mNumMeshes; ++mesh_index) {
			faces.reserve(scene_to_fetch->mMeshes[mesh_index]->mNumFaces);
			for (int face_index = 0; face_index < scene_to_fetch->mMeshes[mesh_index]->mNumFaces; ++face_index) {
				for (int i = 0; i < scene_to_fetch->mMeshes[mesh_index]->mFaces[face_index].mNumIndices; ++i) {
					faces.emplace_back(static_cast<uint32_t>(scene_to_fetch->mMeshes[mesh_index]->mFaces[face_index].mIndices[i]));
				}
			}
		}

		destination_mesh.SetVertices(vertices);
		destination_mesh.SetFaces(faces);
	}

	Engine::Core::Renderer::CustomEntities::Mesh AssetImporter::ImportModel(std::string file_name) {

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(file_name,
			aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_SortByPType);

		if (nullptr == scene) {
			std::cout << importer.GetErrorString();
		}
		else {
			Mesh mesh;
			FetchMeshInfo(scene, mesh);
		}
	}
}