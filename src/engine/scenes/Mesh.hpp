#pragma once

namespace Engine::Scenes
{
	/**
	 * @brief Represents a collection of vertices and face indices as triangles.
	 */
	class Mesh : public Structural::IVulkanUpdatable, public Engine::Structural::IDrawable, public Structural::IPipelineable
	{

	public:

		Mesh() = default;

		/**
		 * @brief Index into the materials list in the Scene this mesh belongs to. A scene shoud always have a default material defined at index 0. See the Material and Scene classes.
		 */
		int _materialIndex = 0;

		/**
		 * @brief Index into the game objects list in the Scene this mesh belongs to. See the GameObject and Scene classes.
		 */
		GameObject* _pGameObject = nullptr;

		Vulkan::ShaderResources Mesh::CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<Vulkan::DescriptorSetLayout>& layouts)
		{
			auto descriptorSetID = 3;

			// Get the textures to send to the shaders.
			auto pScene = _pGameObject->_pScene;
			auto defaultMaterial = pScene->DefaultMaterial();
			Vulkan::Image albedoMap = defaultMaterial._albedo;
			Vulkan::Image roughnessMap = defaultMaterial._roughness;
			Vulkan::Image metalnessMap = defaultMaterial._metalness;

			if (_materialIndex >= 0) {
				if (VK_NULL_HANDLE != pScene->_materials[_materialIndex]._albedo._image) {
					albedoMap = pScene->_materials[_materialIndex]._albedo;
				}
				if (VK_NULL_HANDLE != pScene->_materials[_materialIndex]._roughness._image) {
					roughnessMap = pScene->_materials[_materialIndex]._roughness;
				}
				if (VK_NULL_HANDLE != pScene->_materials[_materialIndex]._metalness._image) {
					metalnessMap = pScene->_materials[_materialIndex]._metalness;
				}
			}

			// Send the textures to the GPU.
			CopyImageToDeviceMemory(logicalDevice, physicalDevice, commandPool, queue, albedoMap._image, albedoMap._createInfo.extent.width, albedoMap._createInfo.extent.height, albedoMap._createInfo.extent.depth, albedoMap._pData, albedoMap._sizeBytes);
			CopyImageToDeviceMemory(logicalDevice, physicalDevice, commandPool, queue, albedoMap._image, roughnessMap._createInfo.extent.width, roughnessMap._createInfo.extent.height, roughnessMap._createInfo.extent.depth, roughnessMap._pData, roughnessMap._sizeBytes);
			CopyImageToDeviceMemory(logicalDevice, physicalDevice, commandPool, queue, albedoMap._image, metalnessMap._createInfo.extent.width, metalnessMap._createInfo.extent.height, metalnessMap._createInfo.extent.depth, metalnessMap._pData, metalnessMap._sizeBytes);

			auto commandBuffer = CreateCommandBuffer(logicalDevice, commandPool);
			StartRecording(commandBuffer);

			// Transition the images to shader read layout.
			{
				VkImageMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.oldLayout = albedoMap._currentLayout;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.image = albedoMap._image;
				barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
				albedoMap._currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.oldLayout = roughnessMap._currentLayout;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.image = roughnessMap._image;
				barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
				roughnessMap._currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.oldLayout = metalnessMap._currentLayout;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.image = metalnessMap._image;
				barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
				metalnessMap._currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			}

			StopRecording(commandBuffer);
			ExecuteCommands(commandBuffer, queue);

			_images.push_back(albedoMap);
			_images.push_back(roughnessMap);
			_images.push_back(metalnessMap);

			VkDescriptorPool descriptorPool{};
			VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 } };
			VkDescriptorPoolCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			createInfo.maxSets = (uint32_t)1;
			createInfo.poolSizeCount = (uint32_t)1;
			createInfo.pPoolSizes = poolSizes;
			vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);

			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = (uint32_t)1;
			allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
			VkDescriptorSet descriptorSet;
			vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet);

			VkDescriptorImageInfo imageInfo[3];
			imageInfo[0] = { albedoMap._sampler, albedoMap._view, albedoMap._currentLayout };
			imageInfo[1] = { roughnessMap._sampler, roughnessMap._view, roughnessMap._currentLayout };
			imageInfo[2] = { metalnessMap._sampler, metalnessMap._view, metalnessMap._currentLayout };
			VkWriteDescriptorSet writeInfo = {};
			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.dstSet = descriptorSet;
			writeInfo.descriptorCount = 3;
			writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeInfo.pImageInfo = imageInfo;
			writeInfo.dstBinding = 0;
			vkUpdateDescriptorSets(logicalDevice, 1, &writeInfo, 0, nullptr);

			auto descriptorSets = std::vector<VkDescriptorSet>{ descriptorSet };
			_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);
			return _shaderResources;
		}

		void Mesh::UpdateShaderResources()
		{
			// TODO
		}

		void Mesh::Update(VulkanContext& vkContext)
		{
			auto& vkBuffer = _vertices._vertexBuffer._buffer;
			auto& vertexData = _vertices._vertexData;

			// Update the Vulkan-only visible memory that is mapped to the GPU so that the updated data is also visible in the vertex shader.
			memcpy(_vertices._vertexBuffer._cpuMemory, _vertices._vertexData.data(), Utils::GetVectorSizeInBytes(_vertices._vertexData));
		}

		void Mesh::Draw(VkPipelineLayout& pipelineLayout, VkCommandBuffer& drawCommandBuffer)
		{
			VkDescriptorSet sets[] = { _shaderResources[3][0] };
			vkCmdBindDescriptorSets(drawCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 3, 1, sets, 0, nullptr);

			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(drawCommandBuffer, 0, 1, &_vertices._vertexBuffer._buffer, &offset);
			vkCmdBindIndexBuffer(drawCommandBuffer, _faceIndices._indexBuffer._buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(drawCommandBuffer, (uint32_t)_faceIndices._indexData.size(), 1, 0, 0, 0);
		}
	};
}