#pragma once

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

		/**
		 * @brief Default constructor.
		 *
		 */
		PhysicalDevice() = default;

		/**
		 * @brief Uses Vulkan calls to query the graphics driver for a list of GPUs. If it finds a GPU that supports Vulkan,
		 * constructs a physical device object given a Vulkan instance, created using vkCreateInstance().
		 * @param instance The Vulkan instance.
		 */
		PhysicalDevice(VkInstance& instance);

		/**
		 * @brief Allocates memory according to the requirements, and returns a handle to be used strictly via the Vulkan
		 * API to access the allocated memory.
		 * @param memoryType This can be any of the following values:
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
		VkDeviceMemory AllocateMemory(VkDevice& logicalDevice, const VkMemoryRequirements& memoryRequirements, const VkMemoryPropertyFlagBits& memoryType);

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
}
