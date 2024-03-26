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
#include "LocalIncludes.hpp"

namespace Engine::Scenes
{

	Scene::Scene(VkDevice& logicalDevice, Vulkan::PhysicalDevice physicalDevice)
	{
		_materials.push_back(Material(logicalDevice, physicalDevice));
	}

	Material Scene::DefaultMaterial()
	{
		if (_materials.size() <= 0) {
			std::cout << "there is a problem... a scene object should always have at least a default material" << std::endl;
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

	Vulkan::ShaderResources Scene::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
	{
		for (auto& gameObject : _gameObjects) {
			if (gameObject._pMesh != nullptr) {
				auto gameObjectResources = gameObject.CreateShaderResources(physicalDevice, logicalDevice, commandPool, graphicsQueue);
				auto meshSets = gameObject._pMesh->CreateShaderResources(physicalDevice, logicalDevice, commandPool, graphicsQueue);

				gameObject.UpdateShaderResources();
				gameObject._pMesh->UpdateShaderResources();

				descriptorSets.insert(descriptorSets.end(), goSets.begin(), goSets.end());
				descriptorSets.insert(descriptorSets.end(), meshSets.begin(), meshSets.end());
			}
		}

		for (auto& light : _pointLights) {
			auto lightSets = light.CreateShaderResources(_physicalDevice, _logicalDevice, _commandPool, _queue);
			light.UpdateShaderResources();
			descriptorSets.insert(descriptorSets.end(), lightSets.begin(), lightSets.end());
		}

		_environmentMap = CubicalEnvironmentMap(physicalDevice, logicalDevice);
		//_environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Waterfall.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Debug.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "ModernBuilding.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Workshop.png");
		//_environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Workshop.hdr");
		_environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "garden.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "ItalianFlag.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "TestPng.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "EnvMap.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "texture.jpg");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Test1.png");

		_environmentMap.CreateShaderResources(physicalDevice, logicalDevice, commandPool, graphicsQueue);
		return _environmentMap._shaderResources;
		
	}

	void Scene::UpdateShaderResources()
	{
		// TODO
	}
}