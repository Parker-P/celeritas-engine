#include "LocalIncludes.hpp"
#include "engine/math/MathUtils.hpp"

namespace Engine::Physics
{
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

	std::vector<glm::vec3> GetContactPoints(const RigidBody& other, const RigidBody& current) 
	{
		std::vector<glm::vec3> outContactPoints;
		auto worldSpaceOther = other._mesh._pMesh->_pGameObject->GetWorldSpaceTransform();
		auto worldSpaceCurrent = current._mesh._pMesh->_pGameObject->GetWorldSpaceTransform();

		for (int i = 0; i < other._mesh._faceIndices.size(); i+=3) {
			for (int j = 0; j < current._mesh._faceIndices.size(); j+=3) {
				glm::vec3 intersectionPoint1;
				glm::vec3 intersectionPoint2;
				glm::vec3 intersectionPoint3;
				auto originE1Other = other._mesh._vertices[other._mesh._faceIndices[i]];
				auto originE2Other = other._mesh._vertices[other._mesh._faceIndices[i]];
				auto originE3Other = other._mesh._vertices[other._mesh._faceIndices[i]];
			}
		}
		return outContactPoints;
	}

	std::vector<glm::vec3> RecursivelyDetectCollisions(const Scenes::GameObject& root, const RigidBody& current)
	{
		std::vector<glm::vec3> outContactPoints;
		auto& body = root._body;
		if (body._isInitialized && body._mesh._vertices.size() > 2) {
			auto contactPoints = GetContactPoints(body, current);
			outContactPoints.insert(outContactPoints.end(), contactPoints.begin(), contactPoints.end());
		}
		return outContactPoints;
	}

	std::vector<glm::vec3> RigidBody::GetContactPoints()
	{
		std::vector<glm::vec3> outContactPoints;
		auto contactPoints = RecursivelyDetectCollisions(*_mesh._pMesh->_pGameObject->_pScene->_pRootGameObject, *this);
		outContactPoints.insert(outContactPoints.end(), contactPoints.begin(), contactPoints.end());
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

		// TODO: Decide how you want to initialize your physics mesh.
		for (int i = 0; i < vertices.size(); ++i) {
			_mesh._vertices[i]._position = vertices[i]._position;
		}

		memcpy(_mesh._faceIndices.data(), indices.data(), Utils::GetVectorSizeInBytes(indices));

		_isInitialized = true;
	}

	void RigidBody::PhysicsUpdate()
	{
		if (_updateImplementation) {
			_updateImplementation(*_mesh._pMesh->_pGameObject);
		}

		float deltaTimeSeconds = (float)Time::Instance()._physicsDeltaTime * 0.001f;
		_mesh._pMesh->_pGameObject->_localTransform.RotateAroundPosition(GetCenterOfMass(), glm::normalize(_angularVelocity), glm::length(_angularVelocity) * deltaTimeSeconds);
		_mesh._pMesh->_pGameObject->_localTransform.Translate(_velocity * deltaTimeSeconds);
	}
}
