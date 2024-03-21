#define GLFW_INCLUDE_VULKAN
#include "Includes.hpp"

namespace Engine::Scenes
{

	Scene::Scene(VkDevice& logicalDevice, Vulkan::PhysicalDevice physicalDevice)
	{
		_materials.push_back(Material(logicalDevice, physicalDevice));
	}

	Material Scene::DefaultMaterial()
	{
		if (_materials.size() <= 0) {
			std::cout << "there is a problem... a scene object should always have at least a default material" << std::endl;
			std::exit(1);
		}
		return _materials[0];
	}

	void Scene::Update()
	{
		for (auto& light : _pointLights) {
			light.Update();
		}

		for (auto& gameObject : _gameObjects) {
			gameObject.Update();
		}
	}

	void Scene::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue, VkDescriptorSetLayout& outLayout, VkDescriptorSet& outDescriptorSet)
	{
		_environmentMap = CubicalEnvironmentMap(physicalDevice, logicalDevice);
		//_environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Waterfall.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Debug.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "ModernBuilding.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Workshop.png");
		//_environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Workshop.hdr");
		_environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "garden.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "ItalianFlag.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "TestPng.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "EnvMap.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "texture.jpg");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Test1.png");

		_environmentMap.CreateShaderResources(physicalDevice, logicalDevice, commandPool, graphicsQueue);

		// Map the cubemap image to the fragment shader.
		{
			VkDescriptorPool descriptorPool{};
			VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
			VkDescriptorPoolCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			createInfo.maxSets = (uint32_t)1;
			createInfo.poolSizeCount = (uint32_t)1;
			createInfo.pPoolSizes = poolSizes;
			vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);

			VkDescriptorSetLayoutBinding bindings[1] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_environmentMap._cubeMapImage.sampler } };
			VkDescriptorSetLayoutCreateInfo descriptorSetCreateInfo{};
			descriptorSetCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetCreateInfo.bindingCount = 1;
			descriptorSetCreateInfo.pBindings = bindings;
			vkCreateDescriptorSetLayout(logicalDevice, &descriptorSetCreateInfo, nullptr, &outLayout);

			// Create the descriptor set.
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = (uint32_t)1;
			allocInfo.pSetLayouts = &outLayout;
			vkAllocateDescriptorSets(logicalDevice, &allocInfo, &outDescriptorSet);

			// Update the descriptor set's data with the environment map's image.
			VkDescriptorImageInfo imageInfo{ _environmentMap._cubeMapImage.sampler, _environmentMap._cubeMapImage.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkWriteDescriptorSet writeInfo = {};
			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.dstSet = outDescriptorSet;
			writeInfo.descriptorCount = 1;
			writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeInfo.pImageInfo = &imageInfo;
			writeInfo.dstBinding = 0;
			vkUpdateDescriptorSets(logicalDevice, 1, &writeInfo, 0, nullptr);
		}
	}

	void Scene::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
	{
	}

	void Scene::UpdateShaderResources()
	{
		// TODO
	}
}