#pragma once

namespace Engine::Vulkan
{
	class Image
	{
	public:
		VkImageCreateInfo _createInfo{};
		VkImageViewCreateInfo _viewCreateInfo{};
		VkSamplerCreateInfo _samplerCreateInfo{};

		VkImage _image{};
		VkImageView _view{};
		VkSampler _sampler{};

		VkImageLayout _currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		void* _pData = nullptr;
		size_t _sizeBytes = 0;

		Image() = default;
		Image(void* pData, size_t sizeBytes);
	};
}
