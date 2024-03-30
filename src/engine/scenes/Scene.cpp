#define GLFW_INCLUDE_VULKAN
#include "LocalIncludes.hpp"

namespace Engine::Scenes
{

	Scene::Scene(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice)
	{
		_materials.push_back(Material(logicalDevice, physicalDevice));
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

		for (auto& gameObject : _gameObjects) {
			gameObject.Update();
		}
	}

	Vulkan::ShaderResources Scene::CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<Vulkan::DescriptorSetLayout>& layouts)
	{
		for (auto& gameObject : _gameObjects) {
			if (gameObject._pMesh != nullptr) {
				auto gameObjectResources = gameObject.CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
				auto meshResources = gameObject._pMesh->CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
				_shaderResources.MergeResources(gameObjectResources);
				_shaderResources.MergeResources(meshResources);

				gameObject.UpdateShaderResources();
				gameObject._pMesh->UpdateShaderResources();

				//descriptorSets.insert(descriptorSets.end(), goSets.begin(), goSets.end());
				//descriptorSets.insert(descriptorSets.end(), meshSets.begin(), meshSets.end());
			}
		}

		for (auto& light : _pointLights) {
			auto lightResources = light.CreateDescriptorSets(physicalDevice, logicalDevice, commandPool, queue, layouts);
			_shaderResources.MergeResources(lightResources);
			light.UpdateShaderResources();

			//descriptorSets.insert(descriptorSets.end(), lightSets.begin(), lightSets.end());
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