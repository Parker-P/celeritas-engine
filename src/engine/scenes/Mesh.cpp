#include <vector>
#include <string>
#include <iostream>

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
#include "engine/structural/Pipelineable.hpp"
#include "engine/structural/Drawable.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/scenes/Mesh.hpp"
#include <utils/Utils.hpp>

namespace Engine::Scenes
{
    void Mesh::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice)
    {
		_data = Array<>

		gameObject._mesh._shaderResources._objectDataBuffer = Buffer(_logicalDevice,
			_physicalDevice,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&gameObject._transform._matrix,
			sizeof(gameObject._transform._matrix));

		gameObject._mesh._shaderResources._objectDataDescriptor = Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &gameObject._mesh._shaderResources._objectDataBuffer);
		gameObject._mesh._shaderResources._textureDescriptor = Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, nullptr, texture);

		gameObject._mesh._shaderResources._objectDataSet = DescriptorSet(_logicalDevice, VK_SHADER_STAGE_VERTEX_BIT, { &gameObject._mesh._shaderResources._objectDataDescriptor });
		gameObject._mesh._shaderResources._samplersSet = DescriptorSet(_logicalDevice, VK_SHADER_STAGE_FRAGMENT_BIT, { &gameObject._mesh._shaderResources._textureDescriptor });

		gameObject._mesh._shaderResources._objectDataPool = DescriptorPool(_logicalDevice, { &gameObject._mesh._shaderResources._objectDataSet });
		gameObject._mesh._shaderResources._samplersPool = DescriptorPool(_logicalDevice, { &gameObject._mesh._shaderResources._samplersSet });

		gameObject._mesh._shaderResources._objectDataSet.SendDescriptorData();
		gameObject._mesh._shaderResources._samplersSet.SendDescriptorData();
    }

    void Mesh::Update()
    {

    }
}