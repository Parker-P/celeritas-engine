#pragma once

namespace Engine::Core::Utils {
	class AssetImporter : public Singleton<AssetImporter> {
	public:
		//Imports a model and returns a Mesh object
		static Engine::Core::Renderer::CustomEntities::Mesh ImportModel(std::string file_name);
	};
}