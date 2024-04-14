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

		/**
		 * @brief See Pipelinable.
		 */
		virtual Vulkan::ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<Vulkan::DescriptorSetLayout>& layouts);

		/**
		 * @brief See Pipelinable.
		 */
		virtual void UpdateShaderResources() override;

		void ApplyForce(const glm::vec3& force);

		/**
		* @brief See IUpdatable.
		*/
		virtual void Update(VulkanContext& vkContext) override;

		/**
		 * @brief See IDrawable.
		 */
		virtual void Draw(VkPipelineLayout& pipelineLayout, VkCommandBuffer& drawCommandBuffer) override;
	};
}