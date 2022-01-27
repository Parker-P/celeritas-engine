//External includes
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <functional>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/BaseImporter.h>
#include <assimp/Importer.hpp> 
#include <assimp/scene.h>
#include <assimp/postprocess.h>

//Config
#include "app_config.h"

//Vulkan entities
#include "vulkan_entities/swap_chain.h"
#include "vulkan_entities/command_pool.h"
#include "vulkan_entities/semaphore.h"
#include "vulkan_entities/instance.h"
#include "vulkan_entities/window_surface.h"
#include "vulkan_entities/physical_device.h"
#include "vulkan_entities/logical_device.h"
#include "vulkan_application.h"

//Utils
#include "../src/engine/utils/singleton.h"
#include "../src/engine/core/vulkan_application.h"
#include "../src/engine/utils/vulkan_factory.h"

float forward = 0.0f;
float right = 0.0f;
float up = 0.0f;
float rotate = 0.0f;
bool upPressed = false;
bool downPressed = false;
bool leftPressed = false;
bool rightPressed = false;
bool shiftPressed = false;
bool ctrlPressed = false;
bool ePressed = false;
bool qPressed = false;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_UP && action == GLFW_REPEAT) {
		upPressed = true;
	}
	if (key == GLFW_KEY_DOWN && action == GLFW_REPEAT) {
		downPressed = true;
	}
	if (key == GLFW_KEY_LEFT && action == GLFW_REPEAT) {
		leftPressed = true;
	}
	if (key == GLFW_KEY_RIGHT && action == GLFW_REPEAT) {
		rightPressed = true;
	}
	if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_REPEAT) {
		shiftPressed = true;
	}
	if (key == GLFW_KEY_LEFT_CONTROL && action == GLFW_REPEAT) {
		ctrlPressed = true;
	}
	if (key == GLFW_KEY_E && action == GLFW_REPEAT) {
		ePressed = true;
	}
	if (key == GLFW_KEY_Q && action == GLFW_REPEAT) {
		qPressed = true;
	}
}

// Vertex layout
struct Vertex {
	float pos[3];
};

namespace Engine::Core {
	VulkanApplication::VulkanApplication(Engine::Core::AppConfig app_config) {
		this->app_config_ = app_config;
	}

	void VulkanApplication::Run() {
		time_start_ = std::chrono::high_resolution_clock::now();
		WindowInit();
		SetupVulkan();
		MainLoop();
		Cleanup(true);
	}

