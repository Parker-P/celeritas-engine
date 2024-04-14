#pragma once

// For a high level understaning of Vulkan and how it interacts with the GPU go to : https://vkguide.dev/
// For all the in depth technical information about the Vulkan API, go to:
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html
// Original project was: https://gist.github.com/Overv/7ac07356037592a121225172d7d78f2d
namespace Engine::Vulkan
{
	/**
	 * @brief Represents the Vulkan application.
	 */
	class VulkanApplication : public Structural::IVulkanUpdatable
	{
	public:

		/**
		 * @brief Runs the application.
		 */
		void Run();

	private:

		/**
		 * @brief Wrapper for the window shown on screen.
		 */
		GLFWwindow* _pWindow;

		/**
		 * @brief Root for all Vulkan functionality.
		 */
		VkInstance _instance;

		/**
		 * @brief Connects the Vulkan API to the windowing system, so that Vulkan knows how to interact with the window on the screen.
		 */
		VkSurfaceKHR _windowSurface;

		/**
		 * @brief Represents the physical GPU. This is mostly used for querying the GPU about its hardware properties so that we know
		 * how to handle memory.
		 */
		VkPhysicalDevice _physicalDevice;

		/**
		 * @brief Represents the GPU and its inner workings at the logical level.
		 */
		VkDevice _logicalDevice;

		/**
		 * @brief Function pointer called by Vulkan each time it wants to report an error.
		 * Error reporting is set by enabling validation layers.
		 */
		VkDebugReportCallbackEXT _callback;

		/**
		 * @brief Semaphore that will be used by Vulkan to signal when an image has finished
		 * rendering and is available in one of the framebuffers.
		 */
		VkSemaphore _imageAvailableSemaphore;

		/**
		 * @brief Same as imageAvailableSemaphore.
		 */
		VkSemaphore	_renderingFinishedSemaphore;

		/**
		 * @brief Encapsulates info for a render pass.
		 * A render pass represents an execution of an entire graphics pipeline to create an image.
		 * Render passes use what are called (in Vulkan gergo) attachments. Attachments are rendered images that contribute to rendering
		 * the final image that will go in the framebuffer. It is the renderpass's job to also do compositing, which
		 * is defining the logic according to which the attachments are merged to create the final image.
		 * See Swapchain to understand what framebuffers are.
		 */
		struct {
			/**
			 * @brief Identifier for Vulkan.
			 */
			VkRenderPass _handle;

			/**
			 * @brief Attachments used by the GPU to write color information to.
			 */
			std::vector<Image> _colorImages;

			/**
			 * @brief Attachment that stores per-pixel depth information for the hardwired depth testing stage.
			 * This makes sure that the pixels of each triangle are rendered or not, depending on which pixel
			 * is closer to the camera, which is the information stored in this image.
			 */
			Image _depthImage;

		} _renderPass;

		/**
		 * @brief Encapsulates info for a swapchain.
		 * The swapchain is an image manager; it manages everything that involves presenting images to the screen,
		 * or more precisely passing the contents of the framebuffers on the GPU down to the window.
		 */
		struct {

			/**
			 * @brief Identifier for Vulkan.
			 */
			VkSwapchainKHR _handle;

			/**
			 * @brief These are the buffers that contain the final rendered images shown on screen.
			 * A framebuffer is stored on a different portion of memory with respect to the depth and
			 * color attachments used by a render pass. The depth and color images CONTRIBUTE to generating
			 * an image for a framebuffer.
			 */
			std::vector<VkFramebuffer> _frameBuffers;

			/**
			 * @brief Dimensions in pixels of the framebuffers.
			 */
			VkExtent2D _framebufferSize;

			/**
			 * @brief Image format that the window surface expects when it has to send images from a framebuffer to a monitor.
			 */
			VkSurfaceFormatKHR _surfaceFormat;

			/**
			 * @brief Used by Vulkan to know where and how to direct the contents of the framebuffers to the window on the screen.
			 */
			VkSurfaceKHR _windowSurface;

			/**
			 * @brief Old swapchain handle used when the swapchain is recreated.
			 */
			VkSwapchainKHR _oldSwapchainHandle;

		} _swapchain;

