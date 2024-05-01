#include "LocalIncludes.hpp"

namespace Game
{
	void Falling(Engine::Scenes::GameObject& gameObject)
	{
		auto& instance = gameObject._body;
		if (!instance._isInitialized) {
			instance.Initialize(gameObject._pMesh, 2.5f);
		}

		auto offset = glm::vec3(1.0f, 1.0f, 0.0f);
		auto gravityForce = glm::vec3(0.0f, -9.81f, 0.0f);
		float deltaTimeSeconds = (float)Time::Instance()._physicsDeltaTime * 0.001f;

		auto rotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);
		auto worldSpaceTransform = instance._mesh._pMesh->_pGameObject->GetWorldSpaceTransform();
		auto centerOfMass = instance.GetCenterOfMass();
		auto worldSpaceCom = glm::vec3(worldSpaceTransform._matrix * glm::vec4(centerOfMass, 1.0f));
		auto worldSpaceVertPosition = glm::vec3(worldSpaceTransform._matrix * glm::vec4(instance._mesh._pMesh->_vertices._vertexData[0]._position, 1.0f));
		auto comPerpendicularDirection = glm::normalize(glm::cross(worldSpaceVertPosition - worldSpaceCom, rotationAxis));
		auto rotationalForce = 10.0f * comPerpendicularDirection;

		auto& input = Engine::Input::KeyboardMouse::Instance();
		//AddForce(gravityForce, true);
		instance.AddForceAtPosition(gravityForce, instance._mesh._pMesh->_vertices._vertexData[0]._position, true);
		instance.AddForceAtPosition(gravityForce, instance._mesh._pMesh->_vertices._vertexData[2]._position, true);

		/*if (input.IsKeyHeldDown(GLFW_KEY_UP)) {
			AddForceAtPosition(-rotationalForce, _mesh._pMesh->_vertices._vertexData[0]._position, true);
		}

		if (input.IsKeyHeldDown(GLFW_KEY_DOWN)) {
			AddForceAtPosition(rotationalForce, _mesh._pMesh->_vertices._vertexData[0]._position, true);
		}*/

		instance._mesh._pMesh->_pGameObject->_localTransform.RotateAroundPosition(centerOfMass, glm::normalize(instance._angularVelocity), glm::length(instance._angularVelocity * deltaTimeSeconds));
		instance._mesh._pMesh->_pGameObject->_localTransform.Translate(instance._velocity * deltaTimeSeconds);
	}

	void Ground(Engine::Scenes::GameObject& gameObject)
	{

	}
}
