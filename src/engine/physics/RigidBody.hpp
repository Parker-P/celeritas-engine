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
		 * @brief True if collision detection and resolution is enabled for the body.
		 */
		bool _isCollidable = false;

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

		inline std::ostream& operator<< (std::ostream& stream, const glm::vec3& vector)
		{
			return stream << "(" << vector.x << ", " << vector.y << ", " << vector.z << ")";
		}

		glm::vec3 BoundingBox::GetCenter()
		{
			return glm::vec3((_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f, (_min.z + _max.z) * 0.5f);
		}

		BoundingBox BoundingBox::Create(const Scenes::Mesh& mesh)
		{
			auto& vertices = mesh._vertices._vertexData;
			BoundingBox boundingBox;

			if (vertices.size() <= 0) {
				return boundingBox;
			}

			float minimumX = vertices[0]._position.x;
			float minimumY = vertices[0]._position.y;
			float minimumZ = vertices[0]._position.z;

			float maximumX = minimumX;
			float maximumY = minimumY;
			float maximumZ = minimumZ;

			for (int i = 0; i < vertices.size(); ++i) {
				minimumX = std::min(minimumX, vertices[i]._position.x);
				minimumY = std::min(minimumY, vertices[i]._position.y);
				minimumZ = std::min(minimumZ, vertices[i]._position.z);

				maximumX = std::max(maximumX, vertices[i]._position.x);
				maximumY = std::max(maximumY, vertices[i]._position.y);
				maximumZ = std::max(maximumZ, vertices[i]._position.z);
			}

			boundingBox._min = glm::vec3(minimumX, minimumY, minimumZ);
			boundingBox._max = glm::vec3(maximumX, maximumY, maximumZ);

			return boundingBox;
		}

		bool IsVectorZero(const glm::vec3& vector, float tolerance = 0.0f)
		{
			return (vector.x >= -tolerance && vector.x <= tolerance &&
				vector.y >= -tolerance && vector.y <= tolerance &&
				vector.z >= -tolerance && vector.z <= tolerance);
		}

		glm::vec3 RigidBody::CalculateTransmittedForce(const glm::vec3& transmitterPosition, const glm::vec3& force, const glm::vec3& receiverPosition)
		{
			if (IsVectorZero(receiverPosition - transmitterPosition, 0.001f)) {
				return force;
			}

			auto effectiveForce = glm::normalize(receiverPosition - transmitterPosition);
			auto scaleFactor = glm::dot(effectiveForce, force);
			return effectiveForce * scaleFactor;
		}

		glm::vec3 RigidBody::GetCenterOfMass()
		{
			if (_isCenterOfMassOverridden) {
				return _overriddenCenterOfMass;
			}

			auto& vertices = _mesh._vertices;
			int vertexCount = (int)vertices.size();
			float totalX = 0.0f;
			float totalY = 0.0f;
			float totalZ = 0.0f;

			for (int i = 0; i < vertexCount; ++i) {
				totalX += vertices[i]._position.x;
				totalY += vertices[i]._position.y;
				totalZ += vertices[i]._position.z;
			}

			return glm::vec3(totalX / vertexCount, totalY / vertexCount, totalZ / vertexCount);
		}

		void RigidBody::AddForceAtPosition(const glm::vec3& force, const glm::vec3& pointOfApplication, bool ignoreTranslation)
		{
			auto& time = Time::Instance();
			float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f;
			auto vertexCount = _mesh._pMesh->_vertices._vertexData.size();
			auto& vertices = _mesh._pMesh->_vertices._vertexData;
			auto worldSpaceTransform = _mesh._pMesh->_pGameObject->GetWorldSpaceTransform()._matrix;

			// First calculate the translation component of the force to apply.
			auto worldSpaceCom = glm::vec3(worldSpaceTransform * glm::vec4(GetCenterOfMass(), 1.0f));
			auto worldSpacePointOfApplication = glm::vec3(worldSpaceTransform * glm::vec4(pointOfApplication, 1.0f));

			if (!ignoreTranslation) {
				glm::vec3 translationForce = CalculateTransmittedForce(worldSpacePointOfApplication, force, worldSpaceCom);
				glm::vec3 translationAcceleration = translationForce / _mass;
				glm::vec3 translationDelta = translationForce * deltaTimeSeconds;
				_velocity += translationDelta;
			}

			// Now calculate the rotation component.
			auto positionToCom = worldSpaceCom - worldSpacePointOfApplication;
			if (IsVectorZero(positionToCom, 0.001f)) {
				return;
			}

			auto rotationAxis = -glm::normalize(glm::cross(positionToCom, force));
			if (glm::isnan(rotationAxis.x) || glm::isnan(rotationAxis.y) || glm::isnan(rotationAxis.x)) {
				return;
			}

			auto comPerpendicularDirection = glm::normalize(glm::cross(positionToCom, rotationAxis));
			auto rotationalForce = comPerpendicularDirection * glm::dot(comPerpendicularDirection, force);

			// Calculate or approximate rotational inertia.
			auto rotationalInertia = 0.0f;
			auto singleVertexMass = _mass / vertexCount;
			for (int i = 0; i < vertexCount; ++i) {
				auto worldSpaceVertexPosition = glm::vec3(worldSpaceTransform * glm::vec4(vertices[i]._position, 1.0f));

				if (worldSpaceVertexPosition == worldSpaceCom) {
					continue;
				}

				auto comToVertexDirection = glm::normalize(worldSpaceVertexPosition - worldSpaceCom);
				auto cathetus = comToVertexDirection * glm::dot(rotationAxis, comToVertexDirection);

				if (IsVectorZero(cathetus), 0.001f) {
					rotationalInertia += (/*vertex.mass **/ glm::length(worldSpaceVertexPosition - worldSpaceCom));
					continue;
				}

				auto endPosition = worldSpaceCom + cathetus;
				auto perpDistance = glm::length(endPosition - worldSpaceVertexPosition);
				rotationalInertia += (singleVertexMass * (perpDistance * perpDistance));
			}

			auto angularAcceleration = glm::cross(rotationalForce, positionToCom) / rotationalInertia;
			auto rotationDelta = angularAcceleration * deltaTimeSeconds;
			_angularVelocity += rotationDelta;
		}

		void RigidBody::AddForce(const glm::vec3& force, bool ignoreMass)
		{
			auto& time = Time::Instance();
			float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f;
			auto translationDelta = ignoreMass ? (force * deltaTimeSeconds) : ((force / _mass) * deltaTimeSeconds);
			_velocity += translationDelta;
			_mesh._pMesh->_pGameObject->_localTransform.Translate(translationDelta);
		}

		std::vector<glm::vec3> RigidBody::GetContactPoints(const RigidBody& other)
		{
			std::vector<glm::vec3> outContactPoints;
			auto worldSpaceOther = other._mesh._pMesh->_pGameObject->GetWorldSpaceTransform();
			auto worldSpaceCurrent = _mesh._pMesh->_pGameObject->GetWorldSpaceTransform();

			for (int i = 0; i < other._mesh._faceIndices.size(); i += 3) {
				for (int j = 0; j < _mesh._faceIndices.size(); j += 3) {

					auto v1Other = glm::vec3(worldSpaceOther._matrix * glm::vec4(other._mesh._vertices[other._mesh._faceIndices[i]]._position, 1.0f));
					auto v2Other = glm::vec3(worldSpaceOther._matrix * glm::vec4(other._mesh._vertices[other._mesh._faceIndices[i + 1]]._position, 1.0f));
					auto v3Other = glm::vec3(worldSpaceOther._matrix * glm::vec4(other._mesh._vertices[other._mesh._faceIndices[i + 2]]._position, 1.0f));

					auto v1 = glm::vec3(worldSpaceCurrent._matrix * glm::vec4(_mesh._vertices[_mesh._faceIndices[j]]._position, 1.0f));
					auto v2 = glm::vec3(worldSpaceCurrent._matrix * glm::vec4(_mesh._vertices[_mesh._faceIndices[j + 1]]._position, 1.0f));
					auto v3 = glm::vec3(worldSpaceCurrent._matrix * glm::vec4(_mesh._vertices[_mesh._faceIndices[j + 2]]._position, 1.0f));

					glm::vec3 intersectionPoint1;
					glm::vec3 intersectionPoint2;
					glm::vec3 intersectionPoint3;

					if (Math::IsRayIntersectingTriangle(v1Other, v2Other - v1Other, v1, v2, v3, intersectionPoint1)) {
						outContactPoints.push_back(intersectionPoint1);
					}

					if (Math::IsRayIntersectingTriangle(v1Other, v3Other - v1Other, v1, v2, v3, intersectionPoint2)) {
						outContactPoints.push_back(intersectionPoint2);
					}

					if (Math::IsRayIntersectingTriangle(v3Other, v2Other - v3Other, v1, v2, v3, intersectionPoint3)) {
						outContactPoints.push_back(intersectionPoint3);
					}
				}
			}
			return outContactPoints;
		}

		std::vector< Scenes::GameObject*> GetAllGameObjects(Scenes::GameObject* pRoot)
		{
			std::vector<Scenes::GameObject*> outGameObjects;
			outGameObjects.push_back(pRoot);
			for (int i = 0; i < pRoot->_children.size(); ++i) {
				auto objects = GetAllGameObjects(pRoot->_children[i]);
				outGameObjects.insert(outGameObjects.end(), objects.begin(), objects.end());
			}
			return outGameObjects;
		}

		std::vector<Scenes::GameObject*> GetOtherGameObjects(Scenes::GameObject* pGameObject)
		{
			auto allGameObjects = GetAllGameObjects(pGameObject->_pScene->_pRootGameObject);
			for (int i = 0; i < allGameObjects.size(); ++i) {
				if (pGameObject == allGameObjects[i]) {
					allGameObjects.erase(allGameObjects.begin() + i);
				}
			}
			return allGameObjects;
		}

		std::vector<glm::vec3> RigidBody::GetContactPoints()
		{
			auto otherGameObjects = GetOtherGameObjects(_mesh._pMesh->_pGameObject);
			std::vector<glm::vec3> outContactPoints;
			for (int i = 0; i < otherGameObjects.size(); ++i) {
				if (otherGameObjects[i]->_body._mesh._pMesh == nullptr) {
					continue;
				}
				auto contactPoints = GetContactPoints(otherGameObjects[i]->_body);
				outContactPoints.insert(outContactPoints.end(), contactPoints.begin(), contactPoints.end());
			}
			return outContactPoints;
		}

		void RigidBody::Initialize(Scenes::Mesh* pMesh, const float& mass, const bool& overrideCenterOfMass, const glm::vec3& overriddenCenterOfMass)
		{
			if (pMesh == nullptr || mass <= 0.001f) {
				return;
			}

			_mass = mass;
			_mesh._pMesh = pMesh;
			_isCenterOfMassOverridden = overrideCenterOfMass;
			_overriddenCenterOfMass = overriddenCenterOfMass;
			auto& vertices = pMesh->_vertices._vertexData;
			auto& indices = pMesh->_faceIndices._indexData;
			_mesh._vertices.resize(vertices.size());
			_mesh._faceIndices.resize(indices.size());

			// TODO: Decide how you want to initialize your physics mesh.
			for (int i = 0; i < vertices.size(); ++i) {
				_mesh._vertices[i]._position = vertices[i]._position;
			}

			memcpy(_mesh._faceIndices.data(), indices.data(), Utils::GetVectorSizeInBytes(indices));

			_isInitialized = true;
		}

		void RigidBody::PhysicsUpdate()
		{
			if (_isCollidable) {

			}

			if (_updateImplementation) {
				_updateImplementation(*_mesh._pMesh->_pGameObject);
			}

			float deltaTimeSeconds = (float)Time::Instance()._physicsDeltaTime * 0.001f;
			_mesh._pMesh->_pGameObject->_localTransform.RotateAroundPosition(GetCenterOfMass(), glm::normalize(_angularVelocity), glm::length(_angularVelocity) * deltaTimeSeconds);
			_mesh._pMesh->_pGameObject->_localTransform.Translate(_velocity * deltaTimeSeconds);
		}

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