#pragma once

// For a high level understaning of Vulkanand how it interacts with the GPU go to : https://vkguide.dev/
// For all the in depth technical information about the Vulkan API, go to:
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html
// Styleguide used for this project: https://google.github.io/styleguide/cppguide.html
// Original project was: https://gist.github.com/Overv/7ac07356037592a121225172d7d78f2d

namespace Engine::Vulkan
{
	/**
	 * @brief Buffers represent linear arrays of data which are used for 
	 * various purposes by binding them to a graphics or compute pipeline via descriptor
	 * sets or via certain commands, or by directly specifying them as parameters to certain commands.
	 */
	class Buffer
	{
		/**
		 * @brief Logical device used to perform Vulkan calls.
		 */
		VkDevice _logicalDevice;

		/**
		 * @brief Contains information about the allocated memory for the buffer which, depending on the memory type flags
		 * passed at construction, can be allocated in the GPU (VRAM) or in the RAM.
		 */
		VkDeviceMemory _memory;

		/**
		 * @brief The memory address of the data inside the allocated buffer. This pointer is only assigned to if the buffer 
		 * is marked as VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, otherwise it's nullptr, as it would likely be an address on VRAM
		 * which the CPU cannot access without passing through Vulkan calls.
		 */
		void* _dataAddress;

		/**
		 * @brief Size in bytes of the data the buffer contains.
		 */
		size_t _size;

		/**
		 * @brief See constructor description.
		 */
		VkMemoryPropertyFlagBits _properties;

		/**
		 * @brief Find the memory type index that identifies a portion of memory that Vulkan uses to store buffers that have the given
		 * properties.
		 * @param gpuMemoryProperties Must come from a call to vkGetPhysicalDeviceMemoryProperties.
		 * @param typeBits Must come from a call made to vkGetBufferMemoryRequirements.
		 * @param properties See constructor description.
		 * @return The memory type index that identifies a portion of memory that Vulkan uses to store buffers that have the given
		 * properties.
		 */
		int GetMemoryTypeIndex(VkPhysicalDeviceMemoryProperties& gpuMemoryProperties, uint32_t typeBits, VkMemoryPropertyFlagBits properties);

	public:

		/**
		 * @brief The handle used by Vulkan to identify this buffer.
		 */
		VkBuffer _handle;

		/**
		 * Constructs a buffer.
		 * @param logicalDevice Needed by Vulkan to have info about the GPU so it can make function calls to allocate memory for the buffer.
		 * @param gpuMemoryProperties Needed to know which types of memory are available on the GPU, so the buffer can be allocated on the
		 * correct heap (a.k.a portion of VRAM or RAM).
		 * @param usageFlags This tells Vulkan what's the buffer's intended use.
		 * @param properties This can be any of the following values:
		 * 1) VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: this means GPU memory, so VRAM. If this is not set, then regular RAM is assumed.
		 * 2) VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: this means that the CPU will be able to read and write from the allocated memory IF YOU CALL vkMapMemory() FIRST.
		 * 3) VK_MEMORY_PROPERTY_HOST_CACHED_BIT: this means that the memory will be cached so that when the CPU writes to this buffer, if the data is small enough to fit in its
		 * cache (which is much faster to access) it will do that instead. Only problem is that this way, if your GPU needs to access that data, it won't be able to unless it's
		 * also marked as HOST_COHERENT.
		 * 4) VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: this means that anything that the CPU writes to the buffer will be able to be read by the GPU as well, effectively
		 * granting the GPU access to the CPU's cache (if the buffer is also marked as HOST_CACHED). COHERENT stands for consistency across memories, so it basically means 
		 * that the CPU, GPU or any other device will see the same memory if trying to access the buffer. If you don't have this flag set, and you try to access the 
		 * buffer from the GPU while the buffer is marked HOST_CACHED, you may not be able to get the data or even worse, you may end up reading the wrong chunk of memory.
		 * @param data Pointer to the start address of the data you want to copy to the buffer.
		 * @param sizeInBytes Size in bytes of the data you want to copy to the buffer.
		 */
		Buffer(VkDevice& logicalDevice, VkPhysicalDeviceMemoryProperties& gpuMemoryProperties, VkBufferUsageFlagBits usageFlags, VkMemoryPropertyFlagBits properties, void* data, size_t sizeInBytes);

