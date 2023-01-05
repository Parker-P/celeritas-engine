#pragma once

// For a high level understaning of Vulkan and how it interacts with the GPU go to : https://vkguide.dev/
// For all the in depth technical information about the Vulkan API, go to:
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html
// Styleguide used for this project: https://google.github.io/styleguide/cppguide.html
// Original project was: https://gist.github.com/Overv/7ac07356037592a121225172d7d78f2d

namespace Engine::Vulkan
{
	/**
	 * @brief Vulkan's representation of a GPU.
	 */
	class PhysicalDevice
	{

	public:

		/**
		 * @brief Identifier.
		 */
		VkPhysicalDevice _handle;

		PhysicalDevice() = default;

		PhysicalDevice(VkInstance& instance);

		/**
		 * @brief Queries the device for swapchain support, returns true if swapchains are supported.
		 * @return
		 */
		bool SupportsSwapchains();

		/**
		 * @brief Queries the device for surface support for the given queue family and surface.
		 * @return True if the queue family can contain command buffers which contain commands that will draw
		 * to the given surface.
		 */
		bool SupportsSurface(uint32_t& queueFamilyIndex, VkSurfaceKHR& surface);

		/**
		 * @brief Queries the physical device to get its memory properties.
		 * @return
		 */
		VkPhysicalDeviceMemoryProperties GetMemoryProperties();

		/**
		 * @brief Returns the memory type index that Vulkan needs to categorize memory by usage properties.
		 * Vulkan uses this type index to tell the driver in which portion RAM or VRAM to allocate
		 * a resource such as an image or buffer.
		 */
		uint32_t GetMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlagBits properties);

		/**
		 * @brief Queries the physical device for queue families, and returns a vector containing
		 * the properties of all supported queue families.
		 * @return
		 */
		std::vector<VkQueueFamilyProperties> GetAllQueueFamilyProperties();

		/**
		 * @brief Gets specific queue families that have the given flags set, and that optionally support presentation to a window surface.
		 * @param queueType The flags that identify the properties of the queues you are looking for. See @see VkQueueFlagBits.
		 * @param presentSupport Whether or not the queues you want to find need to support presenting to a window surface or not.
		 * @param surface
		 */
		void GetQueueFamilyIndices(const VkQueueFlagBits& queueFlags, bool needsPresentationSupport = false, const VkSurfaceKHR& surface = VK_NULL_HANDLE);

		/**
		 * @brief Returns a structure that encapsulates the capabilities of the window surface.
		 * @param surface The window surface you want to check against.
		 * @return
		 */
		VkSurfaceCapabilitiesKHR GetSurfaceCapabilities(VkSurfaceKHR& windowSurface);

		/**
		 * @brief Returns supported image formats for the given window surface.
		 * @param windowSurface
		 * @return
		 */
		std::vector<VkSurfaceFormatKHR> GetSupportedFormatsForSurface(VkSurfaceKHR& windowSurface);

