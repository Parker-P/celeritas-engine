#include <string>
#include <iostream>
#include <filesystem>
#include <vector>
#include <map>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext.hpp>
#include <vulkan/vulkan.h>

#include "structural/IUpdatable.hpp"
#include "structural/Singleton.hpp"
#include "engine/Time.hpp"
#include "settings/GlobalSettings.hpp"
#include "engine/input/Input.hpp"
#include "engine/math/Transform.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/structural/IPipelineable.hpp"
#include "engine/structural/Drawable.hpp"
#include "engine/scenes/Scene.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Mesh.hpp"
#include "engine/scenes/Camera.hpp"
#include <utils/Utils.hpp>

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

	void Camera::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
	{
		using namespace Engine::Vulkan;

		_buffers = Structural::Array<Buffer>(1);
		_descriptors = Structural::Array<Descriptor>(1);
		_sets = Structural::Array<DescriptorSet>(1);

		_buffers[0] = Buffer(logicalDevice,
			physicalDevice,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&_cameraData,
			(size_t)sizeof(_cameraData));

		_descriptors[0] = Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, { &_buffers[0]});
		_sets[0] = DescriptorSet(logicalDevice, VK_SHADER_STAGE_VERTEX_BIT, {&_descriptors[0]});
		_pool = DescriptorPool(logicalDevice, { &_sets[0]});
		_sets[0].SendToGPU();
	}

	void Camera::UpdateShaderResources()
	{
		auto& globalSettings = Settings::GlobalSettings::Instance();

		_cameraData.worldToCamera = _view._matrix;
		_cameraData.tanHalfHorizontalFov = tan(glm::radians(_horizontalFov / 2.0f));
		_cameraData.aspectRatio = Utils::Converter::Convert<uint32_t, float>(globalSettings._windowWidth) / Utils::Converter::Convert<uint32_t, float>(globalSettings._windowHeight);
		_cameraData.nearClipDistance = _nearClippingDistance;
		_cameraData.farClipDistance = _farClippingDistance;

		_buffers[0].UpdateData(&_cameraData, (size_t)sizeof(_cameraData));
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

		/*std::cout << "Up is: " << _up.x << ", " << _up.y << ", " << _up.z << std::endl;
		std::cout << "Axis is: " << axis.x << ", " << axis.y << ", " << axis.z << std::endl;
		std::cout << "Roll is: " << _roll << std::endl;*/

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
