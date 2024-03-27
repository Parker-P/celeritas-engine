#include <vulkan/vulkan.h>
#include "Image.hpp"

Engine::Vulkan::Image::Image(void* pData, size_t sizeBytes)
{
	_pData = pData;
	_sizeBytes = sizeBytes;
}
