#define GLFW_INCLUDE_VULKAN
#include "LocalIncludes.hpp"

namespace Engine::Scenes
{
	Mesh::Mesh(Scene* scene)
	{
		_pScene = scene;
	}

	void Mesh::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
	{
		// Get the textures to send to the shaders.
		Vulkan::Image albedoMap;
		Vulkan::Image roughnessMap;
		Vulkan::Image metalnessMap;
		if (_materialIndex >= 0) {
			if (VK_NULL_HANDLE != _pScene->_materials[_materialIndex]._albedo._imageHandle) {
				albedoMap = _pScene->_materials[_materialIndex]._albedo;
			}
			else {
				albedoMap = Vulkan::Image::SolidColor(logicalDevice, physicalDevice, 125, 125, 125, 255);
			}
			if (VK_NULL_HANDLE != _pScene->_materials[_materialIndex]._roughness._imageHandle) {
				roughnessMap = _pScene->_materials[_materialIndex]._roughness;
			}
			else {
				roughnessMap = Vulkan::Image::SolidColor(logicalDevice, physicalDevice, 255, 255, 255, 255);
			}
			if (VK_NULL_HANDLE != _pScene->_materials[_materialIndex]._metalness._imageHandle) {
				metalnessMap = _pScene->_materials[_materialIndex]._metalness;
			}
			else {
				metalnessMap = Vulkan::Image::SolidColor(logicalDevice, physicalDevice, 125, 125, 125, 255);
			}
		}
		else {
			auto defaultMaterial = _pScene->DefaultMaterial();
			albedoMap = defaultMaterial._albedo;
			roughnessMap = defaultMaterial._roughness;
			metalnessMap = defaultMaterial._metalness;
		}

		// The mesh MUST have a material by this point, so we assign the images we created for the texture maps it needs.
		_pScene->_materials[_materialIndex]._albedo = albedoMap;
		_pScene->_materials[_materialIndex]._roughness = roughnessMap;
		_pScene->_materials[_materialIndex]._metalness = metalnessMap;
		
		// Send the textures to the GPU.
		_pScene->_materials[_materialIndex]._albedo.SendToGPU(commandPool, graphicsQueue);
		_pScene->_materials[_materialIndex]._roughness.SendToGPU(commandPool, graphicsQueue);
		_pScene->_materials[_materialIndex]._metalness.SendToGPU(commandPool, graphicsQueue);

		// Create shader resources.
		_images.push_back(albedoMap);
		_images.push_back(roughnessMap);
		_images.push_back(metalnessMap);
		_descriptors.push_back(Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, _images[0]));
		_descriptors.push_back(Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, _images[1]));
		_descriptors.push_back(Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, _images[2]));
		_sets.push_back(Vulkan::DescriptorSet(logicalDevice, VK_SHADER_STAGE_FRAGMENT_BIT, _descriptors));
		_pool = Vulkan::DescriptorPool(logicalDevice, _sets);
	}

	void Mesh::UpdateShaderResources()
	{
		// TODO
	}

	void Mesh::Update()
	{
		// TODO
	}
}