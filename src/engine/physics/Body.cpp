#include "LocalIncludes.hpp"

namespace Engine::Physics
{
	inline std::ostream& operator<< (std::ostream& stream, const glm::vec3& vector)
	{
		return stream << "(" << vector.x << ", " << vector.y << ", " << vector.z << ")";
	}

	/**
	 * @brief Class that represents the transmission of force from a vertex to another vertex, both in the body's mesh.
	 */
	class TransmittedForce
	{
	public:
		/**
		 * @brief Index of the vertex in the mesh from which the force is transmitted.
		 */
		int _transmitterVertexIndex;

		/**
		 * @brief Index of the vertex in the mesh that receives the force from the transmitter.
		 */
		int _receiverVertexIndex;

		/**
		 * @brief The force transmitted from _sourceVertexIndex.
		 */
		glm::vec3 _force;
	};

	bool IsVectorZero(const glm::vec3& vector, float tolerance = 0.0f)
	{
		return (vector.x >= -tolerance && vector.x <= tolerance &&
			vector.y >= -tolerance && vector.y <= tolerance &&
			vector.z >= -tolerance && vector.z <= tolerance);
	}

	glm::vec3 Body::CalculateTransmittedForce(const glm::vec3& transmitterPosition, const glm::vec3& force, const glm::vec3& receiverPosition)
	{
		if (IsVectorZero(receiverPosition - transmitterPosition, 0.001f)) {
			return force;
		}

		auto effectiveForce = glm::normalize(receiverPosition - transmitterPosition);
		auto scaleFactor = glm::dot(effectiveForce, force);
		return effectiveForce * scaleFactor;
	}

	//void Body::AddForceAtPosition(const glm::vec3& force, const glm::vec3& position)
	//{
	//	auto& time = Time::Instance();
	//	//float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f * /*FOR DEBUG -->*/ 0.1f;
	//	float deltaTimeSeconds = 0.1f;
	//	auto vertexCount = _pMesh->_vertices._vertexData.size();
	//	auto& vertices = _pMesh->_vertices._vertexData;

	//	std::vector<TransmittedForce> transmittedForces;
	//	for (int i = 0; i < vertexCount; ++i) {
	//		auto transmittedForce = CalculateTransmittedForce(position, force, vertices[i]._position);
	//		_forces[i] += transmittedForce;
	//		if (IsVectorZero(transmittedForce)) {
	//			continue;
	//		}
	//		transmittedForces.push_back({ -1, i, transmittedForce });
	//	}

	//	int start = 0;
	//	int end = (int)transmittedForces.size();
	//	int forcesAdded;

	//	for (; start < end;) {
	//		forcesAdded = 0;
	//		auto transmitterIndex = transmittedForces[start]._receiverVertexIndex;
	//		auto receivers = _neighbors[transmitterIndex];
	//		for (auto receiverIndex : receivers) {

	//			// If transmittedForces contains the current receiver as a transmitter vertex, then the force has already been accounted for.
	//			auto res = std::find_if(transmittedForces.begin() + start, transmittedForces.begin() + end, [receiverIndex, transmitterIndex](const TransmittedForce& tf) {
	//				if (tf._transmitterVertexIndex == receiverIndex && tf._receiverVertexIndex == transmitterIndex) { return true; } return false; });

	//			if (res != transmittedForces.end()) {
	//				continue;
	//			}

	//			// Retrieve the last force transmitted where the receiver (in the transmittedForces) was the same as the current transmitter.
	//			auto lastTransmission = std::find_if(transmittedForces.begin() + start, transmittedForces.begin() + end, [transmitterIndex](const TransmittedForce& tf) {
	//				if (tf._receiverVertexIndex == transmitterIndex) { return true; }
	//				else { return false; } });

	//			auto transmittedForce = CalculateTransmittedForce(vertices[transmitterIndex]._position, lastTransmission->_force, vertices[receiverIndex]._position);

	//			if (IsVectorZero(transmittedForce, 0.001f)) {
	//				continue;
	//			}

	//			_forces[receiverIndex] += transmittedForce;
	//			transmittedForces.push_back({ (int)transmitterIndex, (int)receiverIndex, transmittedForce });
	//			++forcesAdded;

	//			end = (int)transmittedForces.size();
	//		}

	//		start = end - forcesAdded;
	//	}

	//	for (int i = 0; i < vertexCount; ++i) {
	//		auto velocityIncrement = (_forces[i] * deltaTimeSeconds);
	//		_velocities[i] += velocityIncrement;
	//		vertices[i]._position += _velocities[i] * deltaTimeSeconds;
	//		_forces[i].x = 0.0f;
	//		_forces[i].y = 0.0f;
	//		_forces[i].z = 0.0f;
	//	}
	//}

