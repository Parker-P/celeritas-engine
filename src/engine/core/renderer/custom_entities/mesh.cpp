#include <vector>
#include <vulkan/vulkan.h>

#include "mesh.h"

namespace Engine::Core::Renderer::CustomEntities {
	void Mesh::GenerateVertexDescriptions()
	{
		
	}

	std::vector<Vertex> Mesh::GetVertices() {
		return vertices_;
	}

	std::vector<uint32_t> Mesh::GetFaces() {
		return faces_;
	}

	void Mesh::SetVertices(std::vector<Vertex> vertices) {
		vertices_ = vertices;
	}

	void Mesh::SetFaces(std::vector<uint32_t> faces) {
		faces_ = faces;
	}
}