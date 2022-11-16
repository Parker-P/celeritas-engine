#pragma once

// For a high level understaning of Vulkanand how it interacts with the GPU go to : https://vkguide.dev/
// For all the in depth technical information about the Vulkan API, go to:
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html
// Styleguide used for this project: https://google.github.io/styleguide/cppguide.html
// Original project was: https://gist.github.com/Overv/7ac07356037592a121225172d7d78f2d

namespace Engine::Vulkan
{
	/// <summary>
	/// Represents the Vulkan application.
	/// </summary>
	class VulkanApplication
	{
	public:

		void Run();

	private:

		// Window
		GLFWwindow* _window;

		// Basic vulkan stuff
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

		// Vertex and index buffers
		VkBuffer										_vertexBuffer;
		VkDeviceMemory									_vertexBufferMemory;
		VkBuffer										_indexBuffer;
		VkDeviceMemory									_indexBufferMemory;
		VkVertexInputBindingDescription					_vertexBindingDescription;
		std::vector<VkVertexInputAttributeDescription>	_vertexAttributeDescriptions;

		// Shader resources (descriptor sets and push constants)
		VkBuffer				_uniformBuffer;
		VkDeviceMemory			_uniformBufferMemory;
		VkDescriptorSetLayout	_descriptorSetLayout;
		VkDescriptorPool		_descriptorPool;
		VkDescriptorSet			_descriptorSet;
		VkPipelineLayout		_pipelineLayout;
		struct
		{
			glm::mat4 transformationMatrix;
		}						_uniformBufferData;

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
		glm::mat4 _modelMatrix;

		/// <summary>
		/// Initializes the window.
		/// </summary>
		void InitializeWindow();

		/// <summary>
		/// Calculates the time elapsed since rendering the last frame.
		/// </summary>
		void CalculateDeltaTime();

		/// <summary>
		/// Initializes the engine.
		/// </summary>
		void SetupVulkan();

		void MainLoop();

		static void OnWindowResized(GLFWwindow* window, int width, int height);

		void OnWindowSizeChanged();

		void Cleanup(bool fullClean);

		bool CheckValidationLayerSupport();

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

		/// <summary>
		/// Update the data that will be sent to the shaders.
		/// </summary>
		void UpdateUniformData();

		/// <summary>
		/// Find device memory that is supported by the requirements (typeBits) and meets the desired properties.
		/// </summary>
		VkBool32 GetMemoryType(uint32_t typeBits, VkFlags properties, uint32_t* typeIndex);

		/// <summary>
		/// Creates the swapchain. The swapchain is essentially an image manager. 
		/// The general purpose of the swap chain is to synchronize the presentation of images with the refresh rate of the screen.
		/// </summary>
		void CreateSwapChain();

		VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

		VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes);

		void CreateRenderPass();

		void CreateImageViews();

		void CreateFramebuffers();

		VkShaderModule CreateShaderModule(const std::filesystem::path& absolutePath);

		void CreateGraphicsPipeline();

		void CreateDescriptorPool();

		void CreateDescriptorSets();

		void CreateCommandBuffers();

		void Draw();
	};
}