		/**
		 * @brief The graphics pipeline represents, at the logical level, the entire process
		 * of inputting vertices, indices and textures into the GPU and getting a 2D image that represents the scene
		 * passed in out of it. In early GPUs, this process was hardwired into the graphics chip, but as technology improved and needs
		 * for better and more complex graphics increased, GPU producers have taken steps to make this a much more programmable
		 * and CPU-like process, so much so that technologies like CUDA (Compute Uniform Device Architecture) have come out.
		 *
		 * Nowadays the typical GPU consists of an array of clusters of microprocessors, where each micro
		 * processor is highly multithreaded, and its ALU and instruction set (thus its circuitry) optimized for operating on
		 * floating point numbers, vectors and matrices (as that is what is used to represent coordinates in space and space transformations).
		 *
		 * CUDA is the result of NVidia and Microsoft working together to improve the programmers' experience for programming
		 * shaders; the result is a product that maps C/C++ instructions to the GPU's ALUs instruction set, to take full advantage
		 * of the GPU's high amount of cores and threads. By doing this, things like crypto mining and machine learning have gotten
		 * a huge boost, as the data that requires manipulation is very loosely coupled (has very few dependecies on the data in
		 * the same set) and, as a result, can be processed by many independent threads simultaneously. This is called a compute pipeline.
		 * Khronos, the developers of the Vulkan API, have also done something similar called OpenCL, the compute side of OpenGL.
		 *
		 * The typical graphics (or render) pipeline consists of programmable, configurable and hardwired stages, where:
		 * a) The programmable stages are custom stages that will be run on the GPU's multi-purpose array of microprocessors using a program (a.k.a shader).
		 * b) The configurable stages are hardwired stages that can perform their task a different way based on user configuration via calls to the Vulkan API.
		 * c) The hardwired stages are immutable stages that cannot be changed unless manipulating the hardware.
		 * The graph of a typical graphics pipeline is shown under docs/GraphicsPipeline.jpg.
		 *
		 * More on the programmable stages:
		 * The programmable stages are the flexible stages that the programmer can fully customize by writing little programs called shaders.
		 * These shader programs will run:
		 * 1) once per vertex in the case of the vertex shader; this shader program's goal is to take vertex attrbutes in, and output
		 * a vertex color and 2D position (more precisely, a 3D position inside of the Vulkan's coordinate range, which is -1 to 1 for X and Y and 0 to 1 for Z).
		 * 2) once per pixel in the case of the fragment shader; this stage's goal is to take the rasterizer's output (which is a hardwired
		 * stage on the GPU chip as of 2022, whose goal is to color pixels based on vertex colors) and textures, and output a colored pixel based on
		 * the color of the textures and other variables such as direct and indirect lighting.
		 * There are other shader stages, but the 2 above are the strictly needed shaders in order to be able to render something.

		 * This type of execution flow has a name and is called SIMD (Single Instruction Multiple Data), as the same program (single instruction)
		 * is run independently on different cores/threads for multiple vertices or pixels (multiple data).
		 *
		 * Examples of the configurable stages are anti-aliasing and tessellation.
		 * Examples of hardwired stages are backface-culling, depth testing and alpha blending.
		 *
		 * For a good overall hardware and software explanation of a typical NVidia GPU, see
		 * https://developer.nvidia.com/gpugems/gpugems2/part-iv-general-purpose-computation-gpus-primer/chapter-30-geforce-6-series-gpu
		 */
		struct {

			/**
			 * @brief Identifier for Vulkan.
			 */
			VkPipeline _handle;

			/**
			 * This variable contains:
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
			 * (VkBuffer) on a per-draw basis via the vkCmdBindVertexBuffers command. Vertex input attributes and vertex input
			 * binding descriptions also contain format information controlling how data is extracted from buffer
			 * memory and converted to the format expected by the vertex shader.
			 *
			 * In short:
			 * Each vertex buffer is identified by a binding number, defined when calling vkCmdBindVertexBuffers.
			 * Each attribute inside a vertex buffer is identified by a location number, defined when creating a pipeline in a VkVertexInputBindingDescription struct.
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
			* @brief Access to descriptor sets from a pipeline is accomplished through a pipeline layout. Zero or more descriptor set layouts and zero or more
			* push constant ranges are combined to form a pipeline layout object describing the complete set of resources that can be accessed by a pipeline.
			* The pipeline layout represents a sequence of descriptor sets with each having a specific layout. This sequence of layouts is used to determine
			* the interface between shader stages and shader resources. Each pipeline is created using a pipeline layout.
			*/
			VkPipelineLayout _layout;

			/**
			 * @brief See ShaderResources definition.
			 */
			ShaderResources _shaderResources;

		} _graphicsPipeline;

		// Vulkan commands.
		VkCommandPool _commandPool;
		VkQueue _queue;
		int _queueFamilyIndex;

		VkCommandBuffer _graphicsCommandBuffer;
		std::vector<VkCommandBuffer> _drawCommandBuffers;

		// Game.
		Input::KeyboardMouse& _input = Input::KeyboardMouse::Instance();
		Settings::GlobalSettings& _settings = Settings::GlobalSettings::Instance();
		Scenes::Scene _scene;
		Scenes::Camera _mainCamera;

