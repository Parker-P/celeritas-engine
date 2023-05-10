#include <vector>
#include <string>
#include <iostream>
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
		_buffers = Structural::Array<Vulkan::Buffer>(1);
		_descriptors = Structural::Array<Vulkan::Descriptor>(1);
		_sets = Structural::Array<Vulkan::DescriptorSet>(1);

		_environmentMap._color = Vulkan::Image(logicalDevice,
			physicalDevice,
			VK_FORMAT_R8G8B8A8_SRGB,
			VkExtent2D{ (uint32_t)_environmentMap._width, (uint32_t)_environmentMap._height },
			_environmentMap._pixelColors.data(),
			(VkImageUsageFlagBits)(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		_environmentMap._positions = Vulkan::Buffer(logicalDevice, 
			physicalDevice, 
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			_environmentMap._pixelCoordinatesWorldSpace.data(), 
			Utils::GetVectorSizeInBytes(_environmentMap._pixelCoordinatesWorldSpace));

		_environmentMap._color.SendToGPU(commandPool, graphicsQueue);
		_environmentMap._positions.SendToGPU(commandPool, graphicsQueue);

		_descriptors[0] = Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, nullptr, &_environmentMap._color);
		_descriptors[1] = Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, &_environmentMap._positions, nullptr);
		_sets[0] = Vulkan::DescriptorSet(logicalDevice, VK_SHADER_STAGE_FRAGMENT_BIT, { &_descriptors[0], &_descriptors[1]});
		_pool = Vulkan::DescriptorPool(logicalDevice, { &_sets[0] });
		_sets[0].SendToGPU();
	}

	void Scene::UpdateShaderResources()
	{
		// TODO
	}
}