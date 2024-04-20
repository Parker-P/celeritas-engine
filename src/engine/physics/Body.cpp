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

		auto vertexToForcePosition = position - _pMesh->_vertices._vertexData[0]._position;
		auto normalizedVertexToForcePosition = glm::normalize(vertexToForcePosition);
		auto scaleFactor = glm::dot(normalizedVertexToForcePosition, force);
		auto effectiveForce = force * scaleFactor;
		auto vertexCount = _pMesh->_vertices._vertexData.size();

		for (int i = 0; i < vertexCount; ++i) {
			auto scaleFactor = -glm::dot(glm::normalize((position - _pMesh->_vertices._vertexData[i]._position)), force);
			auto effectiveForce = force * scaleFactor;
			_forces[i].x = (_forces[i].x + effectiveForce.x) * 0.5f;
			_forces[i].y = (_forces[i].y + effectiveForce.y) * 0.5f;
			_forces[i].z = (_forces[i].z + effectiveForce.z) * 0.5f;

			for (int j = 0; j < vertexCount; ++j) {
				if (i == j) {
					continue;
				}
				auto dst = _pMesh->_vertices._vertexData[i]._position;
				auto src = _pMesh->_vertices._vertexData[j]._position;
				scaleFactor = glm::dot(glm::normalize(dst - src), _forces[i]);
				_forces[j].x = (_forces[j].x + effectiveForce.x) * 0.5f;
				_forces[j].y = (_forces[j].y + effectiveForce.y) * 0.5f;
				_forces[j].z = (_forces[j].z + effectiveForce.z) * 0.5f;
			}
		}


		/*_velocities[i] += _forces[i] * deltaTimeSeconds;
		_pMesh->_vertices._vertexData[i]._position += _velocities[i] * deltaTimeSeconds;*/
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

		/*system("cls");
		std::cout << "Delta time " << (float)time._deltaTime << "\n";
		std::cout << "Physics delta time in seconds " << deltaTimeSeconds << "\n";
		std::cout << "Force " << force << "\n";
		std::cout << "Velocity " << _velocity << "\n";
		std::cout << "Position " << _pMesh->_pGameObject->_localTransform.Position() << "\n";*/
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
		auto pos = _pMesh->_vertices._vertexData[0]._position;
		pos.x += 4;
		AddForceAtPosition(glm::vec3(0.0f, 1.0f, 0.0f), pos);

		//std::cout << "Time " << (float)Time::Instance()._physicsDeltaTime << " velocity " << _velocity.y << "\n";
	}
}
