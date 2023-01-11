#pragma once
namespace Engine::Scenes
{
	/**
	 * @brief Forward declaration used to define a pointer used to keep track of the scene the game object belongs to.
	 */
	class Scene;

	/**
	 * @brief Represents a physical object in a celeritas-engine scene.
	 */
	class GameObject : public IUpdatable
	{
	public:

		/**
		 * @brief Scene this game object belongs to.
		 */
		Scene* _scene;

		/**
		 * @brief Name of the game object.
		 */
		std::string _name;

		/**
		 * @brief Object-to-world transform.
		 */
		Math::Transform _transform;

		/**
		 * @brief Mesh of this game object.
		 */
		Mesh _mesh;

		/**
		 * @brief Contains all game-object-level resources that will go to the shaders.
		 */
		struct
		{
			/**
			 * @brief Descriptor that contains the object-to-world transformation matrix, so the vertex shader knows the positional offset
			 * of this gameobject's mesh's vertices, and can therefore calculate the correct viewable-volume coordinates.
			 */
			Vulkan::Descriptor _transformationData;

			/**
			 * @brief Descriptor set that contains the _transformationData descriptor. This descriptor set will be bound to a graphics
			 * pipeline for each draw call in a render pass that draws this game object's mesh.
			 */
			Vulkan::DescriptorSet _uniformSet;

			/**
			 * @brief Descriptor pool used to allocate _uniformSet.
			 */
			Vulkan::DescriptorPool _setContainer;

		} _shaderResources;

		/**
		 * @brief Per-frame updates.
		 */
		virtual void Update() override;
	};
}