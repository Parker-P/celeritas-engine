#pragma once

namespace Engine::Scenes
{


	class SceneLoader
	{
		static std::vector<Material> LoadMaterials(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, tinygltf::Model& gltfScene);

	public:

		/**
		 * @brief Load a scene from file. Scene must be in the .glb file format.
		 */
		static Engine::Scenes::Scene LoadFile(std::filesystem::path filePath, VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& queue);
	};
}
