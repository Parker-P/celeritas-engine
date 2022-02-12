#pragma once

/*
For a high level understaning of Vulkanand how it interacts with the GPU go to : https://vkguide.dev/
For all the in depth technical information about the Vulkan API, go to:
https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html
Styleguide used for this project: https://google.github.io/styleguide/cppguide.html
Original project was: https://gist.github.com/Overv/7ac07356037592a121225172d7d78f2d
*/

//This is a class for a generic vulkan application. The idea is that this class handles all the interactions between vulkan entities.
//All the creation is done in each specific entity to remove as much creation boilerplate code as possible from this class. 
//For vulkan entities we mean the building blocks of a vulkan application such as the instance, logical device, physical device, swap chain, graphics pipeline etc...
class VulkanApplication {

	//Private member variables
	using Vertex = Engine::Core::Renderer::CustomEntities::Vertex;
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
	Renderer::CustomEntities::Scene scene_;

	std::vector<VkCommandBuffer> graphics_command_buffers_; //A command buffer contains pre recorded Vulkan commands. These commands are recorded in this object then put onto a logical device queue so that Vulkan will then tell the GPU to execute them in order
	std::chrono::high_resolution_clock::time_point time_start_; //A simple time variable to know when the app was started

	//Private member functions
	void LoadScene();
	std::pair<std::vector<Vertex>, std::vector<uint32_t>> GetAllVerticesAndFacesInScene();
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
