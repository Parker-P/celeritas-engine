#define GLFW_INCLUDE_VULKAN
#include "LocalIncludes.hpp"

namespace Engine::Scenes
{

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

	void Scene::Update()
	{
		for (auto& light : _pointLights) {
			light.Update();
		}

		/*for (auto& gameObject : _gameObjects) {
			gameObject.Update();
		}*/
	}

	Vulkan::ShaderResources Scene::CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<Vulkan::DescriptorSetLayout>& layouts)
	{
		for (auto& gameObject : _pRootGameObject->_pChildren) {
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
}