#pragma once

namespace Engine::Vulkan
{
	class Buffer
	{
	public:
		VkBufferCreateInfo _createInfo{};
		VkBufferViewCreateInfo _viewCreateInfo{};

		VkBuffer _buffer{};
		VkBufferView _view{};

		/**
		 * @brief Vulkan-only handle that Vulkan uses to handle the buffer on GPU memory.
		 */
		VkDeviceMemory _gpuMemory = nullptr;

		/**
		 * @brief Pointer to CPU-accessible memory that Vulkan uses to read/write memory from/to the buffer. This is separate from _pData because Vulkan might need to
		 * also tell the GPU where this data is stored, and by making this being able to be set only via Vulkan calls, it ensures that it catches all changes to the data
		 * and maintains it coherent across CPU and GPU.
		 */
		void* _cpuMemory = nullptr;

		/**
		 * @brief Pointer to CPU-only visible data.
		 */
		void* _pData = nullptr;

		size_t _sizeBytes = 0;

		Buffer() = default;
		Buffer(void* pData, size_t sizeBytes);
	};
}
