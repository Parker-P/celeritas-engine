#include <vector>
#include <string>
#include <iostream>
#include <optional>
#include <filesystem>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_vec.hpp>
#include <vulkan/vulkan.h>

#include "utils/Utils.hpp"
#include "settings/Paths.hpp"
#include "structural/IUpdatable.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/Scenes/Vertex.hpp"
#include "engine/structural/Drawable.hpp"
#include "structural/IUpdatable.hpp"
#include "engine/math/Transform.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/structural/IPipelineable.hpp"
#include "engine/scenes/PointLight.hpp"
#include "engine/scenes/CubicalEnvironmentMap.hpp"
#include "engine/scenes/Scene.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/scenes/Mesh.hpp"

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

    void Scene::CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue)
    {
        _environmentMap.LoadFromSphericalHDRI(physicalDevice, logicalDevice, Settings::Paths::TexturesPath() /= "Waterfall.hdr");
        //_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Debug.png");
        //_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "ModernBuilding.hdr");
        //_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Workshop.png");
        //_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "ItalianFlag.png");
        //_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "TestPng.png");
        //_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "EnvMap.png");
        //_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "texture.jpg");
        //_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Test1.png");

        _environmentMap.CreateShaderResources(physicalDevice, logicalDevice, commandPool, graphicsQueue);
        _images.push_back(_environmentMap._cubeMapImage);
        _descriptors.push_back(Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, _images[0], VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 1.0f));
        _sets.push_back(Vulkan::DescriptorSet(logicalDevice, VK_SHADER_STAGE_FRAGMENT_BIT, _descriptors));
        _pool = Vulkan::DescriptorPool(logicalDevice, _sets);
    }

    void Scene::UpdateShaderResources()
    {
        // TODO
    }
}