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
#include "utils/BoxBlur.hpp"
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
        _images.push_back(_environmentMap._cubeMapImage);

        auto environmentMapDescriptor = Vulkan::Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, _images[0], VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.0f, 0.0f, 9.0f, VK_SAMPLER_MIPMAP_MODE_LINEAR);
        _descriptors.push_back(environmentMapDescriptor);

        auto set = Vulkan::DescriptorSet(logicalDevice, VK_SHADER_STAGE_FRAGMENT_BIT, _descriptors);
        _sets.push_back(set);

        _pool = Vulkan::DescriptorPool(logicalDevice, _sets);
    }

    void Scene::UpdateShaderResources()
    {
        // TODO
    }
}