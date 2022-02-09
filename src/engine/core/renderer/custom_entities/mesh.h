#pragma once

namespace Engine::Core::Renderer::CustomEntities {

	struct Vertex {
		float position[3]; //Position of the vertices in viewport coordinates. Vulkan's normalized viewport coordinate system is very weird: +Y points down, +X points to the right, +Z points towards you. The origin is at the exact center of the viewport.
	};

	class Mesh {
		std::vector<Vertex> vertices_;
		std::vector<uint32_t> faces_;
		
		VkBuffer vertex_buffer_; //The CPU side vertex information of an object
		VkBuffer index_buffer_; //The CPU side face information of an object. An index buffer contains integers that are the corresponding array indices in the vertex buffer. For example if we had 4 vertices and 2 triangles (a quad), the index buffer would look something like {0, 1, 2, 2, 1, 3} where the first and last 3 triplets represent a face. Each integer points to the vertex buffer so the GPU knows which vertex to choose from the vertex buffer
		VkDeviceMemory vertex_buffer_memory_; //The GPU side memory allocated for vertex info of an object. This object is filled using the vertex_buffer_ variable
		VkDeviceMemory index_buffer_memory_; //The GPU side memory allocated face info of an object. This object is filled using the index_buffer_ variable
		//The data to be passed in the uniform buffer. A uniform buffer is just a regular buffer containing the variables we want to pass to our shaders. They are called uniform buffers because it comes from OpenGL where variables we wanted to pass to the shaders were called uniforms, but in Vulkan terminology it's more accurate to call them descriptor buffers because they are bound to descriptor sets
		struct {
			glm::mat4 model_matrix; //The model matrix. This transformation matrix is responsible for translating the vertices of a model to the correct world space coordinates
			glm::mat4 view_matrix; //The camera matrix. This transformation matrix is responsible for translating the vertices of a model so that it looks like we are viewing it from a world space camera. In reality there is no camera, it's all the models moving around the logical space
			glm::mat4 projection_matrix; //The projection matrix. This transformation matrix is responsible for taking the 3D space coordinates and generating 2D coordinates on our screen. This matrix makes sure that perspective is taken into account when the shaders calculate vertex positions on the screen. This will make objects that are further away appear smaller
		} uniform_buffer_data_;
	public:
		std::vector<Vertex> GetVertices();
		std::vector<uint32_t> GetFaces();
		void SetVertices(std::vector<Vertex> vertices);
		void SetFaces(std::vector<uint32_t> faces);
	};
}