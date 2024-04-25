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


	glm::vec3 Body::CalculateTransmittedForce(const glm::vec3& transmitterPosition, const glm::vec3& force, const glm::vec3& receiverPosition)
	{
		auto effectiveForce = glm::normalize(receiverPosition - transmitterPosition);
		auto scaleFactor = glm::dot(effectiveForce, force);
		return effectiveForce *= scaleFactor;
	}

	bool IsVectorZero(const glm::vec3& vector, float tolerance = 0.0f)
	{
		return (vector.x >= -tolerance && vector.x <= tolerance &&
			vector.y >= -tolerance && vector.y <= tolerance &&
			vector.z >= -tolerance && vector.z <= tolerance);
	}

	void Body::AddForceAtPosition(const glm::vec3& force, const glm::vec3& position)
	{
		auto& time = Time::Instance();
		//float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f * /*FOR DEBUG -->*/ 0.1f;
		float deltaTimeSeconds = 0.1f;
		auto vertexCount = _pMesh->_vertices._vertexData.size();
		auto& vertices = _pMesh->_vertices._vertexData;

		for (int i = 0; i < _forces.size(); ++i) {
			_forces[i] = glm::vec3(0.0f, 0.0f, 0.0f);
		}

		std::vector<TransmittedForce> transmittedForces;
		for (int i = 0; i < vertexCount; ++i) {
			auto transmittedForce = CalculateTransmittedForce(position, force, vertices[i]._position);
			_forces[i] += transmittedForce;
			if (IsVectorZero(transmittedForce)) {
				continue;
			}
			transmittedForces.push_back({ -1, i, transmittedForce });
		}

		int start = 0;
		int end = (int)transmittedForces.size();
		int forcesAdded;

		for (; start < end;) {
			forcesAdded = 0;
			auto transmitterIndex = transmittedForces[start]._receiverVertexIndex;
			auto receivers = _neighbors[transmitterIndex];
			for (auto receiverIndex : receivers) {

				// If transmittedForces contains the current receiver as a transmitter vertex, then the force has already been accounted for.
				auto res = std::find_if(transmittedForces.begin() + start, transmittedForces.begin() + end, [receiverIndex, transmitterIndex](const TransmittedForce& tf) {
					if (tf._transmitterVertexIndex == receiverIndex && tf._receiverVertexIndex == transmitterIndex) { return true; } return false; });

				if (res != transmittedForces.end()) {
					continue;
				}

				// Retrieve the last force transmitted where the receiver (in the transmittedForces) was the same as the current transmitter.
				auto lastTransmission = std::find_if(transmittedForces.begin() + start, transmittedForces.begin() + end, [transmitterIndex](const TransmittedForce& tf) {
					if (tf._receiverVertexIndex == transmitterIndex) { return true; }
					else { return false; } });

				auto transmittedForce = CalculateTransmittedForce(vertices[transmitterIndex]._position, lastTransmission->_force, vertices[receiverIndex]._position);

				if (IsVectorZero(transmittedForce, 0.001f)) {
					continue;
				}

				_forces[receiverIndex] += transmittedForce;
				transmittedForces.push_back({ (int)transmitterIndex, (int)receiverIndex, transmittedForce });
				++forcesAdded;

				end = (int)transmittedForces.size();
			}

			start = end - forcesAdded;
		}

		for (int i = 0; i < vertexCount; ++i) {
			auto velocityIncrement = (_forces[i] * deltaTimeSeconds);
			_velocities[i] += velocityIncrement;
			vertices[i]._position += _velocities[i] * deltaTimeSeconds;
		}
	}

	void Body::AddForce(const glm::vec3& force)
	{
		auto& time = Time::Instance();
		float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f;
		auto velocityIncrement = (force * deltaTimeSeconds);

		for (int i = 0; i < _pMesh->_vertices._vertexData.size(); ++i) {
			_velocities[i] += velocityIncrement;
			_forces[i].x = (_forces[i].x + force.x) * 0.5f;
			_forces[i].y = (_forces[i].y + force.y) * 0.5f;
			_forces[i].z = (_forces[i].z + force.z) * 0.5f;
			_pMesh->_vertices._vertexData[i]._position += _velocities[i] * deltaTimeSeconds;
		}
	}

	void Body::Initialize(Engine::Scenes::Mesh* pMesh)
	{
		if (pMesh == nullptr) {
			return;
		}

		_pMesh = pMesh;
		auto size = _pMesh->_vertices._vertexData.size();
		_velocities.resize(size);
		_forces.resize(size);

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
		//offset *= 4;
		auto pos = _pMesh->_vertices._vertexData[0]._position + offset;

		auto& input = Input::KeyboardMouse::Instance();
		if (input.WasKeyPressed(GLFW_KEY_UP)) {
			AddForceAtPosition(glm::vec3(0.0f, 1.0f, 0.0f), pos);
		}

		if (input.WasKeyPressed(GLFW_KEY_DOWN)) {
			AddForceAtPosition(glm::vec3(0.0f, -1.0f, 0.0f), pos);
		}

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
