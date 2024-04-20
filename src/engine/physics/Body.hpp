#pragma once

// Forward declaration for mesh.
namespace Engine::Scenes { class Mesh; }

namespace Engine::Physics
{
	/**
	 * @brief Base class for a body that performs physics simulation.
	 */
	class Body : public ::Structural::IPhysicsUpdatable
	{
	public:
		
		/**
		 * @brief Whether the body is initialized and valid for simulation.
		 */
		bool _isInitialized = false;

		/**
		 * @brief The velocity vector of each vertex of the mesh of this physics body in units per second.
		 */
		std::vector<glm::vec3> _velocities;

		/**
		 * @brief The force acting on each vertex of the mesh at a specific point in time in Newtons.
		 */
		std::vector<glm::vec3> _forces;

		/**
		 * @brief Mesh used to update the physics simulation.
		 */
		Engine::Scenes::Mesh* _pMesh;

		/**
		 * @brief .
		 */
		Body() = default;

		/**
		 * @brief Applies a force to the mesh.
		 * @param force The force to be applies.
		 * @param position The position from which the force will be applied to the mesh in local space.
		 */
		void AddForceAtPosition(const glm::vec3& force, const glm::vec3& position);

		/**
		 * @brief Applies a force to the mesh.
		 * @param force The force to be applies.
		 */
		void AddForce(const glm::vec3& force);

		/**
		 * @brief Call this before starting the PhysicsUpdate loop.
		 * @param _pMesh
		 */
		void Initialize(Engine::Scenes::Mesh* pMesh);

		/**
		 * @brief See IPhysicsUpdatable.
		 */
		void PhysicsUpdate() override;
	};
}