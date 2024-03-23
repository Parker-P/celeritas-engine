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

		VkDeviceMemory _gpuMemory = nullptr;
		void* _cpuMemory = nullptr;
	};
}
