#pragma once

namespace Engine::Core::Renderer::CustomEntities {

	struct Vertex {
		float position[3]; //Position of the vertices in viewport coordinates. Vulkan's normalized viewport coordinate system is very weird: +Y points down, +X points to the right, +Z points towards you. The origin is at the exact center of the viewport.
	};

	class Mesh {
		std::vector<Vertex> vertices_;
		std::vector<uint32_t> faces_;
	public:
		std::vector<Vertex> GetVertices();
		std::vector<uint32_t> GetFaces();
		void SetVertices(std::vector<Vertex> vertices);
		void SetFaces(std::vector<uint32_t> faces);
	};
}