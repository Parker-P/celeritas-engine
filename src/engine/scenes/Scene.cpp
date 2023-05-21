#include <vector>
#include <string>
#include <iostream>
#include <optional>
#include <filesystem>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_vec.hpp>
#include <vulkan/vulkan.h>

#include "utils/Utils.hpp"
#include "structural/IUpdatable.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/Scenes/Vertex.hpp"
#include "engine/structural/Drawable.hpp"
#include "structural/IUpdatable.hpp"
#include "engine/math/Transform.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/structural/IPipelineable.hpp"
#include "engine/scenes/PointLight.hpp"
#include "engine/scenes/SphericalEnvironmentMap.hpp"
#include "engine/scenes/Scene.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/scenes/Mesh.hpp"

namespace Engine::Scenes
{
	void Scene::Update()
	{
		for (auto& light : _pointLights) {
			light.Update();
		}

		for (auto& gameObject : _gameObjects) {
			gameObject.Update();
		}
	}

	void Scene::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
	{
		/*_environmentMap._colors = Vulkan::Image(logicalDevice,
			physicalDevice,
			VK_FORMAT_R8G8B8A8_SRGB,
			VkExtent2D{ (uint32_t)_environmentMap._width, (uint32_t)_environmentMap._height },
			_environmentMap._pixelColors.data(),
			(VkImageUsageFlagBits)(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);*/

		/*_environmentMap._positions = Vulkan::Buffer(logicalDevice,
			physicalDevice,
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			_environmentMap._pixelCoordinatesWorldSpace.data(),
			Utils::GetVectorSizeInBytes(_environmentMap._pixelCoordinatesWorldSpace),
			VK_FORMAT_R32G32B32_SFLOAT);*/

		/*_test.resize(3);
		_test[0] = glm::vec3(1.0f, 0.0f, 0.0f);
		_test[1] = glm::vec3(0.0f, 1.0f, 0.0f);
		_test[2] = glm::vec3(1.0f, 0.0f, 1.0f);*/

		_environmentMap._environmentColorsBuffer = Vulkan::Buffer(logicalDevice,
			physicalDevice,
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			_environmentMap._pixelColors.data(),
			Utils::GetVectorSizeInBytes(_environmentMap._pixelColors),
			VK_FORMAT_R32G32B32_SFLOAT);

		_environmentMap._environmentPositionsBuffer = Vulkan::Buffer(logicalDevice,
			physicalDevice,
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			_environmentMap._pixelCoordinatesWorldSpace.data(),
			Utils::GetVectorSizeInBytes(_environmentMap._pixelCoordinatesWorldSpace),
			VK_FORMAT_R32G32B32_SFLOAT);

		_environmentMap._envSize._environmentDataEntryCount = (int)_environmentMap._pixelColors.size();
		_environmentMap._entryCountBuffer = Vulkan::Buffer(logicalDevice,
			physicalDevice,
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&_environmentMap._envSize,
			sizeof(_environmentMap._envSize));

		_environmentMap._environmentColorsBuffer.SendToGPU(commandPool, graphicsQueue);
		_environmentMap._environmentPositionsBuffer.SendToGPU(commandPool, graphicsQueue);
		_environmentMap._entryCountBuffer.SendToGPU(commandPool, graphicsQueue);
		_buffers.push_back(_environmentMap._environmentColorsBuffer);
		_buffers.push_back(_environmentMap._environmentPositionsBuffer);
		_buffers.push_back(_environmentMap._entryCountBuffer);

		_descriptors.push_back(Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 0, _environmentMap._environmentColorsBuffer));
		_descriptors.push_back(Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, _environmentMap._environmentPositionsBuffer));
		_descriptors.push_back(Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, _environmentMap._entryCountBuffer));
		
		_sets.push_back(Vulkan::DescriptorSet(logicalDevice, VK_SHADER_STAGE_FRAGMENT_BIT, _descriptors));
		_pool = Vulkan::DescriptorPool(logicalDevice, _sets);
	}

	void Scene::UpdateShaderResources()
	{
		// TODO
	}
}