		/**
		 * @brief Generates a data structure that Vulkan calls descriptor, which it uses to bind the buffer to a descriptor set.
		 */
		VkDescriptorBufferInfo GenerateDescriptor();

		/**
		 * @brief Destroys the buffer and frees any memory allocated to it.
		 * 
		 */
		void Destroy();
	};

	/**
	 * @brief Represents the Vulkan application.
	 */
	class VulkanApplication
	{
	public:

		void Run();

	private:

		GLFWwindow* _window;

		VkInstance							_instance;
		VkSurfaceKHR						_windowSurface;
		VkPhysicalDevice					_physicalDevice;
		VkDevice							_logicalDevice;
		VkDebugReportCallbackEXT			_callback;
		VkQueue								_graphicsQueue;
		uint32_t							_graphicsQueueFamily;
		VkQueue								_presentQueue;
		uint32_t							_presentQueueFamily;
		VkPhysicalDeviceMemoryProperties	_deviceMemoryProperties;
		VkSemaphore							_imageAvailableSemaphore;
		VkSemaphore							_renderingFinishedSemaphore;
		VkRenderPass						_renderPass;
		VkPipeline							_graphicsPipeline;

		/**
		 * @brief Buffer that stores vertex attributes. A vertex attribute is a piece of data
		 * that decorates the vertex with more information, so that the vertex shader can
		 * do more work based on it. For example a vertex attribute could be a position or a normal vector.
		 * Based on the normal vector, the vertex shader can perform lighting calculations by computing
		 * the angle between the source of the light and the normal..
		 */
		VkBuffer _vertexBuffer;

		/**
		 * @brief .
		 */
		VkDeviceMemory _vertexBufferMemory;

		/**
		 * @brief Buffer that stores indices that point into the vertex buffer. This buffer is used by Vulkan when drawing
		 * using the vkCmdDrawIndexed command. This buffer gives Vulkan information about the order in which to draw
		 * vertices.
		 */
		VkBuffer _indexBuffer;

		/**
		 * @brief .
		 */
		VkDeviceMemory _indexBufferMemory;

		/**
		 * Since we are only using one vertex buffer, this variable contains:
		 * 1) binding: the binding number of the vertex buffer defined when calling vkCmdBindVertexBuffers;
		 * 2) stride: the offset in bytes between each set of vertex attributes in the vertex buffer identified by the binding number above;
		 * 3) inputRate: unknown (info hard to find on this)
		 *
		 * Useful information:
		 * Vertex shaders can define input variables, which receive vertex attribute data transferred from
		 * one or more VkBuffer(s) by drawing commands. Vertex attribute data can be anything, but it's usually
		 * things like its position, normal and uv coordinates. Vertex shader input variables are bound to vertex buffers
		 * via an indirect binding, where the vertex shader associates a vertex input attribute number with
		 * each variable: the "location" decorator. Vertex input attributes (location) are associated to vertex input
		 * bindings (binding) on a per-pipeline basis, and vertex input bindings (binding) are associated with specific buffers
		 * (VkBuffer) on a per-draw basis via the vkCmdBindVertexBuffers command. Vertex input attribute and vertex input
		 * binding descriptions also contain format information controlling how data is extracted from buffer
		 * memory and converted to the format expected by the vertex shader.
		 *
		 * In short:
		 * Each vertex buffer is identified by a binding number, defined when calling vkCmdBindVertexBuffers.
		 * Each attribute inside a vertex buffer is identified by a location number, defined when creating a pipeline in a VkVertexInputBindingDescription struct..
		 */
		VkVertexInputBindingDescription	_vertexBindingDescription;