	glm::vec3 Body::GetCenterOfMass()
	{
		auto& vertices = _pMesh->_vertices._vertexData;
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

	/*
	* To find the rotation component you need to find an axis of rotation and derive the magnitude of the rotation from the force you want to apply.

The magnitude of rotation (angular velocity) depends on the distance from the point of application to the center of mass, and the rotational inertia.

Start with finding the axis. If:
P = position of application of force
F = force vector
COM = position of the center of mass

You can calculate it like this:
comDirection = COM - P
rotationAxis = Normalize(Cross(comDirection, F))

Now that you have the rotation axis, you need to calculate the magnitude of the force that will cause the angular velocity to change, also known as torque, at the point of application of the force.

To do this you need to figure out what the rotational inertia is, i.e. the resistance to the force trying to start or stop rotation.

The rotational inertia of a single point is its mass times the closest distance from it to the axis of rotation.

Because inertia is an extensive property (a physical property that depends on the size of the object), the total rotational inertia is the sum of the inertia of all points.

In your case, you will exclude the point of application of force, as it could be any point, inside or outside the object.

For rigidbodies, the angular velocity around the center of mass will always be the same for any point of the body.
Same goes for translational velocity; it will always be the same across all points of the body.
This is because rigid bodies cannot deform, so each point of the body is always at the exact same position with respect to the other points, which means that the distance between each point needs to be constant, so they must all move by the exact same amount.

To calculate the rotational inertia of the body, you can do the following:

rotationalInertia = 0
foreach(vertex in vertices) {
	comToVertex = COM - vertex.position
	cathetus = Dot(rotationAxis, comToVertex)
	endPosition = COM + cathetus
	perpDistance = VectorLength(endPosition - vertex.position)
	rotationalInertia += (vertex.mass * perpDistance)
}

Now you know by how much our rotational force is opposed when it's applied.

You also need to know the rotational force.
The rotational force is essentially the part of the force applied to the object that causes it to rotate, which is always perpendicular to the direction to the center of mass from the point on which the force is applied.
We can calculate this as follows:

rotationalForce = F * Dot(Normalize(comDirection), F)

Now you can calculate the angular acceleration.
Angular acceleration, as for regular acceleration, is just the force without taking mass into account.

Torque is just the cross product of the rotational force and the vector that goes from the center of mass to the point of application of the force.
Inertia opposes torque, so to find the acceleration you just divide the torque by the inertia.

angularAcceleration = Cross(rotationalForce, P - COM) / rotationalInertia

Now you add the rotational acceleration (multiplied by delta time) to the angular velocity of the body.

rotationDelta = angularAcceleration * deltaTime
body.angularVelocity += rotationAxis * rotationDelta

To find the new rotation of the body for the current physics update, you just rotate the object around rotationAxis by rotationDelta (which will be in radians) for which you already have a function to do so.
	*/

	void Body::AddForceAtPosition(const glm::vec3& force, const glm::vec3& pointOfApplication)
	{
		auto& time = Time::Instance();
		float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f;
		auto vertexCount = _pMesh->_vertices._vertexData.size();
		auto& vertices = _pMesh->_vertices._vertexData;
		auto worldSpaceTransform = glm::mat3x3(_pMesh->_pGameObject->GetWorldSpaceTransform()._matrix);

		// First calculate the translation component of the force to apply.
		auto worldSpaceCom = worldSpaceTransform * GetCenterOfMass();
		auto worldSpacePointOfApplication = worldSpaceTransform * pointOfApplication;

		glm::vec3 translationForce = CalculateTransmittedForce(worldSpacePointOfApplication, force, worldSpaceCom);
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

		// Calculate rotational inertia.
		auto rotationalInertia = 1.0f;
		for (int i = 0; i < vertexCount; ++i) {
			// This can be greatly simplified with some trigonometry.
			auto worldSpaceVertexPosition = worldSpaceTransform * vertices[i]._position;

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
			rotationalInertia += (/*vertex.mass **/ perpDistance); // Mass still to be accounted for.
		}

		auto angularAcceleration = glm::cross(rotationalForce, positionToCom) / rotationalInertia;
		auto rotationDelta = angularAcceleration * deltaTimeSeconds;
		_angularVelocity += rotationDelta;
	}

	void Body::AddForce(const glm::vec3& force)
	{
		auto& time = Time::Instance();
		float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f;
		auto translationDelta = (force * deltaTimeSeconds);
		_velocity += translationDelta;
		_pMesh->_pGameObject->_localTransform.Translate(translationDelta);
	}

	void Body::Initialize(Engine::Scenes::Mesh* pMesh)
	{
		if (pMesh == nullptr) {
			return;
		}

		_pMesh = pMesh;
		auto& faceIndices = _pMesh->_faceIndices._indexData;

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
		}

		_isInitialized = true;
	}

	void Body::PhysicsUpdate()
	{
		//AddForce(glm::vec3(0.0f, -0.3f, 0.0f));
		//auto offset = glm::normalize(_pMesh->_vertices._vertexData[0]._position - _pMesh->_vertices._vertexData[1]._position);
		auto offset = glm::vec3(1.0f, 1.0f, 0.0f);
		auto gravityForce = glm::vec3(0.0f, -9.81f, 0.0f);
		float deltaTimeSeconds = (float)Time::Instance()._physicsDeltaTime * 0.001f;
		//offset *= 4;
		//auto pos = _pMesh->_vertices._vertexData[0]._position + offset;

		auto rotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);
		auto worldSpaceTransform = _pMesh->_pGameObject->GetWorldSpaceTransform();
		auto worldSpaceCom = glm::mat3x3(worldSpaceTransform._matrix) * GetCenterOfMass();
		auto comPerpendicularDirection = glm::normalize(glm::cross(-worldSpaceCom, rotationAxis));
		auto rotationalForce = 5.0f * comPerpendicularDirection;
		//auto pos = _pMesh->_vertices._vertexData[0]._position + offset;

		auto& input = Input::KeyboardMouse::Instance();
		//AddForce(gravityForce);

		if (input.IsKeyHeldDown(GLFW_KEY_UP)) {
			AddForceAtPosition(rotationalForce, _pMesh->_vertices._vertexData[0]._position);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_DOWN)) {
			AddForceAtPosition(-rotationalForce, _pMesh->_vertices._vertexData[0]._position);
		}


		_pMesh->_pGameObject->_localTransform.Rotate(glm::normalize(_angularVelocity), glm::degrees(glm::length(_angularVelocity * deltaTimeSeconds)));
		_pMesh->_pGameObject->_localTransform.Translate(_velocity * deltaTimeSeconds);

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
