#include "LocalIncludes.hpp"

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
		float maximumZ = maximumZ;

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
		auto& vertices = _mesh._pMesh->_vertices._vertexData;
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

	void RigidBody::AddForceAtPosition(const glm::vec3& force, const glm::vec3& pointOfApplication)
	{
		auto& time = Time::Instance();
		float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f;
		auto vertexCount = _mesh._pMesh->_vertices._vertexData.size();
		auto& vertices = _mesh._pMesh->_vertices._vertexData;
		auto worldSpaceTransform = _mesh._pMesh->_pGameObject->GetWorldSpaceTransform()._matrix;

		// First calculate the translation component of the force to apply.
		auto worldSpaceCom = glm::vec3(worldSpaceTransform * glm::vec4(GetCenterOfMass(), 1.0f));
		auto worldSpacePointOfApplication = glm::vec3(worldSpaceTransform * glm::vec4(pointOfApplication, 1.0f));

		glm::vec3 translationForce = CalculateTransmittedForce(worldSpacePointOfApplication, force, worldSpaceCom);
		glm::vec3 translationAcceleration = translationForce / _mass;
		glm::vec3 translationDelta = translationForce * deltaTimeSeconds;
		_velocity += translationDelta;

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
		auto rotationalInertia = _mass;
		//for (int i = 0; i < vertexCount; ++i) {
		//	auto worldSpaceVertexPosition = glm::vec3(worldSpaceTransform * glm::vec4(vertices[i]._position, 1.0f));

		//	if (worldSpaceVertexPosition == worldSpaceCom) {
		//		continue;
		//	}

		//	auto comToVertexDirection = glm::normalize(worldSpaceVertexPosition - worldSpaceCom);
		//	auto cathetus = comToVertexDirection * glm::dot(rotationAxis, comToVertexDirection);

		//	if (IsVectorZero(cathetus), 0.001f) {
		//		rotationalInertia += (/*vertex.mass **/ glm::length(worldSpaceVertexPosition - worldSpaceCom));
		//		continue;
		//	}

		//	auto endPosition = worldSpaceCom + cathetus;
		//	auto perpDistance = glm::length(endPosition - worldSpaceVertexPosition);
		//	rotationalInertia += (/*vertex.mass **/ perpDistance); // Mass still to be accounted for.
		//}

		auto angularAcceleration = glm::cross(rotationalForce, positionToCom) / rotationalInertia;
		auto rotationDelta = angularAcceleration * deltaTimeSeconds;
		_angularVelocity += rotationDelta;
	}

	void RigidBody::AddForce(const glm::vec3& force)
	{
		auto& time = Time::Instance();
		float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f;
		auto translationDelta = (force * deltaTimeSeconds);
		_velocity += translationDelta;
		_mesh._pMesh->_pGameObject->_localTransform.Translate(translationDelta);
	}

	void RigidBody::Initialize(Scenes::Mesh* pMesh, const float& mass)
	{
		if (pMesh == nullptr) {
			return;
		}


		_pMesh = pMesh;

		auto& vertices = _pMesh->_vertices._vertexData;

		for (int i = 0; i < vertices.size(); ++i) {

		}

		/*auto& faceIndices = _pMesh->_faceIndices._indexData;
		for (int i = 0; i < faceIndices.size(); i += 3) {
			auto index1 = faceIndices[i];
			auto index2 = faceIndices[i + 1];
			auto index3 = faceIndices[i + 2];

			if (_neighbors.find(index1) == _neighbors.end()) {
				std::vector<unsigned int> n = { index2, index3 };
				_neighbors.emplace(index1, n);
			}
			else {
				_neighbors[index1].insert(_neighbors[index1].end(), { index2, index3 });
			}

			if (_neighbors.find(index2) == _neighbors.end()) {
				std::vector<unsigned int> n = { index1, index3 };
				_neighbors.emplace(index2, n);
			}
			else {
				_neighbors[index2].insert(_neighbors[index2].end(), { index1, index3 });
			}

			if (_neighbors.find(index3) == _neighbors.end()) {
				std::vector<unsigned int> n = { index1, index2 };
				_neighbors.emplace(index3, n);
			}
			else {
				_neighbors[index3].insert(_neighbors[index3].end(), { index1, index2 });
			}
		}*/

		_isInitialized = true;
	}

	void RigidBody::PhysicsUpdate()
	{
		//AddForce(glm::vec3(0.0f, -0.3f, 0.0f));
		//auto offset = glm::normalize(_pMesh->_vertices._vertexData[0]._position - _pMesh->_vertices._vertexData[1]._position);
		auto offset = glm::vec3(1.0f, 1.0f, 0.0f);
		auto gravityForce = glm::vec3(0.0f, -9.81f, 0.0f);
		float deltaTimeSeconds = (float)Time::Instance()._physicsDeltaTime * 0.001f;
		//offset *= 4;
		//auto pos = _pMesh->_vertices._vertexData[0]._position + offset;

		auto rotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);
		auto worldSpaceTransform = _mesh._pMesh->_pGameObject->GetWorldSpaceTransform();
		auto worldSpaceCom = glm::mat3x3(worldSpaceTransform._matrix) * GetCenterOfMass();
		auto comPerpendicularDirection = glm::normalize(glm::cross(-worldSpaceCom, rotationAxis));
		auto rotationalForce = 2.0f * comPerpendicularDirection;
		//auto pos = _pMesh->_vertices._vertexData[0]._position + offset;

		auto& input = Input::KeyboardMouse::Instance();
		//AddForce(gravityForce);

		if (input.IsKeyHeldDown(GLFW_KEY_UP)) {
			AddForceAtPosition(-gravityForce, _mesh._pMesh->_vertices._vertexData[0]._position);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_DOWN)) {
			AddForceAtPosition(gravityForce, _mesh._pMesh->_vertices._vertexData[0]._position);
		}


		_mesh._pMesh->_pGameObject->_localTransform.Rotate(glm::normalize(_angularVelocity), glm::degrees(glm::length(_angularVelocity * deltaTimeSeconds)));
		_mesh._pMesh->_pGameObject->_localTransform.Translate(_velocity * deltaTimeSeconds);

		/*system("cls");
		auto vert0 = _pMesh->_vertices._vertexData[0]._position;
		auto vert1 = _pMesh->_vertices._vertexData[1]._position;
		auto vert2 = _pMesh->_vertices._vertexData[2]._position;
		std::cout << "Vert 0->1: " << glm::length(vert1 - vert0) << "\n";
		std::cout << "Vert 1->2: " << glm::length(vert2 - vert1) << "\n";
		std::cout << "Vert 2->0: " << glm::length(vert2 - vert0) << "\n";*/

		//std::cout << "Time " << (float)Time::Instance()._physicsDeltaTime << " velocity " << _velocity.y << "\n";
	}
}