		/**
		 * @brief Returns supported present modes for the given window surface.
		 * A present mode is the logic according to which framebuffer contents will be drawn to and
		 * presented to the window, for example the mailbox present mode in Vulkan (triple buffering).
		 */
		std::vector<VkPresentModeKHR> GetSupportedPresentModesForSurface(VkSurfaceKHR& windowSurface);
	};

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
		 * @brief Used to query for GPU hardware properties, to know where the buffer can and cannot be stored in memory.
		 */
		PhysicalDevice _physicalDevice;

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

	public:

		/**
		 * @brief The handle used by Vulkan to identify this buffer.
		 */
		VkBuffer _handle;

		Buffer() = default;

		/**
		 * @brief Constructs a buffer.
		 * @param logicalDevice Needed by Vulkan to have info about the GPU so it can make function calls to allocate memory for the buffer.
		 * @param physicalDevice Needed to know which types of memory are available on the GPU, so the buffer can be allocated on the
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
		 * Further read: https://asawicki.info/news_1740_vulkan_memory_types_on_pc_and_how_to_use_them
		 * @param data Pointer to the start address of the data you want to copy to the buffer.
		 * @param sizeInBytes Size in bytes of the data you want to copy to the buffer.
		 */
		Buffer(VkDevice& logicalDevice, PhysicalDevice& physicalDevice, VkBufferUsageFlagBits usageFlags, VkMemoryPropertyFlagBits properties, void* data = nullptr, size_t sizeInBytes = 0);

		/**
		 * @brief Generates a data structure that Vulkan calls descriptor, which it uses to bind the buffer to a descriptor set.
		 */
		VkDescriptorBufferInfo GenerateDescriptor();

		/**
		 * @brief Updates the data contained in this buffer. Notes: buffer must be marked as VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT and its
		 * memory mapped to the _dataAddress member for this to work. To update a GPU only buffer, you need to use a staging buffer and
		 * submit a buffer transfer command to a queue, then have Vulkan execute the command.
		 *
		 * @param data Address of where the data begins in memory.
		 * @param sizeInBytes Size of the data in bytes.
		 */
		void UpdateData(void* data, size_t sizeInBytes);

		/**
		 * @brief Destroys the buffer and frees any memory allocated to it.
		 *
		 */
		void Destroy();
	};

	/**
	 * @brief Represents a Vulkan image. A Vulkan image uses 2 structures, 1 for storing the data the image contain (VkImage),
	 * and 1 for decorating that data with metadata (VkImageView) that Vulkan can use in the graphics pipeline to know how to treat it.
	 */
	class Image
	{

	public:

		/**
		 * @brief See Vulkan specification.
		 */
		VkDevice _logicalDevice;

		/**
		 * @brief Handle that identifies a structure that contains the image data.
		 */
		VkImage _imageHandle;

		/**
		 * @brief Handle that identifies a structure that contains image metadata, as well as a pointer to the
		 * VkImage. See @see VkImage.
		 */
		VkImageView _imageViewHandle;

		/**
		 * @brief The way the data for each pixel of the image is stored in memory.
		 */
		VkFormat _format;

		/**
		 * @brief Width and height of the image in pixels.
		 */
		VkExtent2D _sizePixels;

		/**
		 * @brief Flags that indicate what type of data this image contains.
		 */
		VkImageAspectFlagBits _typeFlags;

		/**
		 * @brief Describes how this image is going to be read by the GPU texture samplers, which feed textures to shaders.
		 */
		VkSampler _sampler;

		Image() = default;

		/**
		 * @brief Constructs a Vulkan image.
		 *
		 * @param logicalDevice
		 * @param physicalDevice
		 * @param imageFormat
		 * @param size Width and height of the image in pixels.
		 * @param usageFlags Gives Vulkan information about how it's going to be used as a render pass attachment. For example an image could be used to store depth information, denoted by using VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT.
		 * @param aspectFlags Indicates how it's meant to be used and what type of data it contains.
		 * @param memoryPropertiesFlags This can be any of the following values:
		 * 1) VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: this means GPU memory, so VRAM. If this is not set, then regular RAM is assumed.
		 * 2) VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: this means that the CPU will be able to read and write from the allocated memory IF YOU CALL vkMapMemory() FIRST.
		 * 3) VK_MEMORY_PROPERTY_HOST_CACHED_BIT: this means that the memory will be cached so that when the CPU writes to this buffer, if the data is small enough to fit in its
		 * cache (which is much faster to access) it will do that instead. Only problem is that this way, if your GPU needs to access that data, it won't be able to unless it's
		 * also marked as HOST_COHERENT.
		 * 4) VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: this means that anything that the CPU writes to the buffer will be able to be read by the GPU as well, effectively
		 * granting the GPU access to the CPU's cache (if the buffer is also marked as HOST_CACHED). COHERENT stands for consistency across memories, so it basically means
		 * that the CPU, GPU or any other device will see the same memory if trying to access the buffer. If you don't have this flag set, and you try to access the
		 * buffer from the GPU while the buffer is marked HOST_CACHED, you may not be able to get the data or even worse, you may end up reading the wrong chunk of memory.
		 * Further read: https://asawicki.info/news_1740_vulkan_memory_types_on_pc_and_how_to_use_them
		 */
		Image(VkDevice& logicalDevice, PhysicalDevice& physicalDevice, const VkFormat& imageFormat, const VkExtent2D& size, const VkImageUsageFlagBits& usageFlags, const VkImageAspectFlagBits& typeFlags, const VkMemoryPropertyFlagBits& memoryPropertiesFlags);

		/**
		 * @brief Constructs an image using a pre-existing image handle.
		 * @param image The pre-existing image handle.
		 * @param imageFormat
		 * @param size
		 */
		Image(VkDevice& logicalDevice, VkImage& image, const VkFormat& imageFormat);

		/**
		 * @brief Generates an image descriptor. An image descriptor is bound to a sampler, which tells Vulkan how to instruct
		 * the actual GPU hardware samplers on how to read and sample the particular texture.
		 */
		VkDescriptorImageInfo GenerateDescriptor();

		/**
		 * @brief Uses Vulkan calls to deallocate and remove the contents of the image from memory.
		 */
		void Destroy();
	};

	/**
	 * @brief A descriptor is just a block of data, similar to a buffer, with the difference being that a descriptor is bound to metadata
	 * that Vulkan uses to enable their use in shaders. This allows us to exchange data between a program run by the CPU's cores with a shader run 
	 * by the GPU's cores.
	 * A descriptor is accessed by a shader by using an index and a binding number, similar to vertex attributes, which are accessed 
	 * by the vertex shader using a binding and a location number.
	 * For example, in a shader, a descriptor set is declared as follows:
	 * #version 450
	 *
	 * // Descriptor declaration. The shader will access the descriptor set at index 0 with binding number 0.
	 * layout(set = 0, binding = 0) uniform UniformBuffer 
	 * {
	 *     ... data in the uniform buffer ...
	 * } uniformBuffer;
	 *
	 * void main() 
	 * {
     *		... shader code ...
	 * }
	 */
	class Descriptor
	{

	public:

		/**
		 * @brief Wrapper that describes a buffer.
		 */
		VkDescriptorBufferInfo _bufferInfo;

		/**
		 * @brief Wrapper that describes an image.
		 */
		VkDescriptorImageInfo _imageInfo;

		/**
		 * @brief Descriptor type: could be, for example, a uniform buffer (general data) or a texture sampler. A texture sampler is a 
		 * structure that contains an image and some metadata that tells the GPU how to read it.
		 */
		VkDescriptorType _type;

		/**
		 * @brief Binding number used by the shaders to know which descriptor to access within the descriptor set this descriptor belongs to.
		 */
		uint32_t _bindingNumber;

		/**
		 * @brief Default constructor.
		 */
		Descriptor() = default;

		/**
		 * @brief Constructs a descriptor given a descriptor type and a buffer.
		 * @param type Descriptor type: could be, for example, a uniform buffer (general data) or a texture sampler. A texture sampler is a 
		 * structure that contains an image and some metadata that tells the GPU how to read it.
		 * @param pBuffer Pointer to a buffer.
		 * @param bindingNumber Binding number used by a shader to know which descriptor to access within a descriptor set.
		 */
		Descriptor(const VkDescriptorType& type, Buffer& buffer, const uint32_t& bindingNumber);

		/**
		 * @brief Constructs a descriptor given a descriptor type and an image.
		 * @param type Descriptor type: could be, for example, a uniform buffer (general data) or a texture sampler. A texture sampler is a 
		 * structure that contains an image and some metadata that tells the GPU how to read it.
		 * @param pImage Pointer to an image.
		 * @param bindingNumber Binding number used by a shader to know which descriptor to access within a descriptor set.
		 */
		Descriptor(const VkDescriptorType& type, Image& image, const uint32_t& bindingNumber);

	};

	/**
	 * @brief A descriptor set has bindings to descriptors, and is used to cluster descriptors of the same type.
	 */
	class DescriptorSet 
	{
		/**
		 * @brief Used for Vulkan calls.
		 */
		VkDevice _logicalDevice;

	public:

		/**
		 * @brief Identifier for Vulkan.
		 */
		VkDescriptorSet _handle;

		/**
		 * @brief A descriptor set layout object is defined by an array of zero or more descriptor bindings. Each individual descriptor binding is specified
		 * by a descriptor type, a count (array size) of the number of descriptors in the binding, a set of shader stages that can access the binding, and
		 * (if using immutable samplers) an array of sampler descriptors.
		 */
		VkDescriptorSetLayout _layout;

		/**
		 * @brief Set index number used by the shaders to identify the descriptor set to access. This number is set only after memory for the
		 * descriptor set has been allocated by a descriptor pool.
		 */
		short _indexNumber;

		/**
		 * @brief Descriptors this set contains.
		 */
		std::vector<Descriptor*> _descriptors;

		/**
		 * @brief Writes the data contained in its descriptors to the correct GPU-visible allocated portion of memory.
		 */
		void SendDescriptorData();

		/**
		 * @brief Default constructor.
		 */
		DescriptorSet() = default;

		/**
		 * @brief Constructs a descriptor set.
		 * @param logicalDevice Logical device used to call Vulkan functions.
		 * @param shaderFlags Flags to define which shader/s can access this descriptor set.
		 * @param descriptors Descriptors. Must all be of the same type and compatible with the type of data they contain (image or buffer).
		 */
		DescriptorSet(VkDevice& logicalDevice, const VkShaderStageFlagBits& shaderStageFlags, std::vector<Descriptor*> descriptors);
	};

	/**
	 * @brief A descriptor pool is a container for descriptor sets and acts as a facility to allocate memory for them.
	 */
	class DescriptorPool
	{
		/**
		 * @brief Used to make Vulkan creation calls.
		 */
		VkDevice _logicalDevice;

		/**
		 * @brief Descriptor sets the pool contains.
		 */
		std::vector<DescriptorSet*> _descriptorSets;

		/**
		 * @brief .
		 */
		void AllocateDescriptorSets();

	public:

		/**
		 * @brief Identifier for Vulkan.
		 */
		VkDescriptorPool _handle;

		/**
		 * @brief Default constructor.
		 */
		DescriptorPool() = default;

		/**
		 * @brief Constructs a descriptor pool and allocates memory for the given descriptor sets.
		 * @param logicalDevice Used for the Vulkan call to create the descriptor pool and to allocate memory for the given descriptor sets.
		 * @param descriptorSets Descriptor sets to allocate memory for.
		 */
		DescriptorPool(VkDevice& logicalDevice, std::vector<DescriptorSet*> descriptorSets);

		/**
		 * @brief Updates a descriptor identified by its set index and descriptor index (called set and binding in the shaders respectively)
		 * with the data contained in the given buffer.
		 * @param setIndex Descriptor set index.
		 * @param descriptorIndex Descriptor index a.k.a. binding.
		 * @param data The buffer whose data will be sent to the allocated descriptor set.
		 */
		void UpdateDescriptor(Descriptor& d, Buffer& data);

		/**
		 * @brief Updates a descriptor identified by its set indexand descriptor index (called set and binding in the shaders respectively)
		 * with the data contained in the given image.
		 * @param setIndex Descriptor set index.
		 * @param descriptorIndex Descriptor index a.k.a. binding.
		 * @param data The buffer whose data will be sent to the allocated descriptor set.
		 */
		void UpdateDescriptor(Descriptor& d, Image& data);
	};

	/**
	 * @brief Represents the Vulkan application.
	 */
	class VulkanApplication : public IUpdatable
	{
	public:

		/**
		 * @brief Runs the application.
		 *
		 */
		void Run();

	private:

		/**
		 * @brief Wrapper for the window shown on screen.
		 */
		GLFWwindow* _window;

		/**
		 * @brief Root for all Vulkan functionality.
		 */
		VkInstance _instance;

		/**
		 * @brief Connects the Vulkan API to the windowing system, so that Vulkan knows how to interact with the window on the screen.
		 */
		VkSurfaceKHR _windowSurface;

		/**
		 * @brief Represents the physical GPU. This is mostly used for querying the GPU about its hardware properties.
		 */
		PhysicalDevice _physicalDevice;

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
		 * @brief A queue is an ordered collection of commands that gets passed into a command buffer.
		 */
		struct Queue
		{
			/**
			 * @brief Identifier for Vulkan.
			 */
			VkQueue _handle;

			/**
			 * @brief A queue family index enables Vulkan to group queues that serve similar purposes (a.k.a. have the same properties).
			 */
			uint32_t _familyIndex;
		};

		/**
		 * @brief Queue that supports graphics operations and presentation.
		 */
		Queue _queue;

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
		 * A render pass represents a logical step that the GPU must perform in order to render a final
		 * presentable image that will be stored in a framebuffer for presentation. To do this, render passes use
		 * what are called (in Vulkan gergo) attachments, which are rendered images that contribute to rendering
		 * the final image that will go in the framebuffer. It is the renderpass's job to also do compositing, which
		 * is defining the logic according to which the attachments are merged to create the final image.
		 * See @see Swapchain to understand what framebuffers are.
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
		 * or more precisely passing the contents of the framebuffers down to the window.
		 */
		struct {

			/**
			 * @brief Identifier for Vulkan.
			 */
			VkSwapchainKHR _handle;

			/**
			 * @brief These are the buffers that contain the final rendered images shown on screen.
			 * A framebuffer is stored on a different portion of memory with respect to the depth and
			 * color images. The depth and color images CONTRIBUTE to generating an image for a
			 * framebuffer.
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
			 * @brief Buffer that stores vertex attributes. A vertex attribute is a piece of data
			 * that decorates the vertex with more information, so that the vertex shader can
			 * do more work based on it. For example a vertex attribute could be a position or a normal vector.
			 * Based on the normal vector, the vertex shader can perform lighting calculations by computing
			 * the angle between the source of the light and the normal.
			 * At the hardware level, the contents of the vertex buffer are fed into the array of shader cores,
			 * and each vertex, along with its attributes, are processed in parallel on each thread of the cores.
			 */
			Buffer _vertexBuffer;

			/**
			 * @brief Buffer that stores indices that point into the vertex buffer. This buffer is used by Vulkan when drawing
			 * using the vkCmdDrawIndexed command. This buffer gives Vulkan information about the order in which to draw
			 * vertices.
			 */
			Buffer _indexBuffer;

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
			 * (VkBuffer) on a per-draw basis via the vkCmdBindVertexBuffers command. Vertex input attributes and vertex input
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
			 * @brief Encapsulates all information sent to the shaders as well as the info used to create the Vulkan structures to send data to the shaders.
			 */
			struct {

				/**
				 * @brief The Vulkan buffer that contains the data that will be sent to the shaders. This buffer will be a descriptor, which will
				 * be pointed to by a descriptor set. The descriptor set is the structure that actually ends up in the shaders.
				 */
				Buffer _transformationDataBuffer;

				/**
				 * @brief This is the data that will go to the vertex shader.
				 */
				struct
				{
					float tanHalfHorizontalFov;
					float aspectRatio;
					float nearClipDistance;
					float farClipDistance;
					glm::mat4 worldToCamera;
					glm::mat4 objectToWorld;
				} _transformationData;

				/**
				 * @brief Texture to be sent to the shaders.
				 */
				Image _texture;

				/**
				 * @brief Descriptor pool that contains all descriptor sets.
				 */
				DescriptorPool _descriptorPool;

				/**
				 * @brief Descriptor set that contains uniform buffer descriptors.
				 */
				DescriptorSet _uniformSet;

				/**
				 * @brief Descriptor set that contains texture sampler descriptors.
				 */
				DescriptorSet _samplerSet;

				/**
				 * @brief Descriptor that contains transformations for the vertex shader.
				 */
				Descriptor _transformationsDescriptor;

				/**
				 * @brief Desscriptor that contains a texture.
				 */
				Descriptor _textureDescriptor;

				/**
				 * @brief Access to descriptor sets from a pipeline is accomplished through a pipeline layout. Zero or more descriptor set layouts and zero or more
				 * push constant ranges are combined to form a pipeline layout object describing the complete set of resources that can be accessed by a pipeline.
				 * The pipeline layout represents a sequence of descriptor sets with each having a specific layout. This sequence of layouts is used to determine
				 * the interface between shader stages and shader resources. Each pipeline is created using a pipeline layout.
				 */
				VkPipelineLayout _pipelineLayout;

			} _shaderResources;

		} _graphicsPipeline;

		// Vulkan commands.
		VkCommandPool _commandPool;
		std::vector<VkCommandBuffer> _drawCommandBuffers;

		// Game.
		Input::KeyboardMouse& _input = Input::KeyboardMouse::Instance();
		Settings::GlobalSettings& _settings = Settings::GlobalSettings::Instance();
		Scenes::Scene _scene;
		Scenes::Camera _mainCamera;
		Scenes::GameObject _model;

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
		virtual void Update() override;

		/**
		 * GLFW callback for when the window is resized.
		 * @param window
		 * @param width
		 * @param height
		 */
		static void OnWindowResized(GLFWwindow* window, int width, int height);

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
		 * @brief Destroys all Vulkan objects by using Vulkan calls, so resources allocated in VRAM are also destroyed.
		 */
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

		/**
		 * @brief Finds the index of a queue family whose queues can contain command buffers that hold
		 * Vulkan commands that draw to the window surface.
		 */
		void FindQueueFamilyIndex();

		/**
		 * @brief Creates the logical device and the queue that will be used to contain Vulkan commands.
		 */
		void CreateLogicalDeviceAndQueue();

		void CreateDebugCallback();

		void CreateSemaphores();

		void CreateCommandPool();

		void LoadScene();

		void CreateVertexAndIndexBuffers();

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
		 * @brief For each swapchain image, executes the draw commands contained in the corresponding command buffer (by submitting
		 * them to _queue), then waits for the commands to complete (synchronizing using a semaphore). This draws to a framebuffer.
		 * The function then presents the image to the window surface, which shows it to the window.
		 */
		void Draw();

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
		 * @brief Creates a descriptor set, and fills it with the chosen number of descriptors (defined when creating the descriptor pool that will be used to allocate the descriptor set).
		 */
		void AllocateDescriptorSets();

		/**
		 * @brief Updates the data in both the uniform and texture sampler descriptor sets.
		 */
		void UpdateDescriptorSetsData();

		/**
		 * @brief Creates the descriptor set layout. See _graphicsPipeline._shaderResources.
		 */
		void CreateDescriptorSetLayout();
		
		/**
		 * @brief Creates a descriptor pool.
		 * @param descriptorCount The amount of descriptors you plan to allocate from the pool.
		 */
		void CreateDescriptorPool();

		/**
		 * @brief Update the data that will be sent to the shaders.
		 */
		void UpdateShaderData();

		/**
		 * @brief Creates the pipeline layout. See _pipelineLayout.
		 */
		void CreatePipelineLayout();

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
		 * @brief Chooses the VkFormat (format and color space) for the given texture file.
		 *
		 */
		VkFormat ChooseImageFormat(const std::filesystem::path& absolutePathToImage);

		/**
		 * @brief Loads a texture into VRAM.
		 */
		void LoadTexture();
	};
}
