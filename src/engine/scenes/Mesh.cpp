#include <vector>
#include <string>
#include <iostream>
#include <optional>
#include <filesystem>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_vec.hpp>
#include <vulkan/vulkan.h>

#include "structural/IUpdatable.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/structural/IPipelineable.hpp"
#include "engine/structural/Drawable.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/math/Transform.hpp"
#include "engine/scenes/PointLight.hpp"
#include "engine/scenes/CubicalEnvironmentMap.hpp"
#include "engine/scenes/Scene.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Mesh.hpp"
#include "engine/vulkan/Image.hpp"
#include <utils/Utils.hpp>

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
		if (_pScene->_materials.size() > 0) {
			if (VK_NULL_HANDLE != _pScene->_materials[_materialIndex]._albedo._imageHandle) {
				albedoMap = _pScene->_materials[_materialIndex]._albedo;
			}
			if (VK_NULL_HANDLE != _pScene->_materials[_materialIndex]._roughness._imageHandle) {
				roughnessMap = _pScene->_materials[_materialIndex]._roughness;
			}
			if (VK_NULL_HANDLE != _pScene->_materials[_materialIndex]._metalness._imageHandle) {
				metalnessMap = _pScene->_materials[_materialIndex]._metalness;
			}
		}
		else {
			albedoMap = Vulkan::Image::SolidColor(logicalDevice, physicalDevice, 125, 125, 125, 255);
			roughnessMap = Vulkan::Image::SolidColor(logicalDevice, physicalDevice, 125, 125, 125, 255);
			metalnessMap = Vulkan::Image::SolidColor(logicalDevice, physicalDevice, 125, 125, 125, 255);
		}

		// Send the textures to the GPU.
		roughnessMap.SendToGPU(commandPool, graphicsQueue);
		metalnessMap.SendToGPU(commandPool, graphicsQueue);

		// Create shader resources.
		_images.push_back(albedoMap);
		_images.push_back(roughnessMap);
		_images.push_back(metalnessMap);
		_descriptors.push_back(Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, albedoMap));
		_descriptors.push_back(Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, roughnessMap));
		_descriptors.push_back(Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, metalnessMap));
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