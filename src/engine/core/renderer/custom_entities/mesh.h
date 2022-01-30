#pragma once

namespace Engine::Core::Renderer::CustomEntities {

	struct Vertex {
		float position[3]; //Position of the vertices in viewport coordinates. Vulkan's normalized viewport coordinate system is very weird: +Y points down, +X points to the right, +Z points towards you. The origin is at the exact center of the viewport.
	};
	
	class Mesh {

		std::vector<VkVertexInputAttributeDescription> vertex_attribute_descriptions_; //This variable is used to tell vulkan what vertex information we have in our vertex buffer (if it's just vertex positions or also uv coordinates, vertex colors and so on)
		VkVertexInputBindingDescription vertex_binding_description_; //This variable is used to tell Vulkan how to read the vertex buffer.
		std::vector<Vertex> vertices_;
		std::vector<uint32_t> faces_;
	
	public:

		//Generates vertex descriptions. The info generated will then be passed to Vulkan so it can tell the shaders how to read vertices and its attributes
		void GenerateVertexDescriptions();
		std::vector<Vertex> GetVertices();
		std::vector<uint32_t> GetFaces();
		void SetVertices(std::vector<Vertex> vertices);
		void SetFaces(std::vector<uint32_t> faces);
	};
}