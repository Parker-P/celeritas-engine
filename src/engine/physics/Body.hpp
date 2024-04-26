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
		 * @brief The velocity vector of this physics body in units per second.
		 */
		glm::vec3 _velocity;

		/**
		 * @brief The angular velocity in radians per second.
		 */
		glm::vec3 _angularVelocity;

		/**
		 * @brief Mesh used to update the physics simulation.
		 */
		Engine::Scenes::Mesh* _pMesh;

		/**
		 * @brief Map where the key is the index of a vertex in _pMesh, and the value is a list of vertex indices directly connected to the vertex represented by the key.
		 */
		std::map<unsigned int, std::vector<unsigned int>> _neighbors;

		/**
		 * @brief Constructor.
		 */
		Body() = default;

		/**
		 * @brief Calculates and returns the center of mass based on the mass of each of its vertices in local space.
		 */
		glm::vec3 GetCenterOfMass();

		/**
		 * @brief Applies a force to the mesh.
		 * @param force The force to be applies.
		 * @param pointOfApplication The position from which the force will be applied to the mesh in local space.
		 */
		void AddForceAtPosition(const glm::vec3& force, const glm::vec3& pointOfApplication);

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

	private:

		/**
		 * @brief Calculates the force that "force" transmits to "receiverPosition" from "transmitterPosition".
		 * Imagine a microscopic scenario where a molecule M1 has an unbreakable bond to another molecule M2.
		 * As soon as M1 moves, it will transmit some force (depending on the direction of movement) to molecule M2.
		 * In this scenario, "transmitterPosition" is the position of M1, "force" represents the movement of M1, and "receiverPosition"
		 * represents M2's position. This function calculates the movement of M2 caused by the movement of M1.
		 */
		glm::vec3 CalculateTransmittedForce(const glm::vec3& transmitterPosition, const glm::vec3& force, const glm::vec3& receiverPosition);
	};
}