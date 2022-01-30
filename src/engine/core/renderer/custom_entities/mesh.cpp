#include <vector>
#include <vulkan/vulkan.h>

#include "mesh.h"

namespace Engine::Core::Renderer::CustomEntities {
	void Mesh::GenerateVertexDescriptions()
	{
		//This tells the GPU how to read vertex data
		vertex_binding_description_.binding = 0;
		vertex_binding_description_.stride = sizeof(vertices_[0]);
		vertex_binding_description_.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		//This tells the GPU how to connect shader variables and vertex data. Each element of the array defines what attribute it is. For example if for each vertex you have position and normal you can use vertex_attribute_descriptions[0] for positions and vertex_attribute_descriptions[1] for normals.
		vertex_attribute_descriptions_.resize(1);
		vertex_attribute_descriptions_[0].binding = 0;
		vertex_attribute_descriptions_[0].location = 0;
		vertex_attribute_descriptions_[0].format = VK_FORMAT_R32G32B32_SFLOAT;
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