		/*
		 * @brief Function called by Vulkan's validation layers once an error has occourred.
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
		 * @brief Initializes the engine.
		 */
		void SetupVulkan();

		/**
		 * @brief Main loop used for each frame update.
		 */
		void MainLoop();

		/**
		 * @brief See IUpdatable.
		 */
		virtual void Update(VulkanContext& vkContext) override;

		/**
		 * @brief Physics simulation root call, performing updates on all objects that extend IBody.
		 */
		virtual void PhysicsUpdate();

		/**
		 * GLFW callback for when the window is resized.
		 * @param window
		 * @param width
		 * @param height
		 */
		static void OnWindowResized(GLFWwindow* pWindow, int width, int height);

		/**
		 * @brief Function called whenever the window is resized.
		 */
		void WindowSizeChanged();

		/**
		 * @brief Removes any attachments and the render pass object itself from memory.
		 */
		void DestroyRenderPass();

		/**
		 * @brief Removes the framebuffers and the swapchain object itself from memory.
		 */
		void DestroySwapchain();

		/**
		 * @brief Destroys all Vulkan objects by using Vulkan calls.
		 */
		void Cleanup(bool fullClean);

		/**
		 * @brief Queries the physical device to get available validation layers. Once obtained,
		 * each given validation layer will be searched for in the available validation layers.
		 * @param validationLayers The validation layers you want to check that the physical device supports.
		 * @return True if all validation layers are found in the available validation layers, false otherwise.
		 */
		bool ValidationLayersSupported(const std::vector<const char*>& pValidationLayers);

		/**
		 * @brief Creates the Vulkan instance that is the root container for all the Vulkan components that will be created.
		 */
		VkInstance CreateInstance();

		/**
		 * @brief The window surface is a handle that Vulkan uses to know to which window its framebuffers will be shown.
		 */
		VkSurfaceKHR CreateWindowSurface(VkInstance& instance);

		VkPhysicalDevice CreatePhysicalDevice(VkInstance instance);

		VkDevice CreateLogicalDevice(VkPhysicalDevice& physicalDevice, const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos);

		void CreateDebugCallback(VkInstance& instance);

		void CreateSemaphores();

		void LoadScene();

		void LoadEnvironmentMap();

		VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes);

		VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

		VkExtent2D ChooseFramebufferSize(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

		/**
		 * @brief Creates one framebuffer for each color attachment.
		 * @param logicalDevice
		 * @param renderPass Used to tell each framebuffer which attachments are used to generate the image it'll contain.
		 */
		void CreateFramebuffers();

		/**
		 * @brief Creates the render pass.
		 */
		void CreateRenderPass();

		/**
		 * @brief Creates the swapchain.
		 */
		void CreateSwapchain();

		/**
		 * @brief Compiles the given file and creates a Vulkan structure that describes a shader program
		 * with the compiled code.
		 */
		VkShaderModule CreateShaderModule(const std::filesystem::path& absolutePath);

		/**
		 * @brief Creates the API level graphics (or render) pipeline.
		 * @see _graphicsPipeline
		 */
		void CreateGraphicsPipeline();

		/**
		 * @brief Creates the layouts of all descriptor sets used in the shaders.
		 */
		std::vector<Vulkan::DescriptorSetLayout> CreateDescriptorSetLayouts();

		/**
		 * @brief Creates the pipeline layout. See _graphicsPipeline._layout.
		 */
		VkPipelineLayout CreatePipelineLayout(std::vector<DescriptorSetLayout>& descriptorSetLayouts);

		/**
		 * @brief Creates shader resources.
		 */
		void CreateShaderResources(std::vector<DescriptorSetLayout>& descriptorSetLayouts);

		/**
		 * @brief Creates one command buffer for each image in the swapchain (amount depends on present mode).
		 * Each command buffer will be submitted to the queue (see _queue).
		 */
		void AllocateDrawCommandBuffers();

		/**
		 * @brief For each swapchain image, records draw commands into the corresponding draw command buffer.
		 */
		void RecordDrawCommands();

		/**
		 * @brief For each swapchain image, executes the draw commands contained in the corresponding command buffer (by submitting
		 * them to _queue), then waits for the commands to complete (synchronizing using a semaphore). This draws to a framebuffer.
		 * The function then presents the drawn framebuffer image to the window surface, which shows it to the window.
		 */
		void Draw();

		/**
		 * @brief Chooses the VkFormat (format and color space) for the given texture file.
		 *
		 */
		VkFormat ChooseImageFormat(const std::filesystem::path& absolutePathToImage);
	};
}
