#include <vector>
#include <string>
#include <glm/glm.hpp>

#include "camera.h"
#include "mesh.h"
#include "game_object.h"
#include "scene.h"

namespace Engine::Core::Renderer::CustomEntities {
	std::vector<GameObject> Scene::GetGameObjects() {
		return game_objects_;
	}

	void Scene::AddMesh(Engine::Core::Renderer::CustomEntities::Mesh mesh){
		game_objects_.push_back(mesh);
	}

	void Scene::AddCamera(Engine::Core::Renderer::CustomEntities::Camera camera){
		game_objects_.push_back(camera);
	}
	std::vector<Vertex> Scene::GetAllVertices()
	{
		for (int i = 0; i < game_objects_.size(); ++i) {
			if (nullptr != dynamic_cast<Mesh>(game_objects_[i])) {

			}
		}
	}
	std::vector<uint32_t> Scene::GetAllVertexIndices()
	{
		return std::vector<uint32_t>();
	}
}