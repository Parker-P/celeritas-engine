#pragma once

class VulkanApplication {
	//Private member variables
	const std::string kShaderPath_ = "\"C:\\Users\\Paolo Parker\\source\\repos\\Celeritas Engine\\src\\engine\\";
	const bool kEnableDebugging_ = false;
	const char* kDebugLayer_ = "VK_LAYER_LUNARG_standard_validation";
	bool window_resized_ = false;
	VkInstance instance_;
	VkSurfaceKHR window_surface_;
	VkPhysicalDevice physical_device_;
	VkDevice logical_device_;
	VkDebugReportCallbackEXT callback_;
	VkQueue graphics_queue_;
	VkQueue present_queue_;
	VkPhysicalDeviceMemoryProperties device_memory_properties_;
	VkSemaphore image_available_semaphore_;
	VkSemaphore rendering_finished_semaphore_;
	VkBuffer vertex_buffer_;
	VkBuffer index_buffer_;
	VkDeviceMemory vertex_buffer_memory_;
	VkDeviceMemory index_buffer_memory_;
	VkVertexInputBindingDescription vertex_binding_description_;
	std::vector<VkVertexInputAttributeDescription> vertex_attribute_descriptions_;
	struct {
		glm::mat4 transformation_matrix;
	} UniformBufferData;
	VkBuffer uniform_buffer_;
	VkDeviceMemory uniform_buffer_memory_;
	VkDescriptorSetLayout descriptor_set_layout_;
	VkDescriptorPool descriptor_pool_;
	VkDescriptorSet descriptor_set_;
	VkExtent2D swap_chain_extent_;
	VkFormat swap_chain_format_;
	VkSwapchainKHR old_swap_chain_;
	VkSwapchainKHR swap_chain_;
	std::vector<VkImage> swap_chain_images_;
	std::vector<VkImageView> swap_chain_image_views_;
	std::vector<VkFramebuffer> swap_chain_frame_buffers_;
	VkRenderPass render_pass_;
	VkPipeline graphics_pipeline_;
	VkPipelineLayout pipeline_layout_;
	VkCommandPool command_pool_;
	std::vector<VkCommandBuffer> graphics_command_buffers_;
	uint32_t graphics_queue_family_;
	uint32_t present_queue_family_;
	std::chrono::high_resolution_clock::time_point time_start_;

	//Private member functions
	void WindowInit();
	void SetupVulkan();
	void MainLoop();
	static void OnWindowResized(GLFWwindow* window, int width, int height);
	void OnWindowSizeChanged();
	void Cleanup(bool fullClean);
	void CreateInstance();
	void CreateWindowSurface();
	void FindPhysicalDevice();
	void CheckSwapChainSupport();
	void FindQueueFamilies();
	void CreateLogicalDevice();
	void CreateDebugCallback();
	void CreateSemaphores();
	void CreateCommandPool();
	void CreateVertexBuffer();
	void CreateUniformBuffer();
	void UpdateUniformData();
	VkBool32 GetMemoryType(uint32_t typeBits, VkFlags properties, uint32_t* typeIndex);
	void CreateSwapChain();
	VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
	VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes);
	void CreateRenderPass();
	void CreateImageViews();
	void CreateFramebuffers();
	VkShaderModule CreateShaderModule(const std::string& filename);
	void CreateGraphicsPipeline();
	void CreateDescriptorPool();
	void CreateDescriptorSet();
	void CreateCommandBuffers();
	void Draw();

public:
	//Public member variables
	unsigned int id_; //The id of the vulkan application
	uint32_t width_; //The width of the window
	uint32_t height_; //The height of the window
	const char* name_; //The name of the application
	GLFWwindow* window_; //The window the application is running in

	//Public member functions
	void Run();
};