		/**
		 * Each VkVertexInputAttributeDescription contains:
		 * 1) location: identifier for the vertex attribute; also defined in the vertex shader definition of the attribute;
		 * 2) binding: the binding number of the vertex buffer defined when calling vkCmdBindVertexBuffers;
		 * 3) format: the format of this attribute/variable, VkFormat;
		 * 4) offset: the offset of the attribute in bytes within the set of vertex attributes.
		 *
		 * Useful information:
		 * Vertex shaders can define input variables, which receive vertex attribute data transferred from
		 * one or more VkBuffer(s) by drawing commands. Vertex attribute data can be anything, but it's usually
		 * things like its position, normal and uv coordinates. Vertex shader input variables are bound to vertex buffers
		 * via an indirect binding, where the vertex shader associates a vertex input attribute number with
		 * each variable: the "location" decorator. Vertex input attributes (location) are associated to vertex input
		 * bindings (binding) on a per-pipeline basis, and vertex input bindings (binding) are associated with specific buffers
		 * (VkBuffer) on a per-draw basis via the vkCmdBindVertexBuffers command. Vertex input attribute and vertex input
		 * binding descriptions also contain format information controlling how data is extracted from buffer
		 * memory and converted to the format expected by the vertex shader.
		 *
		 * In short:
		 * Each vertex buffer is identified by a binding number, defined every time we draw something by calling vkCmdBindVertexBuffers.
		 * Each attribute inside a vertex buffer is identified by a location number, defined here. The location number is defined when creating a pipeline.
		 */
		std::vector<VkVertexInputAttributeDescription> _vertexAttributeDescriptions;

		/**
		 * @brief The Vulkan buffer that contains the data that will be sent to the shaders. This buffer will be pointed to by a descriptor, which will in turn
		 * be pointed to by a descriptor set. The descriptor set is the structure that actually ends up in the shaders.
		 */
		Buffer _uniformBuffer;

		/**
		 * @brief A descriptor set layout object is defined by an array of zero or more descriptor bindings. Each individual descriptor binding is specified
		 * by a descriptor type, a count (array size) of the number of descriptors in the binding, a set of shader stages that can access the binding, and
		 * (if using immutable samplers) an array of sampler descriptors.
		 */
		VkDescriptorSetLayout _descriptorSetLayout;

		/**
		 * @brief A descriptor pool maintains a pool of descriptors, from which descriptor sets are allocated. Descriptor pools are externally synchronized,
		 * meaning that the application must not allocate and/or free descriptor sets from the same pool in multiple threads simultaneously.
		 */
		VkDescriptorPool _descriptorPool;

		/**
		 * @brief A descriptor set has bindings to descriptors, and is used to cluster descriptors with similar properties.
		 */
		VkDescriptorSet	_descriptorSet;

		/**
		 * @brief Access to descriptor sets from a pipeline is accomplished through a pipeline layout. Zero or more descriptor set layouts and zero or more
		 * push constant ranges are combined to form a pipeline layout object describing the complete set of resources that can be accessed by a pipeline.
		 * The pipeline layout represents a sequence of descriptor sets with each having a specific layout. This sequence of layouts is used to determine
		 * the interface between shader stages and shader resources. Each pipeline is created using a pipeline layout.
		 */
		VkPipelineLayout _pipelineLayout;

		struct
		{
			glm::mat4 transformationMatrix;
		} _uniformBufferData;

		// Swap chain
		VkExtent2D					_swapChainExtent;
		VkFormat					_swapChainFormat;
		VkSwapchainKHR				_oldSwapChain;
		VkSwapchainKHR				_swapChain;
		std::vector<VkImage>		_swapChainImages;
		std::vector<VkImageView>	_swapChainImageViews;
		std::vector<VkFramebuffer>	_swapChainFramebuffers;

		// Vulkan commands
		VkCommandPool					_commandPool;
		std::vector<VkCommandBuffer>	_graphicsCommandBuffers;

