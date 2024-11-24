#pragma once

namespace Engine::Scenes
{
	// Forward declarations for the compiler.
	class GameObject;
	
	/**
	 * @brief Represents a celeritas-engine scene.
	 */
	class Scene : public Structural::IVulkanUpdatable, public Structural::IPipelineable
	{

	public:
		/**
		 * @brief Collection of point lights.
		 */
		std::vector<PointLight> _pointLights;

		/**
		 * @brief Game object hierarchy.
		 */
		GameObject* _pRootGameObject;

		/**
		 * @brief Collection of materials.
		 */
		std::vector<Material> _materials;

		/**
		 * @brief Environment map used for image-based lighting.
		 */
		CubicalEnvironmentMap _environmentMap;

		/**
		 * @brief Default constructor.
		 */
		Scene() = default;

		Scene::Scene(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice)
		{
			_materials.push_back(Material(logicalDevice, physicalDevice));
			_pRootGameObject = new GameObject("Root", this);
		}

		Material Scene::DefaultMaterial()
		{
			if (_materials.size() <= 0) {
				std::cout << "a scene object should always have at least a default material" << std::endl;
				std::exit(1);
			}
			return _materials[0];
		}

		void Scene::Update(VulkanContext& vkContext)
		{
			for (auto& light : _pointLights) {
				light.Update(vkContext);
			}

			for (auto& gameObject : _pRootGameObject->_children) {
				gameObject->Update(vkContext);
			}
		}

		Vulkan::ShaderResources Scene::CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<Vulkan::DescriptorSetLayout>& layouts)
		{
			for (auto& gameObject : _pRootGameObject->_children) {
				if (gameObject->_pMesh != nullptr) {
					auto gameObjectResources = gameObject->CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
					_shaderResources.MergeResources(gameObjectResources);
				}
			}

			for (auto& light : _pointLights) {
				auto lightResources = light.CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
				_shaderResources.MergeResources(lightResources);
				light.UpdateShaderResources();
			}

			auto environmentMapResources = _environmentMap.CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
			_shaderResources.MergeResources(environmentMapResources);
			return _shaderResources;
		}

		void Scene::UpdateShaderResources()
		{
			// TODO
		}
	};
}