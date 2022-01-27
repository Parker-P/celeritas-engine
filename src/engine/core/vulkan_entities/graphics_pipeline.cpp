#include <iostream>
#include <fstream>
#include <vulkan/vulkan.hpp>

#include "graphics_pipeline.h"

namespace Engine::Core::VulkanEntities {

	VkShaderModule GraphicsPipeline::CreateShaderModule(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (file) {
			std::vector<char> file_bytes(file.tellg());
			file.seekg(0, std::ios::beg);
			file.read(file_bytes.data(), file_bytes.size());
			file.close();

			VkShaderModuleCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			create_info.codeSize = file_bytes.size();
			create_info.pCode = (uint32_t*)file_bytes.data();

			VkShaderModule shader_module;
			if (vkCreateShaderModule(logical_device_, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
				std::cerr << "failed to create shader module for " << filename << std::endl;
				exit(1);
			}

			std::cout << "created shader module for " << filename << std::endl;
			return shader_module;
		}
		else {
			std::cout << "could not open file " << filename << std::endl;
		}
	}

	void GraphicsPipeline::CreateGraphicsPipeline() {
		//Compile and load the shaders
		//system((std::string("start \"\" \"") + kShaderPath_ + std::string("shader_compiler.bat\"")).c_str());
		VkShaderModule vertex_shader_module = CreateShaderModule(kShaderPath_ + std::string("vertex_shader.spv"));
		VkShaderModule fragment_shader_module = CreateShaderModule(kShaderPath_ + std::string("fragment_shader.spv"));

		//Set up shader stage info
		VkPipelineShaderStageCreateInfo vertex_shader_create_info = {};
		vertex_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertex_shader_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertex_shader_create_info.module = vertex_shader_module;
		vertex_shader_create_info.pName = "main";
		VkPipelineShaderStageCreateInfo fragment_shader_create_info = {};
		fragment_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragment_shader_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragment_shader_create_info.module = fragment_shader_module;
		fragment_shader_create_info.pName = "main";
		VkPipelineShaderStageCreateInfo shader_stages[] = { vertex_shader_create_info, fragment_shader_create_info };

		//Describe vertex input meaning how the graphics driver should interpret the information given in the vertex buffer
		VkPipelineVertexInputStateCreateInfo vertex_input_create_info = {};
		vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_create_info.vertexBindingDescriptionCount = 1;
		vertex_input_create_info.pVertexBindingDescriptions = &vertex_binding_description_;
		vertex_input_create_info.vertexAttributeDescriptionCount = 1;
		vertex_input_create_info.pVertexAttributeDescriptions = vertex_attribute_descriptions_.data();

		//Describe input assembly meaning what we are going to draw to the screen. We want to draw triangles
		VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info = {};
		input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

		//Describe viewport and scissor. The viewport specifies how the normalized window coordinates 
		//(-1 to 1 for both width and height) are transformed into the pixel coordinates of the framebuffer.
		//Scissor is the area where you can render, this is similar to the viewport in that regard but changing the scissor 
		//rectangle doesn't affect the coordinates
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)extent_.width;
		viewport.height = (float)extent_.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = extent_.width;
		scissor.extent.height = extent_.height;

		//Note: scissor test is always enabled (although dynamic scissor is possible)
		//Number of viewports must match number of scissors
		VkPipelineViewportStateCreateInfo viewport_create_info = {};
		viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_create_info.viewportCount = 1;
		viewport_create_info.pViewports = &viewport;
		viewport_create_info.scissorCount = 1;
		viewport_create_info.pScissors = &scissor;

		//Describe rasterization
		//Note: depth bias and using polygon modes other than fill require changes to logical device creation (device features)
		VkPipelineRasterizationStateCreateInfo rasterization_create_info = {};
		rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_create_info.depthClampEnable = VK_FALSE;
		rasterization_create_info.rasterizerDiscardEnable = VK_FALSE;
		rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterization_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterization_create_info.depthBiasEnable = VK_FALSE;
		rasterization_create_info.depthBiasConstantFactor = 0.0f;
		rasterization_create_info.depthBiasClamp = 0.0f;
		rasterization_create_info.depthBiasSlopeFactor = 0.0f;
		rasterization_create_info.lineWidth = 1.0f;

		//Describe multisampling
		//Note: using multisampling also requires turning on device features
		VkPipelineMultisampleStateCreateInfo multisample_create_info = {};
		multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisample_create_info.sampleShadingEnable = VK_FALSE;
		multisample_create_info.minSampleShading = 1.0f;
		multisample_create_info.alphaToCoverageEnable = VK_FALSE;
		multisample_create_info.alphaToOneEnable = VK_FALSE;

		//Describing color blending
		//Note: all paramaters except blendEnable and colorWriteMask are irrelevant here
		VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
		color_blend_attachment_state.blendEnable = VK_FALSE;
		color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		//Note: all attachments must have the same values unless a device feature is enabled
		VkPipelineColorBlendStateCreateInfo color_blend_create_info = {};
		color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blend_create_info.logicOpEnable = VK_FALSE;
		color_blend_create_info.logicOp = VK_LOGIC_OP_COPY;
		color_blend_create_info.attachmentCount = 1;
		color_blend_create_info.pAttachments = &color_blend_attachment_state;
		color_blend_create_info.blendConstants[0] = 0.0f;
		color_blend_create_info.blendConstants[1] = 0.0f;
		color_blend_create_info.blendConstants[2] = 0.0f;
		color_blend_create_info.blendConstants[3] = 0.0f;

		//Describe pipeline layout
		//Note: this describes the mapping between memory and shader resources (descriptor sets)
		//This is for uniform buffers and samplers
		VkDescriptorSetLayoutBinding layout_binding = {};
		layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layout_binding.descriptorCount = 1;
		layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
		descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptor_set_layout_create_info.bindingCount = 1;
		descriptor_set_layout_create_info.pBindings = &layout_binding;
		if (vkCreateDescriptorSetLayout(logical_device_, &descriptor_set_layout_create_info, nullptr, &descriptor_set_layout_) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor layout" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor layout" << std::endl;
		}
		VkPipelineLayoutCreateInfo layout_create_info = {};
		layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_create_info.setLayoutCount = 1;
		layout_create_info.pSetLayouts = &descriptor_set_layout_;
		if (vkCreatePipelineLayout(logical_device_, &layout_create_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
			std::cerr << "failed to create pipeline layout" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created pipeline layout" << std::endl;
		}

		//Configure the creation of the graphics pipeline
		VkGraphicsPipelineCreateInfo pipeline_create_info = {};
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = shader_stages;
		pipeline_create_info.pVertexInputState = &vertex_input_create_info;
		pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
		pipeline_create_info.pViewportState = &viewport_create_info;
		pipeline_create_info.pRasterizationState = &rasterization_create_info;
		pipeline_create_info.pMultisampleState = &multisample_create_info;
		pipeline_create_info.pColorBlendState = &color_blend_create_info;
		pipeline_create_info.layout = pipeline_layout_;
		pipeline_create_info.renderPass = render_pass_;
		pipeline_create_info.subpass = 0;
		pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
		pipeline_create_info.basePipelineIndex = -1;

		//Create the pipeline
		if (auto result = vkCreateGraphicsPipelines(logical_device_, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &graphics_pipeline_); result != VK_SUCCESS) {
			std::cerr << "failed to create graphics pipeline with error " << result << std::endl;
			exit(1);
		}
		else {
			std::cout << "created graphics pipeline" << std::endl;
		}

		//Delete the unused shader modules as they have been transferred to the VRAM now
		vkDestroyShaderModule(logical_device_, vertex_shader_module, nullptr);
		vkDestroyShaderModule(logical_device_, fragment_shader_module, nullptr);
	}
}