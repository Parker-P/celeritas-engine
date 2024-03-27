#include <vulkan/vulkan.h>
#include "Buffer.hpp"

Engine::Vulkan::Buffer::Buffer(void* pData, size_t sizeBytes)
{
	_pData = pData;
	_sizeBytes = sizeBytes;
}
