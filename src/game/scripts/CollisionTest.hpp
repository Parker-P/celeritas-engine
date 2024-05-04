#include "LocalIncludes.hpp"

namespace Game
{
	using namespace Engine;

	void Falling(Engine::Scenes::GameObject& gameObject)
	{
		auto& body = gameObject._body;
		auto& scene = gameObject._pScene;
		if (!body._isInitialized) {
			body.Initialize(gameObject._pMesh, 2.5f);
		}

		auto offset = glm::vec3(1.0f, 1.0f, 0.0f);
		auto gravityForce = glm::vec3(0.0f, -9.81f, 0.0f);
		float deltaTimeSeconds = (float)Time::Instance()._physicsDeltaTime * 0.001f;

		auto rotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);
		auto worldSpaceTransform = body._mesh._pMesh->_pGameObject->GetWorldSpaceTransform();
		auto centerOfMass = body.GetCenterOfMass();
		auto worldSpaceCom = glm::vec3(worldSpaceTransform._matrix * glm::vec4(centerOfMass, 1.0f));
		auto worldSpaceVertPosition = glm::vec3(worldSpaceTransform._matrix * glm::vec4(body._mesh._pMesh->_vertices._vertexData[0]._position, 1.0f));
		auto comPerpendicularDirection = glm::normalize(glm::cross(worldSpaceVertPosition - worldSpaceCom, rotationAxis));
		auto rotationalForce = 10.0f * comPerpendicularDirection;

		auto& input = Input::KeyboardMouse::Instance();
		gameObject._body.AddForce(gravityForce, true);
		
		// Detect collisions.
		std::vector<glm::vec3> contactPoints = body.GetContactPoints();

		body._mesh._pMesh->_pGameObject->_localTransform.RotateAroundPosition(centerOfMass, glm::normalize(body._angularVelocity), glm::length(body._angularVelocity * deltaTimeSeconds));
		body._mesh._pMesh->_pGameObject->_localTransform.Translate(body._velocity * deltaTimeSeconds);
	}

	void Ground(Engine::Scenes::GameObject& gameObject)
	{

	}
}
