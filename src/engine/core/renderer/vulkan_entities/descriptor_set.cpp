#include <iostream>
#include <vulkan/vulkan.h>

#include "logical_device.h"
#include "descriptor_set.h"

namespace Engine::Core::Renderer::VulkanEntities {

	void DescriptorSet::CreateDescriptorSet(LogicalDevice& logical_device) {
		//Define shader bindings
		shader_binding_.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shader_binding_.binding = 0;								//Shader binding point
		shader_binding_.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;	//Accessible from the vertex shader only (flags can be combined to make it accessible to multiple shader stages)
		shader_binding_.descriptorCount = 1;						//Binding contains one element (can be used for array bindings)

		//Define the layout of the descriptor set
		VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
		descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptor_set_layout_create_info.bindingCount = 1;
		descriptor_set_layout_create_info.pBindings = &shader_binding_;
		vkCreateDescriptorSetLayout(logical_device.GetLogicalDevice(), &descriptor_set_layout_create_info, nullptr, &descriptor_set_layout_);
	}

	VkDescriptorSet DescriptorSet::GetDescriptorSet() {
		return descriptor_set_;
	}

	void DescriptorSet::SetDescriptorSet(VkDescriptorSet descriptor_set) {
		descriptor_set_ = descriptor_set;
	}

	VkDescriptorSetLayout DescriptorSet::GetLayout() {
		return descriptor_set_layout_;
	}
}