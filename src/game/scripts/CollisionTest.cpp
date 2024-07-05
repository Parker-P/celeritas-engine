#include "LocalIncludes.hpp"
#include "CollisionTest.hpp"

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

		auto gravityForce = glm::vec3(0.0f, -9.81f, 0.0f);
		float deltaTimeSeconds = (float)Time::Instance()._physicsDeltaTime * 0.001f;
		auto centerOfMass = body.GetCenterOfMass();

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
