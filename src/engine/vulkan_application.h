#pragma once

//For a high level understaning of Vulkan and how it interacts with the GPU go to: https://vkguide.dev/
//For all the in depth technical information about the Vulkan API, go to:
//https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html
//Styleguide used for this project: https://google.github.io/styleguide/cppguide.html

//This is a class for a generic vulkan application
class VulkanApplication {
	//Private member variables
	const std::string kShaderPath_ = "C:\\Users\\Paolo Parker\\source\\repos\\Celeritas Engine\\src\\engine\\shaders"; //Path to the folder where the shader files to be compiled to SPIR-V are
	const bool kEnableDebugging_ = false; //Enable debugging?
	const char* kDebugLayer_ = "VK_LAYER_LUNARG_standard_validation"; //Debug layer constant
	bool window_resized_ = false; //Was the window resized?
	VkInstance instance_; //The instance is our gateway to the vulkan API. The instance is what allows us to use vulkan commands and is the root of the entire Vulkan application context
	VkPhysicalDevice physical_device_; //This is the handle to the actual physical graphics card. The physical device handles the queues and GPU-local memory (VRAM)
	VkDevice logical_device_; //The logical device is an interface that we use to communicate to the physical device
	VkDebugReportCallbackEXT callback_; //Extension callback used for debugging purposes with validation layers
	VkPhysicalDeviceMemoryProperties device_memory_properties_; //Stores information for us to do memory management. This information is acquired when creating the logical device
	VkSemaphore image_available_semaphore_; //Semaphore used to know when an image is available. Semaphores are a synchronization primitive that can be used to insert a dependency between queue operations or between a queue operation and the host (from the Vulkan spec)
	VkSemaphore rendering_finished_semaphore_; //Semaphore used to know when an image has finished rendering. Semaphores are a synchronization primitive that can be used to insert a dependency between queue operations or between a queue operation and the host (from the Vulkan spec)
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
		glm::mat4 projection_matrix; //The projection matrix. This transformation matrix is responsible for the 3D look of the models. This matrix makes sure that perspective is taken into account when the shaders calculate vertex positions on the screen. This will make objects that are further away appear smaller
	} uniform_buffer_data_; 
	VkBuffer uniform_buffer_; //This is the buffer that contains the uniform_buffer_data_ struct
	VkDeviceMemory uniform_buffer_memory_; //Provides memory allocation info to vulkan when creating the uniform buffer
	VkDescriptorSetLayout descriptor_set_layout_; //This is used to describe the layout of a descriptor set
	VkDescriptorPool descriptor_pool_; //This is used to allocate memory for the descriptor sets. The memory allocation done by the pool is handled by the Vulkan drivers
	VkDescriptorSet descriptor_set_; //A descriptor set is a collection of descriptors. A descriptor is a shader resource. Each descriptor contains a pointer to a buffer or image and a description of what that pointed-to data represents. We use sets of descriptors so that we can group descriptors by how they are used in the rendering process. Descriptors are the main way to pass variables to the GPU's shaders and that's why they are also called shader resources. Another way to pass data to shaders is by using push constants, but their use is more limited
	VkExtent2D swap_chain_extent_; //The size of the swapchain images
	VkFormat swap_chain_format_; //The format of the images in the swapchain's queue
	VkSwapchainKHR swap_chain_; //This is the object that handles retrieving and updating the images to be displayed. The swapchain decides when to swap the buffers and contains a queue of images to be drawn. The type has the KHR suffix because it is an extension, meaning that it's an optional object that contains pieces of code that enable you to do something that is not native to Vulkan. In this case this extension is provided by Khronos, which also created Vulkan in the first place
	VkSurfaceKHR window_surface_; //This is the object that acts as an interface between the glfw window (in our case) and the swapchain. The type has the KHR suffix because it is an extension, meaning that it's an optional object that contains pieces of code that enable you to do something that is not native to Vulkan. In this case this extension is provided by Khronos, which also created Vulkan in the first place
	std::vector<VkImage> swap_chain_images_; //The images in the swapchain's queue
	std::vector<VkImageView> swap_chain_image_views_; //The interfaces that allow us to know certain information about the images in the swapchain. This information allows us to know how to use the images, it's just metadata
	std::vector<VkFramebuffer> swap_chain_frame_buffers_; //The frame buffers used by the swapchain. A frame buffer is a connection between a render pass and an image.
	VkRenderPass render_pass_; //The object that holds information for generating an image
	VkPipeline graphics_pipeline_; //The graphics pipeline is the entire process of generating an image from the information we are given. It's the process of going fron vertex positions and face information to actual triangles drawn on screen. This object contains all the information needed to do that
	VkPipelineLayout pipeline_layout_;
	VkCommandPool command_pool_; //The command pool is a space of memory that is divided into equally sized blocks and is used to allocate memory for the command buffers. From the Vulkan spec: command pools are opaque objects that command buffer memory is allocated from, and which allow the implementation to amortize the cost of resource creation across multiple command buffers
	std::vector<VkCommandBuffer> graphics_command_buffers_; //A command buffer contains pre recorded Vulkan commands. These commands are recorded in this object then put onto a logical device queue so that Vulkan will then tell the GPU to execute them in order
	uint32_t graphics_queue_family_; //A queue family is a category of queue. Queues within a single family are considered compatible with one another, and work produced for a family of queues can be executed on any queue within that family
	uint32_t present_queue_family_; //A queue family is a category of queue. Queues within a single family are considered compatible with one another, and work produced for a family of queues can be executed on any queue within that family
	VkQueue graphics_queue_; //This is the actual queue to process graphics commands
	VkQueue present_queue_; //This is the actual queue to process present commands
	std::chrono::high_resolution_clock::time_point time_start_; //A simple time variable to know when the app was started

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
	void CreateVertexAndIndexBuffers();
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
	void CreateDescriptorSets();
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