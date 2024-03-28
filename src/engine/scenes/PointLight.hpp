#pragma once

namespace Engine::Scenes
{
	/**
	 * @brief Represents an infinitesimally small light source.
	 */
	class PointLight : public ::Structural::IUpdatable, public Engine::Structural::IPipelineable
	{
	public:

		/**
		 * @brief Name of the light.
		 */
		std::string _name;

		/**
		 * @brief .
		 */
		Math::Transform _transform;

		/**
		 * @brief Data to be sent to the shaders.
		 */
		struct {
			glm::vec3 position;
			glm::vec4 colorIntensity;
		} _lightData;

		/**
		 * @brief X, Y, Z represent red, blue and green for light color, while the W component represents
		 * light intensity.
		 */
		glm::vec4 _colorIntensity;

		/**
		 * @brief Default constructor.
		 */
		PointLight() = default;
		
		/**
		 * @brief .
		 * @param name
		 */
		PointLight(std::string name);

		/**
		 * @brief See IPipelinable
		 */
		virtual Vulkan::ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& graphicsQueue, std::vector<Vulkan::DescriptorSetLayout>& layouts) override;

		/**
		 * @brief See IPipelinable
		 */
		virtual void UpdateShaderResources() override;

		/**
		 * @brief See IUpdatable.
		 */
		void Update() override;
	};
}