	void VulkanApplication::WindowInit() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window_ = glfwCreateWindow(app_config_.settings_.width_, app_config_.settings_.height_, app_config_.appInfo_.name, nullptr, nullptr);
		glfwSetWindowSizeCallback(window_, VulkanApplication::OnWindowResized);
		glfwSetKeyCallback(window_, key_callback);
	}

	void VulkanApplication::SetupVulkan() {
		instance_.CreateInstance(app_config_);
		window_surface_.CreateWindowSurface(instance_, physical_device_, window_);
		physical_device_.SelectPhysicalDevice(instance_, window_surface_);
		logical_device_.CreateLogicalDevice(physical_device_, app_config_);
		image_available_semaphore_.CreateSemaphore(logical_device_);
		rendering_finished_semaphore_.CreateSemaphore(logical_device_);
		graphics_command_pool_.CreateCommandPool(logical_device_, physical_device_.GetGraphicsQueue());
		swap_chain_.CreateSwapChain(physical_device_, logical_device_, window_surface_, app_config_);

		CreateVertexAndIndexBuffers();
		CreateUniformBuffer();

		CreateGraphicsPipeline();
		CreateDescriptorPool();
		CreateDescriptorSets();
		CreateCommandBuffers();
	}

	void VulkanApplication::MainLoop() {
		while (!glfwWindowShouldClose(window_)) {
			UpdateUniformData();
			Draw();
			glfwPollEvents();
		}
	}

	void VulkanApplication::OnWindowResized(GLFWwindow* window, int width, int height) {
		VulkanFactory::GetInstance().GetApplicationByGlfwWindow(window)->window_resized_ = true;
	}

	void VulkanApplication::OnWindowSizeChanged() {
		window_resized_ = false;
		// Only recreate objects that are affected by framebuffer size changes
		Cleanup(false);
		CreateFramebuffers();
		CreateGraphicsPipeline();
		CreateCommandBuffers();
	}

	void VulkanApplication::Cleanup(bool fullClean) {
		vkDeviceWaitIdle(logical_device_);
		vkFreeCommandBuffers(logical_device_, command_pool_, (uint32_t)graphics_command_buffers_.size(), graphics_command_buffers_.data());
		vkDestroyPipeline(logical_device_, graphics_pipeline_, nullptr);
		vkDestroyRenderPass(logical_device_, render_pass_, nullptr);
		for (size_t i = 0; i < images_.size(); i++) {
			vkDestroyFramebuffer(logical_device_, swap_chain_frame_buffers_[i], nullptr);
			vkDestroyImageView(logical_device_, image_views_[i], nullptr);
		}
		vkDestroyDescriptorSetLayout(logical_device_, descriptor_set_layout_, nullptr);
		if (fullClean) {
			vkDestroySemaphore(logical_device_, image_available_semaphore_, nullptr);
			vkDestroySemaphore(logical_device_, rendering_finished_semaphore_, nullptr);
			vkDestroyCommandPool(logical_device_, command_pool_, nullptr);

			//Clean up uniform buffer related objects
			vkDestroyDescriptorPool(logical_device_, descriptor_pool_, nullptr);
			vkDestroyBuffer(logical_device_, uniform_buffer_, nullptr);
			vkFreeMemory(logical_device_, uniform_buffer_memory_, nullptr);

			//Buffers must be destroyed after no command buffers are referring to them anymore
			vkDestroyBuffer(logical_device_, vertex_buffer_, nullptr);
			vkFreeMemory(logical_device_, vertex_buffer_memory_, nullptr);
			vkDestroyBuffer(logical_device_, index_buffer_, nullptr);
			vkFreeMemory(logical_device_, index_buffer_memory_, nullptr);

			//Note: implicitly destroys images (in fact, we're not allowed to do that explicitly)
			vkDestroySwapchainKHR(logical_device_, swap_chain_, nullptr);
			vkDestroyDevice(logical_device_, nullptr);
			vkDestroySurfaceKHR(instance_, window_surface_, nullptr);
			if (kEnableDebugging_) {
				PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugReportCallbackEXT");
				DestroyDebugReportCallback(instance_, callback_, nullptr);
			}
			vkDestroyInstance(instance_, nullptr);
		}
	}

	uint32_t indices_size;

	void VulkanApplication::CreateVertexAndIndexBuffers() {
		//In this function we copy vertex and face information to the GPU.
		//With a GPU we have two types of memory: RAM (on the motherboard) and VRAM (on the GPU).
		//The memory this program uses to allocate variables is the RAM because the instructions are processed by the CPU
		//which uses the RAM. Our goal is to have the GPU read vertex information, but for it to be as fast as possible
		//we want it to be reading from the VRAM because it sits inside the GPU and would be much quicker to read, so our goal 
		//is to transfer the vertex information from here (the RAM) to the VRAM once so it's much quicker to read multiple times.
		//The procedure to copy data to the VRAM is quite
		//complicated because of the nature of how the GPU works. What we need to do is:
		//1) Allocate some memory on the VRAM that is visible to both the CPU and the GPU. This will be what we call the staging buffer
		//This staging buffer is a temporary location that we use to expose the data we want to send to the GPU
		//2) Copy the vertex information to the allocated memory (to the staging buffer)
		//3) Allocate memory on the VRAM that is only visible to the GPU. This is the memory location that will be used by the shaders
		//and the destination to where we will copy the data we previously copied to the staging buffer
		//4) Create a command buffer and fill it with instructions to copy data from the staging buffer to the VRAM
		//5) Submit the command buffer to a queue for it to be processed by the GPU

		//Import a model
		// Create an instance of the Importer class
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile("C:\\Users\\Paolo Parker\\source\\repos\\Celeritas Engine\\models\\monkey.dae",
			aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_SortByPType);

		// If the import failed, report it
		if (nullptr != scene) {
			std::cout << importer.GetErrorString();
		}

		//Add the vertices from the imported model
		std::vector<Vertex> vertices;
		for (int mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index) {
			vertices.reserve(scene->mMeshes[mesh_index]->mVertices->Length());
			for (int vert_index = 0; vert_index < scene->mMeshes[mesh_index]->mNumVertices; ++vert_index) {
				Vertex v{ { static_cast<float>(scene->mMeshes[mesh_index]->mVertices[vert_index].x), static_cast<float>(scene->mMeshes[mesh_index]->mVertices[vert_index].y), static_cast<float>(scene->mMeshes[mesh_index]->mVertices[vert_index].z) } };
				vertices.emplace_back(v);
			}
		}

		//Setup vertices. Vulkan's normalized viewport coordinate system is very weird: +Y points down, +X points to the right, 
		//+Z points towards you. The origin is at the exact center of the viewport. Very unintuitive.
		//std::vector<Vertex> vertices = {
		//	{ -1.0f, 1.0f, 1.0f }, //Bottom left near
		//	{ -1.0f, 1.0f, -1.0f }, //Bottom left far
		//	{ 1.0f, 1.0f, 1.0f }, //Bottom right near
		//	{ 1.0f, 1.0f, -1.0f }, //Bottom right far
		//	{ 0.0f, -1.0f, 0.0f } //Tip
		//};
		uint32_t vertices_size = static_cast<uint32_t>(vertices.size() * (sizeof(float) * 3));

		//Add faces from the imported model
		std::vector<uint32_t> faces;
		for (int mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index) {
			faces.reserve(scene->mMeshes[mesh_index]->mNumFaces);
			for (int face_index = 0; face_index < scene->mMeshes[mesh_index]->mNumFaces; ++face_index) {
				for (int i = 0; i < scene->mMeshes[mesh_index]->mFaces[face_index].mNumIndices; ++i) {
					faces.emplace_back(static_cast<uint32_t>(scene->mMeshes[mesh_index]->mFaces[face_index].mIndices[i]));
				}
			}
		}

		//Setup indices (faces) for faces to actually show, vertex indices need to be defined in counter clockwise order
		//std::vector<faces> indices = { 0, 2, 4, 2, 3, 4, 3, 1, 4, 1, 0, 4, 1, 3, 0, 0, 3, 2 };
		//uint32_t indices_size = (uint32_t)(indices.size() * (sizeof(int) * 3));
		uint32_t faces_size = static_cast<uint32_t>(faces.size() * sizeof(int));
		indices_size = faces_size;

		//This tells the GPU how to read vertex data
		vertex_binding_description_.binding = 0;
		vertex_binding_description_.stride = sizeof(vertices[0]);
		vertex_binding_description_.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		//This tells the GPU how to connect shader variables and vertex data
		vertex_attribute_descriptions_.resize(1);
		vertex_attribute_descriptions_[0].binding = 0;
		vertex_attribute_descriptions_[0].location = 0;
		vertex_attribute_descriptions_[0].format = VK_FORMAT_R32G32B32_SFLOAT;

		//Get memory related variables ready. Note that the HOST_COHERENT_BIT flag allows us to not have to flush
		//to the VRAM once allocating memory, it tells Vulkan to do this automatically
		VkMemoryAllocateInfo mem_alloc = {};
		mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mem_alloc.memoryTypeIndex = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		VkMemoryRequirements mem_reqs;

		//Prepare the staging buffer data structure/layout.
		struct StagingBuffer {
			VkDeviceMemory memory;
			VkBuffer buffer;
		};
		struct {
			StagingBuffer vertices;
			StagingBuffer indices;
		} staging_buffers;

		//1) Allocate some memory on the VRAM that is visible to both the CPU and the GPU. This will be what we call the staging buffer
		VkBufferCreateInfo vertex_buffer_info = {};
		vertex_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertex_buffer_info.size = vertices_size;
		vertex_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		vkCreateBuffer(logical_device_, &vertex_buffer_info, nullptr, &staging_buffers.vertices.buffer);
		vkGetBufferMemoryRequirements(logical_device_, staging_buffers.vertices.buffer, &mem_reqs);
		mem_alloc.allocationSize = mem_reqs.size;
		GetMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mem_alloc.memoryTypeIndex);
		vkAllocateMemory(logical_device_, &mem_alloc, nullptr, &staging_buffers.vertices.memory);

		//2) Copy the vertex information to the allocated memory (to the staging buffer)
		void* data;
		vkMapMemory(logical_device_, staging_buffers.vertices.memory, 0, vertices_size, 0, &data);
		memcpy(data, vertices.data(), vertices_size);
		vkUnmapMemory(logical_device_, staging_buffers.vertices.memory);
		vkBindBufferMemory(logical_device_, staging_buffers.vertices.buffer, staging_buffers.vertices.memory, 0);

		//3) Allocate memory on the VRAM that is only visible to the GPU. This is the memory location that will be used by the shaders
		vertex_buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vkCreateBuffer(logical_device_, &vertex_buffer_info, nullptr, &vertex_buffer_);
		vkGetBufferMemoryRequirements(logical_device_, vertex_buffer_, &mem_reqs);
		mem_alloc.allocationSize = mem_reqs.size;
		GetMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mem_alloc.memoryTypeIndex);
		vkAllocateMemory(logical_device_, &mem_alloc, nullptr, &vertex_buffer_memory_);
		vkBindBufferMemory(logical_device_, vertex_buffer_, vertex_buffer_memory_, 0);

		//Now we repeat the same steps for the index buffer (face information)
		//1) Allocate some memory on the RAM that is visible to both the CPU and the GPU. This will be what we call the staging buffer
		VkBufferCreateInfo index_buffer_info = {};
		index_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		index_buffer_info.size = faces_size;
		index_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		vkCreateBuffer(logical_device_, &index_buffer_info, nullptr, &staging_buffers.indices.buffer);
		vkGetBufferMemoryRequirements(logical_device_, staging_buffers.indices.buffer, &mem_reqs);
		mem_alloc.allocationSize = mem_reqs.size;
		GetMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mem_alloc.memoryTypeIndex);
		vkAllocateMemory(logical_device_, &mem_alloc, nullptr, &staging_buffers.indices.memory);

		//2) Copy the vertex information to the allocated memory (to the staging buffer)
		vkMapMemory(logical_device_, staging_buffers.indices.memory, 0, faces_size, 0, &data);
		memcpy(data, faces.data(), faces_size);
		vkUnmapMemory(logical_device_, staging_buffers.indices.memory);
		vkBindBufferMemory(logical_device_, staging_buffers.indices.buffer, staging_buffers.indices.memory, 0);

		//3) Allocate memory on the VRAM that is only visible to the GPU. This is the memory location that will be used by the shaders
		index_buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vkCreateBuffer(logical_device_, &index_buffer_info, nullptr, &index_buffer_);
		vkGetBufferMemoryRequirements(logical_device_, index_buffer_, &mem_reqs);
		mem_alloc.allocationSize = mem_reqs.size;
		GetMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mem_alloc.memoryTypeIndex);
		vkAllocateMemory(logical_device_, &mem_alloc, nullptr, &index_buffer_memory_);
		vkBindBufferMemory(logical_device_, index_buffer_, index_buffer_memory_, 0);

		//We are done with passing information to the staging buffers, now we need to actually copy the buffers to the VRAM

		//4) Create a command buffer and fill it with instructions to copy data from the staging buffers to the VRAM
		VkCommandBufferAllocateInfo cmd_buf_info = {};
		cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_buf_info.commandPool = command_pool_;
		cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_buf_info.commandBufferCount = 1;
		VkCommandBuffer copy_command_buffer;
		vkAllocateCommandBuffers(logical_device_, &cmd_buf_info, &copy_command_buffer);
		VkCommandBufferBeginInfo buffer_begin_info = {};
		buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(copy_command_buffer, &buffer_begin_info);
		VkBufferCopy copyRegion = {};
		copyRegion.size = vertices_size;
		vkCmdCopyBuffer(copy_command_buffer, staging_buffers.vertices.buffer, vertex_buffer_, 1, &copyRegion);
		copyRegion.size = faces_size;
		vkCmdCopyBuffer(copy_command_buffer, staging_buffers.indices.buffer, index_buffer_, 1, &copyRegion);
		vkEndCommandBuffer(copy_command_buffer);

		//5) Submit the command buffer to a queue for it to be processed by the GPU
		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &copy_command_buffer;
		vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphics_queue_);
		vkFreeCommandBuffers(logical_device_, command_pool_, 1, &copy_command_buffer);

		//Destroy the temporary staging buffers to free VRAM
		vkDestroyBuffer(logical_device_, staging_buffers.vertices.buffer, nullptr);
		vkFreeMemory(logical_device_, staging_buffers.vertices.memory, nullptr);
		vkDestroyBuffer(logical_device_, staging_buffers.indices.buffer, nullptr);
		vkFreeMemory(logical_device_, staging_buffers.indices.memory, nullptr);

		std::cout << "set up vertex and index buffers" << std::endl;
	}

	void VulkanApplication::CreateUniformBuffer() {
		//Configure the uniform buffer creation
		VkBufferCreateInfo buffer_info = {};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = sizeof(uniform_buffer_data_);
		buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		vkCreateBuffer(logical_device_, &buffer_info, nullptr, &uniform_buffer_);

		//Get memory requirements for the uniform buffer
		VkMemoryRequirements mem_reqs;
		vkGetBufferMemoryRequirements(logical_device_, uniform_buffer_, &mem_reqs);

		//Allocate memory for the uniform buffer on the VRAM (using the HOST_VISIBLE_BIT flag) and make it CPU accessible (using the vkMapMemory call)
		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_reqs.size;
		GetMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &alloc_info.memoryTypeIndex);
		vkAllocateMemory(logical_device_, &alloc_info, nullptr, &uniform_buffer_memory_);
		vkBindBufferMemory(logical_device_, uniform_buffer_, uniform_buffer_memory_, 0);

		//Copy the buffer to the VRAM
		UpdateUniformData();
	}

	void VulkanApplication::UpdateUniformData() {
		if (upPressed) {
			upPressed = false;
			forward += 0.2f;
		}
		if (downPressed) {
			downPressed = false;
			forward -= 0.2f;
		}
		if (rightPressed) {
			rightPressed = false;
			right += 0.2f;
		}
		if (leftPressed) {
			leftPressed = false;
			right -= 0.2f;
		}
		if (shiftPressed) {
			shiftPressed = false;
			up += 0.2f;
		}
		if (ctrlPressed) {
			ctrlPressed = false;
			up -= 0.2f;
		}
		if (ePressed) {
			ePressed = false;
			rotate += 1.f;
		}
		if (qPressed) {
			qPressed = false;
			rotate -= 1.f;
		}

		//Set up transformation matrices
		uniform_buffer_data_.model_matrix = glm::translate(glm::mat4x4(1), glm::vec3(right, up, forward));
		uniform_buffer_data_.model_matrix *= glm::rotate(glm::mat4x4(1), glm::radians(rotate), glm::vec3(0, 1.0f, 0.0f));
		uniform_buffer_data_.view_matrix = glm::mat4x4(1);
		uniform_buffer_data_.projection_matrix = glm::perspective(glm::radians(70.f), (float)extent_.width / (float)extent_.height, 0.1f, 1000.0f);

		//Copy the data to the VRAM (this procedure is similar to what we do when creating the vertex and index buffers)
		void* data;
		vkMapMemory(logical_device_, uniform_buffer_memory_, 0, sizeof(uniform_buffer_data_), 0, &data);
		memcpy(data, &uniform_buffer_data_, sizeof(uniform_buffer_data_));
		vkUnmapMemory(logical_device_, uniform_buffer_memory_);
	}

	//Find device memory that is supported by the requirements (typeBits) and meets the desired properties
	VkBool32 VulkanApplication::GetMemoryType(uint32_t typeBits, VkFlags properties, uint32_t* typeIndex) {
		for (uint32_t i = 0; i < 32; i++) {
			if ((typeBits & 1) == 1) {
				if ((device_memory_properties_.memoryTypes[i].propertyFlags & properties) == properties) {
					*typeIndex = i;
					return true;
				}
			}
			typeBits >>= 1;
		}
		return false;
	}	

	void VulkanApplication::CreateDescriptorPool() {
		//This describes how many descriptor sets we'll create from this pool for each type
		VkDescriptorPoolSize type_count;
		type_count.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		type_count.descriptorCount = 1;

		//Configure the pool creation
		VkDescriptorPoolCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		create_info.poolSizeCount = 1;
		create_info.pPoolSizes = &type_count;
		create_info.maxSets = 1;

		//Create the pool with the specified config
		if (vkCreateDescriptorPool(logical_device_, &create_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor pool" << std::endl;
			exit(1);
		}
		else {
			std::cout << "descriptor pool created" << std::endl;
		}
	}

	void VulkanApplication::CreateDescriptorSets() {
		//With descriptor sets there are three levels. You have the descriptor set that contains descriptors.
		//Descriptors are buffers (pieces of memory) that point to uniform buffers and also contain other
		//information such as the size and the type of the uniform buffer they point to.
		//The uniform buffer is the last in the chain: the uniform buffer contains the actual data we want
		//to pass to the shaders.
		//Desciptor sets are allocated using a descriptor pool, but the behaviour of the pool is handled by
		//the Vulkan drivers so we don't need to worry about how it works.

		//There needs to be one descriptor set per binding point in the shader
		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool = descriptor_pool_;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &descriptor_set_layout_;

		//Create the descriptor set
		if (vkAllocateDescriptorSets(logical_device_, &alloc_info, &descriptor_set_) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor set" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor set" << std::endl;
		}

		//Bind the uniform buffer to the descriptor. This descriptor will then be bound to a descriptor set and then that descriptor
		//set will be uploaded to the VRAM
		VkDescriptorBufferInfo descriptor_buffer_info = {};
		descriptor_buffer_info.buffer = uniform_buffer_;
		descriptor_buffer_info.offset = 0;
		descriptor_buffer_info.range = sizeof(uniform_buffer_data_);

		//Bind the descriptor to the descriptor set
		VkWriteDescriptorSet write_descriptor_set = {};
		write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_set.dstSet = descriptor_set_;
		write_descriptor_set.descriptorCount = 1;
		write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write_descriptor_set.pBufferInfo = &descriptor_buffer_info;
		write_descriptor_set.dstBinding = 0;

		//Send the descriptor set to the VRAM
		vkUpdateDescriptorSets(logical_device_, 1, &write_descriptor_set, 0, nullptr);
	}

	void VulkanApplication::CreateCommandBuffers() {
		//This is where it all comes together. In this function we allocate memory for the command buffers from the command pool,
		//then, we configure and record commands on it and we submit it to a queue for it to be processed by the GPU.

		//Configure and allocate command buffers from the command pool
		graphics_command_buffers_.resize(images_.size());
		VkCommandBufferAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = command_pool_;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = (uint32_t)graphics_command_buffers_.size();
		if (vkAllocateCommandBuffers(logical_device_, &alloc_info, graphics_command_buffers_.data()) != VK_SUCCESS) {
			std::cerr << "failed to allocate graphics command buffers" << std::endl;
			exit(1);
		}
		else {
			std::cout << "allocated graphics command buffers" << std::endl;
		}

		//Configure command buffer command recording
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		VkImageSubresourceRange sub_resource_range = {};
		sub_resource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		sub_resource_range.baseMipLevel = 0;
		sub_resource_range.levelCount = 1;
		sub_resource_range.baseArrayLayer = 0;
		sub_resource_range.layerCount = 1;

		//Set the background color
		VkClearValue clearColor = {
			{ 0.1f, 0.1f, 0.1f, 1.0f } // R, G, B, A
		};

		//For each image in the swapchain, we record the same set of commands
		for (size_t i = 0; i < images_.size(); i++) {

			//Start recording commands
			vkBeginCommandBuffer(graphics_command_buffers_[i], &begin_info);

			//If present queue family and graphics queue family are different, then a barrier is necessary
			//The barrier is also needed initially to transition the image to the present layout
			VkImageMemoryBarrier present_to_draw_barrier = {};
			present_to_draw_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			present_to_draw_barrier.srcAccessMask = 0;
			present_to_draw_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			present_to_draw_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			present_to_draw_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			if (present_queue_family_ != graphics_queue_family_) {
				present_to_draw_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				present_to_draw_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			}
			else {
				present_to_draw_barrier.srcQueueFamilyIndex = present_queue_family_;
				present_to_draw_barrier.dstQueueFamilyIndex = graphics_queue_family_;
			}
			present_to_draw_barrier.image = images_[i];
			present_to_draw_barrier.subresourceRange = sub_resource_range;
			vkCmdPipelineBarrier(graphics_command_buffers_[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &present_to_draw_barrier);

			//Configure a render pass instance and tell Vulkan to instantiate a render pass and run it
			VkRenderPassBeginInfo render_pass_begin_info = {};
			render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			render_pass_begin_info.renderPass = render_pass_;
			render_pass_begin_info.framebuffer = swap_chain_frame_buffers_[i];
			render_pass_begin_info.renderArea.offset.x = 0;
			render_pass_begin_info.renderArea.offset.y = 0;
			render_pass_begin_info.renderArea.extent = extent_;
			render_pass_begin_info.clearValueCount = 1;
			render_pass_begin_info.pClearValues = &clearColor;
			vkCmdBeginRenderPass(graphics_command_buffers_[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

			//Bind the data to be sent to the shaders (descriptor sets)
			vkCmdBindDescriptorSets(graphics_command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);

			//Bind the graphics pipeline. The graphics pipeline contains all the information Vulkan needs to render an image
			vkCmdBindPipeline(graphics_command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

			//Bind the index and vertex buffers
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(graphics_command_buffers_[i], 0, 1, &vertex_buffer_, &offset);
			vkCmdBindIndexBuffer(graphics_command_buffers_[i], index_buffer_, 0, VK_INDEX_TYPE_UINT32);

			//Draw the triangles
			vkCmdDrawIndexed(graphics_command_buffers_[i], indices_size, 1, 0, 0, 0);

			//End the render pass
			vkCmdEndRenderPass(graphics_command_buffers_[i]);

			// If present and graphics queue families differ, then another barrier is required
			if (present_queue_family_ != graphics_queue_family_) {
				VkImageMemoryBarrier draw_to_present_barrier = {};
				draw_to_present_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				draw_to_present_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				draw_to_present_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
				draw_to_present_barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				draw_to_present_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				draw_to_present_barrier.srcQueueFamilyIndex = graphics_queue_family_;
				draw_to_present_barrier.dstQueueFamilyIndex = present_queue_family_;
				draw_to_present_barrier.image = images_[i];
				draw_to_present_barrier.subresourceRange = sub_resource_range;
				vkCmdPipelineBarrier(graphics_command_buffers_[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &draw_to_present_barrier);
			}

			//Stop recording commands
			if (vkEndCommandBuffer(graphics_command_buffers_[i]) != VK_SUCCESS) {
				std::cerr << "failed to record command buffer" << std::endl;
				exit(1);
			}
		}

		std::cout << "recorded command buffers" << std::endl;

		//No longer needed
		vkDestroyPipelineLayout(logical_device_, pipeline_layout_, nullptr);
	}

	void VulkanApplication::Draw() {
		//Acquire image
		uint32_t imageIndex;
		VkResult res = vkAcquireNextImageKHR(logical_device_, swap_chain_, UINT64_MAX, image_available_semaphore_, VK_NULL_HANDLE, &imageIndex);

		//Unless surface is out of date right now, defer swap chain recreation until end of this frame
		if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
			OnWindowSizeChanged();
			return;
		}
		else if (res != VK_SUCCESS) {
			std::cerr << "failed to acquire image" << std::endl;
			exit(1);
		}

		//Wait for image to be available and draw
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &image_available_semaphore_;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &rendering_finished_semaphore_;

		//This is the stage where the queue should wait on the semaphore
		VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		submitInfo.pWaitDstStageMask = &waitDstStageMask;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &graphics_command_buffers_[imageIndex];
		if (vkQueueSubmit(graphics_queue_, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
			std::cerr << "failed to submit draw command buffer" << std::endl;
			exit(1);
		}

		//Present drawn image
		//Note: semaphore here is not strictly necessary, because commands are processed in submission order within a single queue
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &rendering_finished_semaphore_;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swap_chain_;
		presentInfo.pImageIndices = &imageIndex;
		res = vkQueuePresentKHR(present_queue_, &presentInfo);
		if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR || window_resized_) {
			OnWindowSizeChanged();
		}
		else if (res != VK_SUCCESS) {
			std::cerr << "failed to submit present command buffer" << std::endl;
			exit(1);
		}
	}
}

