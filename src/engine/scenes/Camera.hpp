#pragma once

namespace Engine::Scenes
{
	// Forward declarations for the compiler.
	//class Scene;

	/**
	 * @brief Represents a general-purpose camera.
	 */
	class Camera : public GameObject
	{
	public:

		/**
		 * @brief Horizontal FOV in degrees.
		 */
		float _horizontalFov;

		/**
		 * @brief Lower bound that maps to normalizedDeviceCoordinates.z = 0 in the vertex shader, in meters.
		 * Anything closer than this will not be rendered by the graphics pipeline.
		 */
		float _nearClippingDistance;

		/**
		 * @brief Upper bound that maps to normalizedDeviceCoordinates.z = 1 in the vertex shader, in meters.
		 * Anything farther than this will not be rendered by the graphics pipeline.
		 */
		float _farClippingDistance;

		/**
		 * @brief This is a transform passed to the vertex shader that translates vertices from world space into camera space.
		 */
		Math::Transform _view;

		/**
		 * @brief Up direction of the camera.
		 */
		glm::vec3 _up;

		// Temp
		float _lastYaw;
		float _lastPitch;
		float _lastRoll;

		float _yaw;
		float _pitch;
		float _roll;

		float _lastScrollY;

		/**
		 * @brief Camera related data directed to the vertex shader. Knowing this, the vertex shader is able to calculate the correct
		 * Vulkan view volume coordinates.
		 */
		struct
		{
			float tanHalfHorizontalFov;
			float aspectRatio;
			float nearClipDistance;
			float farClipDistance;
			glm::mat4 worldToCamera;
			glm::vec3 transform;
		} _cameraData;

		/**
		 * @brief Default constructor.
		 */
		Camera();

		/**
		 * @brief Constructor.
		 * @param horizontalFov Horizontal FOV in degrees.
		 * @param nearClippingDistance Clipping distance in engine units, which are meters.
		 * @param farClippingDistance Far clipping distance in engine units, which are meters.
		 */
		Camera(float horizontalFov, float nearClippingDistance, float farClippingDistance);

		/**
		 * @brief See IPipelinable.
		 */
		virtual Vulkan::ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<Vulkan::DescriptorSetLayout>& layouts) override;

		/**
		 * @brief See IPipelinable.
		 */
		virtual void UpdateShaderResources() override;

		/**
		 * @brief See IUpdatable.
		 */
		void Update() override;
	};
}