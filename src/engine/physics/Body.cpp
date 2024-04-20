#include "LocalIncludes.hpp"

namespace Engine::Physics
{
	inline std::ostream& operator<< (std::ostream& stream, const glm::vec3& vector)
	{
		return stream << "(" << vector.x << ", " << vector.y << ", " << vector.z << ")";
	}

	void Body::AddForceAtPosition(const glm::vec3& force, const glm::vec3& position)
	{
		auto& time = Time::Instance();
		float deltaTimeSeconds = (float)time._physicsDeltaTime * 0.001f;
		auto vertexCount = _pMesh->_vertices._vertexData.size();

		for (int i = 0; i < vertexCount; ++i) {
			auto dst = _pMesh->_vertices._vertexData[i]._position;
			auto src = position;
			auto effectiveForce = glm::normalize(dst - src);
			auto scaleFactor = glm::dot(effectiveForce, force);
			effectiveForce *= scaleFactor;
			bool forceIsZero = _forces[i] == glm::vec3(0.0f, 0.0f, 0.0f);
			_forces[i] += effectiveForce;
			_forces[i] = forceIsZero ? _forces[i] : _forces[i] * 0.5f;

			for (int j = 0; j < vertexCount; ++j) {
				if (i == j) {
					continue;
				}
				dst = _pMesh->_vertices._vertexData[i]._position;
				src = _pMesh->_vertices._vertexData[j]._position;
				effectiveForce = glm::normalize(dst - src);
				scaleFactor = glm::dot(glm::normalize(dst - src), _forces[i]);
				effectiveForce *= scaleFactor;
				forceIsZero = _forces[j] == glm::vec3(0.0f, 0.0f, 0.0f);
				_forces[j] += effectiveForce;
				_forces[j] = forceIsZero ? _forces[j] : _forces[j] * 0.5f;
			}
		}

		for (int i = 0; i < vertexCount; ++i) {
			_velocities[i] += _forces[i] * deltaTimeSeconds;
			_pMesh->_vertices._vertexData[i]._position += _velocities[i] * deltaTimeSeconds;
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

		_isInitialized = true;
	}

	void Body::PhysicsUpdate()
	{
		//AddForce(glm::vec3(0.0f, -0.3f, 0.0f));
		//auto offset = glm::normalize(_pMesh->_vertices._vertexData[0]._position - _pMesh->_vertices._vertexData[1]._position);
		auto offset = glm::vec3(1.0f, 0.0f, 0.0f);
		//offset *= 4;
		auto pos = _pMesh->_vertices._vertexData[0]._position + offset;
		AddForceAtPosition(glm::vec3(0.0f, 0.3f, 0.0f), pos);

		system("cls");
		auto vert0 = _pMesh->_vertices._vertexData[0]._position;
		auto vert1 = _pMesh->_vertices._vertexData[1]._position;
		auto vert2 = _pMesh->_vertices._vertexData[2]._position;
		std::cout << "Vert 0->1: " << glm::length(vert1-vert0) << "\n";
		std::cout << "Vert 1->2: " << glm::length(vert2-vert1) << "\n";
		std::cout << "Vert 2->0: " << glm::length(vert2-vert0) << "\n";

		//std::cout << "Time " << (float)Time::Instance()._physicsDeltaTime << " velocity " << _velocity.y << "\n";
	}
}
