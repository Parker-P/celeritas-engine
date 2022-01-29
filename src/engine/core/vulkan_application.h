#pragma once

//For a high level understaning of Vulkan and how it interacts with the GPU go to: https://vkguide.dev/
//For all the in depth technical information about the Vulkan API, go to:
//https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html
//Styleguide used for this project: https://google.github.io/styleguide/cppguide.html

//This project follows the convention of namespace structure = folder structure. The root folder of all source code is of course the "src"
//folder
namespace Engine::Core {

	//This is a class for a generic vulkan application. The idea is that this class handles all the interactions between vulkan entities.
	//All the creation is done in each specific entity to remove as much creation boilerplate code as possible from this class. 
	//For vulkan entities we mean the building blocks of a vulkan application such as the instance, logical device, physical device, swap chain, graphics pipeline etc...
	class VulkanApplication {

		//Private member variables
		bool window_resized_ = false; //Was the window resized?
		Renderer::VulkanEntities::Instance instance_;
		Renderer::VulkanEntities::PhysicalDevice physical_device_;
		Renderer::VulkanEntities::WindowSurface window_surface_;
		Renderer::VulkanEntities::LogicalDevice logical_device_;
		Renderer::VulkanEntities::Semaphore image_available_semaphore_;
		Renderer::VulkanEntities::Semaphore rendering_finished_semaphore_;
		Renderer::VulkanEntities::CommandPool graphics_command_pool_;
		Renderer::VulkanEntities::SwapChain swap_chain_;
		Renderer::VulkanEntities::GraphicsPipeline graphics_pipeline_;
		Renderer::VulkanEntities::DescriptorPool descriptor_pool_;

		//3D Model related stuff
		VkBuffer vertex_buffer_; //The CPU side vertex information of an object
		VkBuffer index_buffer_; //The CPU side face information of an object. An index buffer contains integers that are the corresponding array indices in the vertex buffer. For example if we had 4 vertices and 2 triangles (a quad), the index buffer would look something like {0, 1, 2, 2, 1, 3} where the first and last 3 triplets represent a face. Each integer points to the vertex buffer so the GPU knows which vertex to choose from the vertex buffer
		VkDeviceMemory vertex_buffer_memory_; //The GPU side memory allocated for vertex info of an object. This object is filled using the vertex_buffer_ variable
		VkDeviceMemory index_buffer_memory_; //The GPU side memory allocated face info of an object. This object is filled using the index_buffer_ variable
		VkVertexInputBindingDescription vertex_binding_description_; //This variable is used to tell Vulkan (thus the GPU) how to read the vertex buffer.
		std::vector<VkVertexInputAttributeDescription> vertex_attribute_descriptions_; //This variable is used to tell vulkan what vertex information we have in our vertex buffer (if it's just vertex positions or also uv coordinates, vertex colors and so on)
		//The data to be passed in the uniform buffer. A uniform buffer is just a regular buffer containing the variables we want to pass to our shaders. They are called uniform buffers because it comes from OpenGL where variables we wanted to pass to the shaders were called uniforms, but in Vulkan terminology it's more accurate to call them descriptor buffers because they are bound to descriptor sets
		struct {
			glm::mat4 model_matrix; //The model matrix. This transformation matrix is responsible for translating the vertices of a model to the correct world space coordinates
			glm::mat4 view_matrix; //The camera matrix. This transformation matrix is responsible for translating the vertices of a model so that it looks like we are viewing it from a world space camera. In reality there is no camera, it's all the models moving around the logical space
			glm::mat4 projection_matrix; //The projection matrix. This transformation matrix is responsible for taking the 3D space coordinates and generating 2D coordinates on our screen. This matrix makes sure that perspective is taken into account when the shaders calculate vertex positions on the screen. This will make objects that are further away appear smaller
		} uniform_buffer_data_;
		
		std::vector<VkCommandBuffer> graphics_command_buffers_; //A command buffer contains pre recorded Vulkan commands. These commands are recorded in this object then put onto a logical device queue so that Vulkan will then tell the GPU to execute them in order
		std::chrono::high_resolution_clock::time_point time_start_; //A simple time variable to know when the app was started

		//Private member functions
		void WindowInit();
		void SetupVulkan();
		void MainLoop();
		static void OnWindowResized(GLFWwindow* window, int width, int height);
		void OnWindowSizeChanged();
		void Cleanup(bool fullClean);

		void CreateVertexAndIndexBuffers();
		void CreateUniformBuffer();
		void UpdateUniformData();
		VkBool32 GetMemoryType(uint32_t typeBits, VkFlags properties, uint32_t* typeIndex);
		VkShaderModule CreateShaderModule(const std::string& filename);

		void CreateDescriptorPool();
		void CreateDescriptorSets();
		void CreateCommandBuffers();
		void Draw();

	public:
		//Public member variables
		unsigned int id_; //The id of the vulkan application
		const char* name_; //The name of the application
		GLFWwindow* window_; //The window the application is running in
		Engine::Core::AppConfig app_config_; //The configuration information for the application

		//Public member functions
		VulkanApplication(Engine::Core::AppConfig app_config);
		void Run();
	};
}
