#pragma once

// Forward declaration for mesh.
namespace Engine::Scenes { class Mesh; }

namespace Engine::Physics
{
	/**
	 * @brief Represents a three-dimensional bounding box.
	 */
	class BoundingBox
	{
	public:

		/**
		 * @brief Low bound, or more accurately, the position whose components are all the lowest number calculated from a collection of positions.
		 */
		glm::vec3 _min;

		/**
		 * @brief High bound, or more accurately, the position whose components are all the highest number calculated from a collection of positions.
		 */
		glm::vec3 _max;

		/**
		 * @brief Returns the center of the bounding box.
		 */
		glm::vec3 GetCenter();

		/**
		 * @brief Creates a bounding box from a visual mesh.
		 */
		static BoundingBox Create(const Scenes::Mesh& mesh);
	};

	/**
	 * @brief Represents a vertex in the mesh of a physics body.
	 */
	class PhysicsVertex
	{
	public:

		/**
		 * @brief Position in local space.
		 */
		glm::vec3 _position;
	};

	/**
	 * @brief Physics mesh.
	 */
	class PhysicsMesh
	{
	public:

		/**
		 * @brief Vertices that form this mesh.
		 */
		std::vector<PhysicsVertex> _vertices;

		/**
		 * @brief Face indices.
		 */
		std::vector<unsigned int> _faceIndices;

		/**
		 * @brief Visual mesh that appears rendered on screen, which this physics mesh class simulates physics for.
		 */
		Scenes::Mesh* _pMesh;
	};

	/**
	 * @brief Base class for a body that performs physics simulation.
	 */
	class RigidBody : public ::Structural::IPhysicsUpdatable
	{
	public:
		
		/**
		 * @brief Whether the body is initialized and valid for simulation.
		 */
		bool _isInitialized = false;

		/**
		 * @brief The velocity vector of this physics body in units per second.
		 */
		glm::vec3 _velocity = glm::vec3(0.0f, 0.0f, 0.0f);

		/**
		 * @brief The angular velocity in radians per second.
		 */
		glm::vec3 _angularVelocity = glm::vec3(0.0f, 0.0f, 0.0f);

		/**
		 * @brief Mass in kg.
		 */
		float _mass;

		/**
		 * @brief If this is set to true, _overriddenCenterOfMass will be used as center of mass.
		 */
		bool _isCenterOfMassOverridden;

		/**
		 * @brief Overridden center of mass in local space.
		 */
		glm::vec3 _overriddenCenterOfMass;

		/**
		 * @brief Physics mesh used as a bridge between this physics body and its visual counterpart.
		 */
		PhysicsMesh _mesh;

		/**
		 * @brief Physics update implementation for this specific rigidbody.
		 */
		void(*_updateImplementation)(Scenes::GameObject&);

		/**
		 * @brief Map where the key is the index of a vertex in _pMesh, and the value is a list of vertex indices directly connected to the vertex represented by the key.
		 */
		//std::map<unsigned int, std::vector<unsigned int>> _neighbors;

		/**
		 * @brief Constructor.
		 */
		RigidBody() = default;

		/**
		 * @brief Returns the center of mass based on the position of each of its vertices in local space.
		 */
		glm::vec3 GetCenterOfMass();

		/**
		 * @brief Applies a force to the mesh.
		 * @param force The force to be applies.
		 * @param pointOfApplication The position from which the force will be applied to the mesh in local space.
		 * @param ignoreTranslation The translation component that the force exerts on the body will be ignored.
		 */
		void AddForceAtPosition(const glm::vec3& force, const glm::vec3& pointOfApplication, bool ignoreTranslation = false);

		/**
		 * @brief Applies a force to the mesh.
		 * @param force The force to be applied.
		 */
		void AddForce(const glm::vec3& force, bool ignoreMass = false);

		std::vector<glm::vec3> GetContactPoints(const RigidBody& other);

		/**
		 * @brief .
		 */
		std::vector<glm::vec3> GetContactPoints();

		/**
		 * @brief Call this before starting the PhysicsUpdate loop.
		 * @param _pMesh
		 */
		void Initialize(Scenes::Mesh* pMesh, const float& mass = 1.0f, const bool& overrideCenterOfMass = false, const glm::vec3& overriddenCenterOfMass = glm::vec3(0.0f));

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