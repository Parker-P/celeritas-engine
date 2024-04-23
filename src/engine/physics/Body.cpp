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

	void Body::AddForceAtPosition(const glm::vec3& force, const glm::vec3& position)
	{
		auto& time = Time::Instance();
		float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f;
		auto vertexCount = _pMesh->_vertices._vertexData.size();
		auto& vertices = _pMesh->_vertices._vertexData;

		std::vector<TransmittedForce> transmittedForces;
		for (int i = 0; i < vertexCount; ++i) {
			auto transmittedForce = CalculateTransmittedForce(position, force, vertices[i]._position);
			if (transmittedForce.x == 0.0f && transmittedForce.y == 0.0f && transmittedForce.z == 0.0f) {
				continue;
			}
			transmittedForces.push_back({ -1, i, transmittedForce });
		}

		int start = 0;
		int end = (int)transmittedForces.size() - 1;
		bool first = true;

	transmissionCalculation:
		for (int i = start;; ++i) {
			auto transmitterIndex = transmittedForces[i]._receiverVertexIndex;
			auto receivers = _neighbors[transmitterIndex];
			for (auto receiverIndex : receivers) {

				// If transmittedForces contains the current neighbor as a transmitter vertex, then the force has already been accounted for.
				auto res = std::find_if(transmittedForces.begin() + start, transmittedForces.begin() + end, [receiverIndex](const TransmittedForce& tf) {
					if (tf._transmitterVertexIndex == receiverIndex) { return true; }
					else { return false; } });

				if (res != transmittedForces.end()) {
					continue;
				}

				// Calculate the last force transmitted where neighbor(in the transmittedForces) is the current vertex
				auto lastTransmission = std::find_if(transmittedForces.begin() + start, transmittedForces.begin() + end, [transmitterIndex](const TransmittedForce& tf) {
					if (tf._receiverVertexIndex == transmitterIndex) { return true; }
					else { return false; } });

				auto transmittedForce = CalculateTransmittedForce(vertices[transmitterIndex]._position, vertices[receiverIndex]._position, lastTransmission->_force);

				if (transmittedForce == glm::vec3(0.0f, 0.0f, 0.0f)) {
					continue;
				}

				_forces[receiverIndex] += transmittedForce;
				transmittedForces.push_back({ transmitterIndex, receiverIndex, transmittedForce });

				if (first) {
					start = (int)transmittedForces.size() - 1;
					first = false;
				}
			}

			if (i == end) {
				end = (int)transmittedForces.size() - 1;
				break;
			}
		}

		if (start != end) {
			// Cleanup the transmittedForces to save space for large meshes.
			goto transmissionCalculation;
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
		
		auto& vertices = _pMesh->_vertices._vertexData;
		auto& faceIndices = _pMesh->_faceIndices._indexData;
		for (int i = 0; i < faceIndices.size(); ++i) {
			auto result = std::find_if(_neighbors.begin(), _neighbors.end(), [i, faceIndices](const std::pair<int, std::vector<int>>& current) {
				if (current.first == faceIndices[i]) {
					return true;
				}
				return false;
				});

			if (result == _neighbors.end()) {
				_neighbors.emplace()
			}
		}

		_isInitialized = true;
	}

	void Body::PhysicsUpdate()
	{
		//AddForce(glm::vec3(0.0f, -0.3f, 0.0f));
		//auto offset = glm::normalize(_pMesh->_vertices._vertexData[0]._position - _pMesh->_vertices._vertexData[1]._position);
		auto offset = glm::vec3(1.0f, 0.0f, 0.0f);
		//offset *= 4;
		auto pos = _pMesh->_vertices._vertexData[0]._position + offset;
		AddForceAtPosition(glm::vec3(0.0f, 1.0f, 0.0f), pos);

		system("cls");
		auto vert0 = _pMesh->_vertices._vertexData[0]._position;
		auto vert1 = _pMesh->_vertices._vertexData[1]._position;
		auto vert2 = _pMesh->_vertices._vertexData[2]._position;
		std::cout << "Vert 0->1: " << glm::length(vert1 - vert0) << "\n";
		std::cout << "Vert 1->2: " << glm::length(vert2 - vert1) << "\n";
		std::cout << "Vert 2->0: " << glm::length(vert2 - vert0) << "\n";

		//std::cout << "Time " << (float)Time::Instance()._physicsDeltaTime << " velocity " << _velocity.y << "\n";
	}
}