		// Time
		std::chrono::high_resolution_clock::time_point	_timeStart;		// The time the app was started
		std::chrono::high_resolution_clock::time_point	_lastFrameTime; // Time last frame started
		double											_deltaTime;		// The time since last frame started in milliseconds

		// Game
		Input::KeyboardMouse& _input = Input::KeyboardMouse::Instance();
		Settings::GlobalSettings& _settings = Settings::GlobalSettings::Instance();
		Scenes::Scene _scene;
		Scenes::Camera _mainCamera;
		Scenes::GameObject _model;

		/*
		 * Function called by Vulkan's validation layers once an error has occourred.
		 *
		 * @param flags
		 * @param objType
		 * @param srcObject
		 * @param location
		 * @param msgCode
		 * @param pLayerPrefix
		 * @param pMsg
		 * @param pUserData
		 * @return
		 */
		static VkBool32 DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char* pLayerPrefix, const char* pMsg, void* pUserData);

		/**
		 * Initializes the window.
		 */
		void InitializeWindow();

		/**
		 * Calculates the time elapsed since rendering the last frame..
		 */
		void CalculateDeltaTime();

		/**
		 * @brief Initializes the engine.
		 */
		void SetupVulkan();

		/**
		 * @brief Main loop used for each frame update.
		 */
		void MainLoop();

		/**
		 * Callback for when the window is resized.
		 *
		 * @param window
		 * @param width
		 * @param height
		 */
		static void OnWindowResized(GLFWwindow* window, int width, int height);

		/**
		 * @brief .
		 */
		void OnWindowSizeChanged();

		void Cleanup(bool fullClean);

		/**
		 * @brief Queries the physical device to get available validation layers. Once obtained,
		 * each given validation layer will be searched for in the available validation layers.
		 * @param validationLayers The validation layers you want to check that the physical device supports.
		 * @return True if all validation layers are found in the available validation layers, false otherwise.
		 */
		bool ValidationLayersSupported(const std::vector<const char*>& validationLayers);

		void CreateInstance();

		void CreateWindowSurface();

		void FindPhysicalDevice();

		void CheckSwapChainSupport();

		void FindQueueFamilies();

		void CreateLogicalDevice();

		void CreateDebugCallback();

		void CreateSemaphores();

		void CreateCommandPool();

		void CreateUniformBuffer();

		void CreateVertexAndIndexBuffers();

		/**
		 * @brief Update the data that will be sent to the shaders.
		 */
		void UpdateUniformData();

		/**
		 * @brief Creates the swapchain.
		 * @see _swapChain
		 */
		void CreateSwapChain();

		VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

		VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes);

		void CreateRenderPass();

		void CreateImageViews();

		void CreateFramebuffers();

		VkShaderModule CreateShaderModule(const std::filesystem::path& absolutePath);

		void CreateGraphicsPipeline();

		/**
		 * @brief Creates a descriptor pool.
		 * @param descriptorCount The amount of descriptors you plan to allocate from the pool.
		 */
		void CreateDescriptorPool(const uint32_t& descriptorCount);

		/**
		 * @brief Creates a descriptor set, and fills it with the chosen number of descriptors (defined when creating the descriptor pool that will be used to allocate the descriptor set).
		 */
		void CreateDescriptorSet();

		/**
		 * @brief Creates a descriptor set layout.
		 * @param descriptorCount The amount of descriptors you plan.
		 */
		void CreateDescriptorSetLayout(const uint32_t& descriptorCount);

		/**
		 * @brief Creates a descriptor buffer. A descriptor buffer is the actual descriptor that will be pointed to by a descriptor set.
		 * A descriptor set points to a descriptor buffer, which points to a VkBuffer.
		 * @param buffer The buffer you want the descriptor to point to.
		 */
		VkDescriptorBufferInfo CreateDescriptorBuffer(const Buffer& buffer);

		void CreatePipelineLayout();

		void CreateCommandBuffers();

		void Draw();
	};
}
