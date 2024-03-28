#define GLFW_INCLUDE_VULKAN
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <functional>
#include <optional>
#include <filesystem>
#include <map>
#include <bitset>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "LocalIncludes.hpp"

namespace Engine::Scenes
{
	Camera::Camera()
	{
		_horizontalFov = 55.0f;
		_nearClippingDistance = 0.1f;
		_farClippingDistance = 200.0f;
		_up = glm::vec3(0.0f, 1.0f, 0.0f);
	}

	Camera::Camera(float horizontalFov, float nearClippingDistance, float farClippingDistance)
	{
		_horizontalFov = horizontalFov;
		_nearClippingDistance = nearClippingDistance;
		_farClippingDistance = farClippingDistance;
		_up = glm::vec3(0.0f, 1.0f, 0.0f);
	}

	Vulkan::ShaderResources Camera::CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<Vulkan::DescriptorSetLayout>& layouts)
	{
		using namespace Engine::Vulkan;

		auto descriptorSetID = 0;

		// Create a temporary buffer.
		Buffer buffer{};
		auto bufferSizeBytes = sizeof(_cameraData);
		buffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer._createInfo.size = bufferSizeBytes;
		buffer._createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		vkCreateBuffer(logicalDevice, &buffer._createInfo, nullptr, &buffer._buffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(logicalDevice, buffer._buffer, &requirements);
		buffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Map memory to the correct GPU and CPU ranges for the buffer.
		vkBindBufferMemory(logicalDevice, buffer._buffer, buffer._gpuMemory, 0);
		vkMapMemory(logicalDevice, buffer._gpuMemory, 0, bufferSizeBytes, 0, &buffer._cpuMemory);
		memcpy(buffer._cpuMemory, &_cameraData, bufferSizeBytes);

		_buffers.push_back(buffer);

		VkDescriptorPool descriptorPool{};
		VkDescriptorPoolSize poolSizes[1] = { VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.maxSets = (uint32_t)1;
		createInfo.poolSizeCount = (uint32_t)1;
		createInfo.pPoolSizes = poolSizes;
		vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);

		// Create the descriptor set.
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = (uint32_t)1;
		allocInfo.pSetLayouts = &layouts[descriptorSetID]._layout;
		VkDescriptorSet descriptorSet;
		vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet);

		// Update the descriptor set's data with the environment map's image.
		VkDescriptorBufferInfo bufferInfo{ buffer._buffer, 0, buffer._createInfo.size };
		VkWriteDescriptorSet writeInfo = {};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.dstSet = descriptorSet;
		writeInfo.descriptorCount = 1;
		writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeInfo.pBufferInfo = &bufferInfo;
		writeInfo.dstBinding = 0;
		vkUpdateDescriptorSets(logicalDevice, 1, &writeInfo, 0, nullptr);

		auto descriptorSets = std::vector<VkDescriptorSet>{ descriptorSet };
		_shaderResources._data.try_emplace(layouts[descriptorSetID], descriptorSets);
		return _shaderResources;
	}

	void Camera::UpdateShaderResources()
	{
		auto& globalSettings = Settings::GlobalSettings::Instance();

		_cameraData.worldToCamera = _view._matrix;
		_cameraData.tanHalfHorizontalFov = tan(glm::radians(_horizontalFov / 2.0f));
		_cameraData.aspectRatio = Utils::Converter::Convert<uint32_t, float>(globalSettings._windowWidth) / Utils::Converter::Convert<uint32_t, float>(globalSettings._windowHeight);
		_cameraData.nearClipDistance = _nearClippingDistance;
		_cameraData.farClipDistance = _farClippingDistance;
		_cameraData.transform = _transform.Position();

		memcpy(_buffers[0]._cpuMemory, &_cameraData, sizeof(_cameraData));
	}

	void Camera::Update()
	{
		auto& input = Input::KeyboardMouse::Instance();
		auto& time = Time::Instance();
		auto mouseSens = Settings::GlobalSettings::Instance()._mouseSensitivity;
		_yaw += (float)input._deltaMouseX * mouseSens;

		if ((_pitch + (input._deltaMouseY * mouseSens)) > -90 && (_pitch + (input._deltaMouseY * mouseSens)) < 90) {
			_pitch += (float)input._deltaMouseY * mouseSens;
		}

		if (input.IsKeyHeldDown(GLFW_KEY_Q)) {
			_roll += 0.1f * (float)Time::Instance()._deltaTime;
		}

		if (input.IsKeyHeldDown(GLFW_KEY_E)) {
			_roll -= 0.1f * (float)Time::Instance()._deltaTime;
		}

		if (input.IsKeyHeldDown(GLFW_KEY_W)) {
			_transform.Translate(_transform.Forward() * 0.009f * (float)time._deltaTime);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_A)) {
			_transform.Translate(-_transform.Right() * 0.009f * (float)time._deltaTime);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_S)) {
			_transform.Translate(-_transform.Forward() * 0.009f * (float)time._deltaTime);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_D)) {
			_transform.Translate(_transform.Right() * 0.009f * (float)time._deltaTime);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_SPACE)) {
			_transform.Translate(_transform.Up() * 0.009f * (float)time._deltaTime);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_LEFT_CONTROL)) {
			_transform.Translate(-_transform.Up() * 0.009f * (float)time._deltaTime);
		}

		float _deltaYaw = (_yaw - _lastYaw);
		float _deltaPitch = (_pitch - _lastPitch);
		float _deltaRoll = (_roll - _lastRoll);

		_lastYaw = _yaw;
		_lastPitch = _pitch;
		_lastRoll = _roll;

		// First apply roll rotation.
		_transform.Rotate(_transform.Forward(), _deltaRoll);

		// Transform the up vector according to delta roll but not delta pitch, so the
		// up vector is not affected by looking up and down, but it is by rolling the camera
		// left and right.
		auto axis = _transform.Forward(); // This gets the pitch-independent forward vector.
		auto angleRadians = glm::radians(_deltaRoll);
		auto cosine = cos(angleRadians / 2.0f);
		auto sine = sin(angleRadians / 2.0f);
		_up = glm::quat(cosine, axis.x * sine, axis.y * sine, axis.z * sine) * _up;

		// Then apply yaw rotation.
		_transform.Rotate(_up, _deltaYaw);

		// Then pitch.
		_transform.Rotate(_transform.Right(), _deltaPitch);

		// Vulkan's coordinate system is: X points to the right, Y points down, Z points into the screen.
		// Vulkan's viewport coordinate system is right handed and we are doing all our calculations assuming 
		// +Z is forward, +Y is up and +X is right, so using a left-handed coordinate system.

		// We use the inverse of the camera transformation matrix because this matrix will be applied to each vertex of each object in the scene in the vertex shader.
		// There is no such thing as a camera, because in the end we use the position of the vertices to draw stuff on screen. We can only modify the position of the
		// vertices as if they were viewed from the camera, because in the end the physical position of the vertex ends up in the vertex shader, which then calculates
		// its position on 2D screen using the projection matrix.
		// Suppose that our vertex V is at (0,10,0) (so above your head if you are looking down the z axis from the origin)
		// Also suppose that our camera is at (0,0,-10). This means that the camera is behind us by 10 units and can likely see the object
		// somewhat in the upper area of its screen. If we want to see vertex V as if we viewed it from the camera, we would have to move
		// vertex V 10 units forward from its current position. If instead of moving the camera BACK 10 units we move vertex V FORWARD 10 units, 
		// we get the exact same effect as if we actually had a camera and moved it backwards.
		_view._matrix = glm::inverse(_transform._matrix);

		float _deltaScrollY = ((float)input._scrollY - _lastScrollY);
		_horizontalFov -= _deltaScrollY;
		_lastScrollY = (float)input._scrollY;

		UpdateShaderResources();
	}
}
