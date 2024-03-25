#define __STDC_LIB_EXT1__ 1

#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <ctime>
#include <stdio.h>
#include <corecrt_math.h>
//#include <stb/stb_image_write.h>
//#include <stb/stb_image.h>
#include <iostream>
#include <fstream>
#include <vector>

#include "settings/Paths.hpp"
#include "utils/Utils.hpp"

namespace Utils
{

	// All the data that this boxblur application needs to do its thing.
	class BoxBlur
	{

	private:
		// Input image size.
		unsigned int _imageWidthPixels;
		unsigned int _imageHeightPixels;
		unsigned int _radiusPixels = 60;
		unsigned char* _loadedImage;
		VkPhysicalDevice _physicalDevice; // A handle for the graphics card used in the application.

		uint32_t _workGroupCount[3]; // Size of the 3D lattice of workgroups.
		uint32_t _workGroupSize[3];  // Size of the 3D lattice of threads in each workgroup.
		uint32_t _coalescedMemory;

		// Bridging information, that allows shaders to freely access resources like buffers and images.
		VkDescriptorPool _descriptorPool;
		VkDescriptorSetLayout _descriptorSetLayout;
		VkDescriptorSet _descriptorSet;

		// Pipeline handles.
		VkPipelineLayout _pipelineLayout;
		VkPipeline _pipeline;

		// Input and output buffers.
		uint32_t _inputBufferCount;
		VkBuffer _inputBuffer;
		VkDeviceMemory _inputBufferDeviceMemory;
		uint32_t _outputBufferCount;
		VkBuffer _outputBuffer;
		VkDeviceMemory _outputBufferDeviceMemory;

		// Vulkan dependencies
		VkInstance _instance;                                                // A connection between the application and the Vulkan library .
		VkDevice _device;                                                    // A logical device, interacting with the physical device.
		VkPhysicalDeviceProperties _physicalDeviceProperties;                // Basic device properties.
		VkPhysicalDeviceMemoryProperties _physicalDeviceMemoryProperties;    // Basic memory properties of the device.
		VkDebugUtilsMessengerEXT _debugMessenger;                            // Extension for debugging.
		uint32_t _queueFamilyIndex;                                          // If multiple queues are available, specify the used one.
		VkQueue _queue;                                                      // A place, where all operations are submitted.
		VkCommandPool _commandPool;                                          // An opaque object that command buffer memory is allocated from.
		VkFence _fence;                                                      // A fence used to synchronize dispatches.

		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
		{
			std::cout << "validation layer: " << pCallbackData->pMessage << std::endl;
			return VK_FALSE;
		}

		VkResult SetupDebugUtilsMessenger()
		{
			VkDebugUtilsMessengerCreateInfoEXT
				debugUtilsMessengerCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
					(const void*)NULL,
					(VkDebugUtilsMessengerCreateFlagsEXT)0,
					(VkDebugUtilsMessageSeverityFlagsEXT)VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
					(VkDebugUtilsMessageTypeFlagsEXT)VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
					(PFN_vkDebugUtilsMessengerCallbackEXT)debugCallback,
					(void*)NULL };

			PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");

			if (func != NULL) {
				if (func(_instance, &debugUtilsMessengerCreateInfo, NULL, &_debugMessenger) != VK_SUCCESS) {
					return VK_ERROR_INITIALIZATION_FAILED;
				}
			}
			else {
				return VK_ERROR_EXTENSION_NOT_PRESENT;
			}

			return VK_SUCCESS;
		}

		void DestroyDebugUtilsMessengerEXT(VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
			//pointer to the function, as it is not part of the core. Function destroys debugging messenger
			PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT");
			if (func != NULL) {
				func(_instance, debugMessenger, pAllocator);
			}
		}

		VkResult CreateComputePipeline(VkBuffer* shaderBuffersArray, VkDeviceSize* arrayOfSizesOfEachBuffer, const char* shaderFilename)
		{
			// Create an application interface to Vulkan. This function binds the shader to the compute pipeline, so it can be used as a part of the command buffer later.
			VkResult res = VK_SUCCESS;
			uint32_t descriptorCount = 2;

			// We have two storage buffer objects in one set in one pool.
			VkDescriptorPoolSize descriptorPoolSize = { (VkDescriptorType)VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
														   (uint32_t)descriptorCount };

			const VkDescriptorType descriptorTypes[2] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
															  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };

			VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
											   (const void*)NULL,
											   (VkDescriptorPoolCreateFlags)0,
											   (uint32_t)1,
											   (uint32_t)1,
											   (const VkDescriptorPoolSize*)&descriptorPoolSize };

			res = vkCreateDescriptorPool(_device, &descriptorPoolCreateInfo, NULL, &_descriptorPool);
			if (res != VK_SUCCESS) return res;

			// Specify each object from the set as a storage buffer.
			VkDescriptorSetLayoutBinding* descriptorSetLayoutBindings =
				(VkDescriptorSetLayoutBinding*)malloc(descriptorCount * sizeof(VkDescriptorSetLayoutBinding));
			for (uint32_t i = 0; i < descriptorCount; ++i) {
				descriptorSetLayoutBindings[i].binding = (uint32_t)i;
				descriptorSetLayoutBindings[i].descriptorType = (VkDescriptorType)descriptorTypes[i];
				descriptorSetLayoutBindings[i].descriptorCount = (uint32_t)1;
				descriptorSetLayoutBindings[i].stageFlags = (VkShaderStageFlags)VK_SHADER_STAGE_COMPUTE_BIT;
				descriptorSetLayoutBindings[i].pImmutableSamplers = (const VkSampler*)NULL;
			}

			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
													(const void*)NULL,
													(VkDescriptorSetLayoutCreateFlags)0,
													(uint32_t)descriptorCount,
													(const VkDescriptorSetLayoutBinding*)descriptorSetLayoutBindings };
			//create layout
			res = vkCreateDescriptorSetLayout(_device, &descriptorSetLayoutCreateInfo, NULL, &_descriptorSetLayout);
			if (res != VK_SUCCESS) return res;
			free(descriptorSetLayoutBindings);

			//provide the layout with actual buffers and their sizes
			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
												(const void*)NULL,
												(VkDescriptorPool)_descriptorPool,
												(uint32_t)1,
												(const VkDescriptorSetLayout*)&_descriptorSetLayout };
			res = vkAllocateDescriptorSets(_device, &descriptorSetAllocateInfo, &_descriptorSet);
			if (res != VK_SUCCESS) return res;

			for (uint32_t i = 0; i < descriptorCount; ++i) {

				VkDescriptorBufferInfo descriptorBufferInfo = { shaderBuffersArray[i],
												 0,
												 arrayOfSizesOfEachBuffer[i] };

				VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
												 (const void*)NULL,
												 (VkDescriptorSet)_descriptorSet,
												 (uint32_t)i,
												 (uint32_t)0,
												 (uint32_t)1,
												 (VkDescriptorType)descriptorTypes[i],
												 0,
												 (const VkDescriptorBufferInfo*)&descriptorBufferInfo,
												 (const VkBufferView*)NULL };
				vkUpdateDescriptorSets((VkDevice)_device,
					(uint32_t)1,
					(const VkWriteDescriptorSet*)&writeDescriptorSet,
					(uint32_t)0,
					(const VkCopyDescriptorSet*)NULL);
			}

			VkPushConstantRange range;
			range.offset = 0;
			range.size = sizeof(unsigned int) * 3;
			range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
											   (const void*)NULL,
											   (VkPipelineLayoutCreateFlags)0,
											   (uint32_t)1,
											   (const VkDescriptorSetLayout*)&_descriptorSetLayout,
											   (uint32_t)1,
											   (const VkPushConstantRange*)&range };

			//create pipeline layout
			res = vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, NULL, &_pipelineLayout);
			if (res != VK_SUCCESS) return res;

			// Define the specialization constant values
			VkSpecializationMapEntry* specMapEntry = (VkSpecializationMapEntry*)malloc(sizeof(VkSpecializationMapEntry) * 3);
			VkSpecializationMapEntry specConstant0 = { 0, 0, sizeof(uint32_t) };
			VkSpecializationMapEntry specConstant1 = { 1, sizeof(uint32_t), sizeof(uint32_t) };
			VkSpecializationMapEntry specConstant2 = { 2, sizeof(uint32_t) * 2, sizeof(uint32_t) };
			specMapEntry[0] = specConstant0;
			specMapEntry[1] = specConstant1;
			specMapEntry[2] = specConstant2;
			VkSpecializationInfo specializationInfo = { 3, specMapEntry, sizeof(uint32_t) * 3, _workGroupSize };

			std::ifstream file(shaderFilename, std::ios::ate | std::ios::binary);
			VkShaderModule shaderModule = nullptr;
			if (file.is_open()) {
				std::vector<char> fileBytes(file.tellg());
				file.seekg(0, std::ios::beg);
				file.read(fileBytes.data(), fileBytes.size());
				file.close();

				VkShaderModuleCreateInfo createInfo = {};
				createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				createInfo.codeSize = fileBytes.size();
				createInfo.pCode = (uint32_t*)fileBytes.data();

				if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
					std::cout << "failed to create shader module for " << shaderFilename << std::endl;
				}
			}
			else {
				std::cout << "failed to open file " << shaderFilename << std::endl;
			}

			VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
													(const void*)NULL,
													(VkPipelineShaderStageCreateFlags)0,
													(VkShaderStageFlagBits)VK_SHADER_STAGE_COMPUTE_BIT,
													(VkShaderModule)shaderModule,
													(const char*)"main",
													(VkSpecializationInfo*)&specializationInfo };

			VkComputePipelineCreateInfo computePipelineCreateInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
												(const void*)NULL,
												(VkPipelineCreateFlags)0,
												pipelineShaderStageCreateInfo,
												(VkPipelineLayout)_pipelineLayout,
												(VkPipeline)NULL,
												(int32_t)0 };

			//create pipeline
			res = vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, NULL, &_pipeline);
			if (res != VK_SUCCESS) return res;

			vkDestroyShaderModule(_device, pipelineShaderStageCreateInfo.module, NULL);
			return res;
		}

		VkResult Dispatch()
		{
			VkResult res = VK_SUCCESS;
			VkCommandBuffer commandBuffer = { 0 };

			// Create a command buffer to be executed on the GPU.
			VkCommandBufferAllocateInfo commandBufferAllocateInfo;
			commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.pNext = (const void*)NULL;
			commandBufferAllocateInfo.commandPool = _commandPool;
			commandBufferAllocateInfo.level = (VkCommandBufferLevel)VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocateInfo.commandBufferCount = (uint32_t)1;
			res = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &commandBuffer);

			// Begin command buffer recording.
			VkCommandBufferBeginInfo commandBufferBeginInfo;
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			commandBufferBeginInfo.pNext = (const void*)NULL;
			commandBufferBeginInfo.flags = (VkCommandBufferUsageFlags)VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			commandBufferBeginInfo.pInheritanceInfo = (const VkCommandBufferInheritanceInfo*)NULL;
			res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

			if (res != VK_SUCCESS) return res;

			unsigned int pushConstants[3] = { _imageWidthPixels, _imageHeightPixels, _radiusPixels };
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);
			vkCmdPushConstants(commandBuffer, _pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(unsigned int) * 3, pushConstants);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _pipelineLayout, 0, 1, &_descriptorSet, 0, NULL);
			vkCmdDispatch(commandBuffer, _workGroupCount[0], _workGroupCount[1], _workGroupCount[2]);

			// End command buffer recording.
			res = vkEndCommandBuffer(commandBuffer);
			if (res != VK_SUCCESS) return res;

			// Submit the command buffer.
			VkSubmitInfo submitInfo;
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.pNext = (const void*)NULL;
			submitInfo.waitSemaphoreCount = (uint32_t)0;
			submitInfo.pWaitSemaphores = (const VkSemaphore*)NULL;
			submitInfo.pWaitDstStageMask = (const VkPipelineStageFlags*)NULL;
			submitInfo.commandBufferCount = (uint32_t)1;
			submitInfo.pCommandBuffers = (const VkCommandBuffer*)&commandBuffer;
			submitInfo.signalSemaphoreCount = (uint32_t)0;
			submitInfo.pSignalSemaphores = (const VkSemaphore*)NULL;
			clock_t t;
			t = clock();
			res = vkQueueSubmit(_queue, 1, &submitInfo, _fence);
			if (res != VK_SUCCESS) return res;

			// Wait for the fence that was configured to signal when execution has finished.
			res = vkWaitForFences(_device, 1, &_fence, VK_TRUE, 30000000000);
			if (res != VK_SUCCESS) return res;

			// Calculate the time in milliseconds it took to execute and print the result to the console.
			t = clock() - t;
			double time = ((double)t) / CLOCKS_PER_SEC * 1000;
			std::cout << "Executions finished in " << (float)time << "ms\n";

			// Destroy the command buffer and reset the fence's status. A fence can be signalled or unsignalled. Once a fence is signalled, control is given back to the
			// the code that called vkWaitForFences().
			res = vkResetFences(_device, 1, &_fence);
			if (res != VK_SUCCESS) return res;

			vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
			return res;
		}

		VkResult FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t memoryTypeBits, VkMemoryPropertyFlags memoryPropertyFlags, uint32_t* memoryTypeIndex)
		{
			//find memory with specified properties
			VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties = { 0 };

			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

			for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; ++i) {
				if ((memoryTypeBits & (1 << i)) && ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags))
				{
					memoryTypeIndex[0] = i;
					return VK_SUCCESS;
				}
			}
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		VkResult AllocateGPUOnlyBuffer(VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize bufferSizeBytes, VkBuffer* outBuffer, VkDeviceMemory* outDeviceMemory)
		{
			//allocate the buffer used by the GPU with specified properties
			VkResult res = VK_SUCCESS;
			uint32_t queueFamilyIndices;
			VkBufferCreateInfo bufferCreateInfo;
			bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferCreateInfo.pNext = (const void*)NULL;
			bufferCreateInfo.flags = (VkBufferCreateFlags)0;
			bufferCreateInfo.size = (VkDeviceSize)bufferSizeBytes;
			bufferCreateInfo.usage = (VkBufferUsageFlags)bufferUsageFlags;
			bufferCreateInfo.sharingMode = (VkSharingMode)VK_SHARING_MODE_EXCLUSIVE;
			bufferCreateInfo.queueFamilyIndexCount = (uint32_t)1;
			bufferCreateInfo.pQueueFamilyIndices = (const uint32_t*)&queueFamilyIndices;

			res = vkCreateBuffer(_device, &bufferCreateInfo, NULL, outBuffer);
			if (res != VK_SUCCESS) return res;

			VkMemoryRequirements memoryRequirements = { 0 };
			vkGetBufferMemoryRequirements(_device, *outBuffer, &memoryRequirements);

			//find memory with specified properties
			uint32_t memoryTypeIndex = 0xFFFFFFFF;
			vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &_physicalDeviceMemoryProperties);

			for (uint32_t i = 0; i < _physicalDeviceMemoryProperties.memoryTypeCount; ++i) {
				if ((memoryRequirements.memoryTypeBits & (1 << i)) && ((_physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags))
				{
					memoryTypeIndex = i;
					break;
				}
			}
			if (0xFFFFFFFF == memoryTypeIndex) return VK_ERROR_INITIALIZATION_FAILED;

			VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
										 (const void*)NULL,
										 (VkDeviceSize)memoryRequirements.size,
										 (uint32_t)memoryTypeIndex };

			res = vkAllocateMemory(_device, &memoryAllocateInfo, NULL, outDeviceMemory);
			if (res != VK_SUCCESS) return res;

			res = vkBindBufferMemory(_device, *outBuffer, *outDeviceMemory, 0);
			return res;
		}

		VkResult UploadDataToGPU(void* data, VkBuffer* outBuffer, VkDeviceSize bufferSizeBytes)
		{
			VkResult res = VK_SUCCESS;

			//a function that transfers data from the CPU to the GPU using staging buffer,
				//because the GPU memory is not host-coherent
			VkDeviceSize stagingBufferSize = bufferSizeBytes;
			VkBuffer stagingBuffer = { 0 };
			VkDeviceMemory stagingBufferMemory = { 0 };
			res = AllocateGPUOnlyBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBufferSize,
				&stagingBuffer,
				&stagingBufferMemory);
			if (res != VK_SUCCESS) return res;

			void* stagingData;
			res = vkMapMemory(_device, stagingBufferMemory, 0, stagingBufferSize, 0, &stagingData);
			if (res != VK_SUCCESS) return res;
			memcpy(stagingData, data, stagingBufferSize);
			vkUnmapMemory(_device, stagingBufferMemory);

			VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
												(const void*)NULL,
												(VkCommandPool)_commandPool,
												(VkCommandBufferLevel)VK_COMMAND_BUFFER_LEVEL_PRIMARY,
												(uint32_t)1 };
			VkCommandBuffer commandBuffer = { 0 };
			res = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &commandBuffer);
			if (res != VK_SUCCESS) return res;



			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
											 (const void*)NULL,
											 (VkCommandBufferUsageFlags)VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
											 (const VkCommandBufferInheritanceInfo*)NULL };
			res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
			if (res != VK_SUCCESS) return res;
			VkBufferCopy copyRegion = { 0, 0, stagingBufferSize };
			vkCmdCopyBuffer(commandBuffer, stagingBuffer, *outBuffer, 1, &copyRegion);
			res = vkEndCommandBuffer(commandBuffer);
			if (res != VK_SUCCESS) return res;

			VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO,
								 (const void*)NULL,
								 (uint32_t)0,
								 (const VkSemaphore*)NULL,
								 (const VkPipelineStageFlags*)NULL,
								 (uint32_t)1,
								 (const VkCommandBuffer*)&commandBuffer,
								 (uint32_t)0,
								 (const VkSemaphore*)NULL };

			res = vkQueueSubmit(_queue, 1, &submitInfo, _fence);
			if (res != VK_SUCCESS) return res;


			res = vkWaitForFences(_device, 1, &_fence, VK_TRUE, 100000000000);
			if (res != VK_SUCCESS) return res;

			res = vkResetFences(_device, 1, &_fence);
			if (res != VK_SUCCESS) return res;

			vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
			vkDestroyBuffer(_device, stagingBuffer, NULL);
			vkFreeMemory(_device, stagingBufferMemory, NULL);
			return res;
		}

		VkResult DownloadDataFromGPU(void* data, VkBuffer* outBuffer, VkDeviceSize bufferSize)
		{
			VkResult res = VK_SUCCESS;
			VkCommandBuffer commandBuffer = { 0 };

			// A function that transfers data from the GPU to the CPU using staging buffer, because GPU memory is not host-coherent (meaning it is
			// not synced with the contents of CPU memory).
			VkDeviceSize stagingBufferSize = bufferSize;
			VkBuffer stagingBuffer = { 0 };
			VkDeviceMemory stagingBufferMemory = { 0 };
			res = AllocateGPUOnlyBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBufferSize,
				&stagingBuffer,
				&stagingBufferMemory);
			if (res != VK_SUCCESS) return res;

			VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
												(const void*)NULL,
												(VkCommandPool)_commandPool,
												(VkCommandBufferLevel)VK_COMMAND_BUFFER_LEVEL_PRIMARY,
												(uint32_t)1 };
			res = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &commandBuffer);
			if (res != VK_SUCCESS) return res;

			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
											 (const void*)NULL,
											 (VkCommandBufferUsageFlags)VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
											 (const VkCommandBufferInheritanceInfo*)NULL };
			res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
			if (res != VK_SUCCESS) return res;

			VkBufferCopy copyRegion = { (VkDeviceSize)0,
								 (VkDeviceSize)0,
								 (VkDeviceSize)stagingBufferSize };

			vkCmdCopyBuffer(commandBuffer, outBuffer[0], stagingBuffer, 1, &copyRegion);
			vkEndCommandBuffer(commandBuffer);

			VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO,
								 (const void*)NULL,
								 (uint32_t)0,
								 (const VkSemaphore*)NULL,
								 (const VkPipelineStageFlags*)NULL,
								 (uint32_t)1,
								 (const VkCommandBuffer*)&commandBuffer,
								 (uint32_t)0,
								 (const VkSemaphore*)NULL };

			res = vkQueueSubmit(_queue, 1, &submitInfo, _fence);
			if (res != VK_SUCCESS) return res;

			res = vkWaitForFences(_device, 1, &_fence, VK_TRUE, 100000000000);
			if (res != VK_SUCCESS) return res;

			res = vkResetFences(_device, 1, &_fence);
			if (res != VK_SUCCESS) return res;

			vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);

			void* stagingData;
			res = vkMapMemory(_device, stagingBufferMemory, 0, stagingBufferSize, 0, &stagingData);
			if (res != VK_SUCCESS) return res;

			memcpy(data, stagingData, stagingBufferSize);
			vkUnmapMemory(_device, stagingBufferMemory);

			vkDestroyBuffer(_device, stagingBuffer, NULL);
			vkFreeMemory(_device, stagingBufferMemory, NULL);
			return res;
		}

		VkResult GetComputeQueueFamilyIndex()
		{
			//find a queue family for a selected GPU, select the first available for use
			uint32_t queueFamilyCount;
			vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, NULL);

			VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);

			vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, queueFamilies);
			uint32_t i = 0;
			for (; i < queueFamilyCount; i++) {
				VkQueueFamilyProperties props = queueFamilies[i];

				if (props.queueCount > 0 && (props.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
					break;
				}
			}
			free(queueFamilies);
			if (i == queueFamilyCount) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			_queueFamilyIndex = i;
			return VK_SUCCESS;
		}

		VkResult CreateLogicalDevice()
		{
			//create logical device representation
			VkResult res = VK_SUCCESS;
			res = GetComputeQueueFamilyIndex();
			if (res != VK_SUCCESS) return res;
			vkGetDeviceQueue(_device, _queueFamilyIndex, 0, &_queue);
			return res;

			float queuePriorities = 1.0;
			VkDeviceQueueCreateInfo deviceQueueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					(const void*)NULL,
					(VkDeviceQueueCreateFlags)0,
					(uint32_t)_queueFamilyIndex,
					(uint32_t)1,
					(const float*)&queuePriorities };

			VkPhysicalDeviceFeatures physicalDeviceFeatures = { 0 };
			physicalDeviceFeatures.shaderFloat64 = VK_TRUE;//this enables double precision support in shaders 

			VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
					(const void*)NULL,
					(VkDeviceCreateFlags)0,
					(uint32_t)1,
					(const VkDeviceQueueCreateInfo*)&deviceQueueCreateInfo,
					(uint32_t)0,
					(const char* const*)NULL,
					(uint32_t)0,
					(const char* const*)NULL,
					(const VkPhysicalDeviceFeatures*)&physicalDeviceFeatures };

			res = vkCreateDevice(_physicalDevice, &deviceCreateInfo, NULL, &_device);
			if (res != VK_SUCCESS) return res;

		}

		void InitializeVulkan()
		{
			VkResult res = VK_SUCCESS;

			// Create the instance - a connection between the application and the Vulkan library.
			/*res = CreateInstance(vkGPU);
			std::cout << "Instance creation failed, error code: " << res;*/

			// Set up the debugging messenger.
			/*res = SetupDebugUtilsMessenger(vkGPU);
			std::cout << "Debug utils messenger creation failed, error code: " << res;*/

			// Check if there are GPUs that support Vulkan and select one.
			/*res = FindGPU(vkGPU);
			std::cout << "Physical device not found, error code: " << res;*/

			// Create logical device representation.
			if (CreateLogicalDevice() != VK_SUCCESS) {
				std::cout << "Logical device creation failed." << std::endl;
			}

			// Create a fence for synchronization.
			VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
			if (vkCreateFence(_device, &fenceCreateInfo, NULL, &_fence)) {
				std::cout << "Fence creation failed." << std::endl;
			}

			// Create a structure from which command buffer memory is allocated from.
			VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, _queueFamilyIndex };
			if (vkCreateCommandPool(_device, &commandPoolCreateInfo, NULL, &_commandPool)) {
				std::cout << "Command Pool Creation failed." << std::endl;
			}
		}

		void CalculateWorkGroupCountAndSize()
		{
			// Prepare variables.
			uint32_t maxWorkGroupInvocations = _physicalDeviceProperties.limits.maxComputeWorkGroupInvocations;
			uint32_t* maxWorkGroupSize = _physicalDeviceProperties.limits.maxComputeWorkGroupSize;
			uint32_t* maxWorkGroupCount = _physicalDeviceProperties.limits.maxComputeWorkGroupCount;
			uint32_t workGroupSize[3] = { 1, 1, 1 };
			uint32_t workGroupCount[3] = { 1, 1, 1 };

			// Use the work group size first, as that directly controls the amount of threads 1 to 1.
			uint32_t totalWorkGroupSize = workGroupSize[0] * workGroupSize[1] * workGroupSize[2];
			for (int i = 0; i < 3; ++i) {
				for (; workGroupSize[i] < maxWorkGroupSize[i]; ++workGroupSize[i]) {
					totalWorkGroupSize = workGroupSize[0] * workGroupSize[1] * workGroupSize[2];
					if (totalWorkGroupSize >= _inputBufferCount || totalWorkGroupSize == maxWorkGroupInvocations) { break; }
				}
			}

			// If one workgroup still doesn't do it, use multiple workgroups with the size of each one calculated earlier.
			if (totalWorkGroupSize < _inputBufferCount) {
				for (int i = 0; i < 3; ++i) {
					for (; workGroupCount[i] < maxWorkGroupCount[i]; ++workGroupCount[i]) {
						if (((workGroupCount[0] * workGroupCount[1] * workGroupCount[2]) * totalWorkGroupSize) >= _inputBufferCount) { break; }
					}
				}
			}

			// Return the calculated size and count in the output params.
			_workGroupCount[0] = workGroupCount[0];
			_workGroupCount[1] = workGroupCount[1];
			_workGroupCount[2] = workGroupCount[2];
			_workGroupSize[0] = workGroupSize[0];
			_workGroupSize[1] = workGroupSize[1];
			_workGroupSize[2] = workGroupSize[2];
		}

	public:
		unsigned char* Run(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, unsigned char* loadedImage, unsigned int imageWidthPixels, unsigned int imageHeightPixels, unsigned int radiusPixels = 15)
		{
			_physicalDevice = physicalDevice;
			_device = logicalDevice;
			_imageWidthPixels = imageWidthPixels;
			_imageHeightPixels = imageHeightPixels;
			_radiusPixels = radiusPixels;

			InitializeVulkan();

			// Get device properties and memory properties, if needed.
			vkGetPhysicalDeviceProperties(physicalDevice, &_physicalDeviceProperties);
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &_physicalDeviceMemoryProperties);

			// Load the image.
			int wantedComponents = 4;

			// In the stbi_load() function, comp stands for components. In a PNG image, for example, there are 4 components 
			// for each pixel, red, green, blue and alpha.
			// The image's pixels are read and stored left to right, top to bottom, relative to the image.
			// Each pixel's component is an unsigned char.
			int width = imageWidthPixels;
			int height = imageHeightPixels;

			//unsigned char* loadedImage = stbi_load("C:\\Users\\paolo.parker\\source\\repos\\celeritas-engine\\textures\\LeftFace.png", &width, &height, &componentsDetected, wantedComponents);

			// The most optimal memory has the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT flag and is usually not accessible by the CPU on dedicated graphics cards. 
			// The memory type that allows us to access it from the CPU may not be the most optimal memory type for the graphics card itself to read from.
			// Generate some random data in the array to use as input for the compute shader.
			uint32_t inputAndOutputBufferSizeInBytes = (uint32_t)(imageWidthPixels * imageHeightPixels) * 4;

			// Prepare the input and output buffers.
			_inputBufferCount = (uint32_t)(imageWidthPixels * imageHeightPixels);
			_inputBuffer;
			_inputBufferDeviceMemory;
			_outputBufferCount = _inputBufferCount;
			_outputBuffer;
			_outputBufferDeviceMemory;

			// Calculate how many workgroups and the size of each workgroup we are going to use.
			// We want one GPU thread to operate on a single value from the input buffer, so the required thread size is the input buffer size.
			CalculateWorkGroupCountAndSize();

			//use default values if coalescedMemory = 0
			if (_coalescedMemory == 0) {
				switch (_physicalDeviceProperties.vendorID) {
				case 0x10DE://NVIDIA - change to 128 before Pascal
					_coalescedMemory = 32;
					break;
				case 0x8086://INTEL
					_coalescedMemory = 64;
					break;
				case 0x13B5://AMD
					_coalescedMemory = 64;
					break;
				default:
					_coalescedMemory = 64;
					break;
				}
			}

			// Create the input buffer.
			VkResult res = VK_SUCCESS;
			if (AllocateGPUOnlyBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, //device local memory
				inputAndOutputBufferSizeInBytes,
				&_inputBuffer,
				&_inputBufferDeviceMemory) != VK_SUCCESS) {
				std::cout << "Input buffer allocation failed." << std::endl;
			}

			// Create the output buffer.
			if (AllocateGPUOnlyBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, //device local memory
				inputAndOutputBufferSizeInBytes,
				&_outputBuffer,
				&_outputBufferDeviceMemory) != VK_SUCCESS) {
				std::cout << "Output buffer allocation failed." << std::endl;
			}

			// Transfer data to GPU staging buffer and thereafter sync the staging buffer with GPU local memory.
			if (UploadDataToGPU(loadedImage, &_inputBuffer, inputAndOutputBufferSizeInBytes) != VK_SUCCESS) {
				std::cout << "Failed uploading image to GPU." << std::endl;
			}

			VkBuffer buffers[2] = { _inputBuffer, _outputBuffer };
			VkDeviceSize buffersSize[2] = { inputAndOutputBufferSizeInBytes, inputAndOutputBufferSizeInBytes };

			// Create 
			//const char* shaderPath = "C:\\code\\vulkan-compute\\shaders\\Shader.spv";
			auto shaderPath = Settings::Paths::ShadersPath() /= L"compute\\BoxBlur.spv";
			if (CreateComputePipeline(buffers, buffersSize, shaderPath.string().c_str()) != VK_SUCCESS) {
				std::cout << "Application creation failed." << std::endl;
			}

			if (Dispatch() != VK_SUCCESS) {
				std::cout << "Application run failed." << std::endl;
			}

			unsigned char* shaderOutputBufferData = (unsigned char*)malloc(inputAndOutputBufferSizeInBytes);

			res = vkGetFenceStatus(_device, _fence);

			//Transfer data from GPU using staging buffer, if needed
			if (DownloadDataFromGPU(shaderOutputBufferData,
				&_outputBuffer,
				inputAndOutputBufferSizeInBytes) != VK_SUCCESS) {
				std::cout << "Failed downloading image from GPU." << std::endl;
			}

			// Print data for debugging.
			/*printf("Output buffer result: [");
			uint32_t startAndEndSize = 50;
			uint32_t i = 0;
			for (; i < outputBufferCount - 1 && i < startAndEndSize; ++i) {
				printf("%d, ", shaderOutputBufferData[i]);
			}
			if (i >= startAndEndSize) {
				printf("..., ");
				i = outputBufferCount - startAndEndSize + 1;
				for (; i < outputBufferCount - 1; ++i)
					printf("%d, ", shaderOutputBufferData[i]);
			}
			printf("%d]\n", shaderOutputBufferData[outputBufferCount - 1]);*/

			// Free resources.
			vkDestroyBuffer(_device, _inputBuffer, nullptr);
			vkFreeMemory(_device, _inputBufferDeviceMemory, nullptr);
			vkDestroyBuffer(_device, _outputBuffer, nullptr);
			vkFreeMemory(_device, _outputBufferDeviceMemory, nullptr);

			return shaderOutputBufferData;
		}

		void Destroy() {
			vkDestroyFence(_device, _fence, nullptr);
			vkDestroyDescriptorPool(_device, _descriptorPool, NULL);
			vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, NULL);
			vkDestroyPipelineLayout(_device, _pipelineLayout, NULL);
			vkDestroyPipeline(_device, _pipeline, NULL);
		}
	};
}