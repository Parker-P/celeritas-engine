#include <vector>
#include <string>
#include <iostream>
#include <filesystem>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_vec.hpp>
#include <vulkan/vulkan.h>

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
#include "engine/scenes/Scene.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/scenes/Mesh.hpp"
#include "engine/scenes/SphericalEnvironmentMap.hpp"

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

		auto b = Engine::Vulkan::Buffer::Buffer(logicalDevice,
			physicalDevice,
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&_environmentMap._environmentMap,
			sizeof(_environmentMap));

		b.SendToGPU(commandPool, graphicsQueue);

		_descriptors[0] = Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &b, nullptr);
		_sets[0] = Vulkan::DescriptorSet(logicalDevice, VK_SHADER_STAGE_FRAGMENT_BIT, { &_descriptors[0] });
		_pool = Vulkan::DescriptorPool(logicalDevice, { &_sets[0] });
		_sets[0].SendToGPU();
	}

	void Scene::UpdateShaderResources()
	{
		// TODO
	}
}