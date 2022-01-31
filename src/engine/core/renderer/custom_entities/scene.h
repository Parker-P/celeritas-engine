#pragma once

namespace Engine::Core::Renderer::CustomEntities {
	class Scene {
		std::vector<GameObject> game_objects_;
	public:
		std::vector<GameObject> GetGameObjects();
		void AddMesh(Engine::Core::Renderer::CustomEntities::Mesh mesh);
		void AddCamera(Engine::Core::Renderer::CustomEntities::Camera camera);
	};
}