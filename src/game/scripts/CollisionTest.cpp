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
		// Rules for implementation:
		// Overview:
		// Each body stores the forces applied on it the previous frame, so if a collision happens, it can be "moved back in time" to a point when
		// it wasn't colliding (but was very close).
		// 
		// If there's a collision between 2 moving bodies, the body with the lowest mass gets moved "back in time" (to the previous physics update when it wasn't colliding).
		// If 2 bodies have the same mass, the body with the highest speed gets moved back.
		// If 2 bodies have the same mass and the same speed, either of them gets moved back.
		// 
		// If 2 bodies are stationary, the one with the lowest mass is moved in the direction of the normal of the face of the object with the bigger mass.
		// If 2 bodies are stationary and have the same mass, the above behaviour applies to either of them.
		// 
		// If one body is stationary or immovable, and the other is moving, the one that's moving gets moved back, regardless of mass.
		// 
		// Collision between multiple bodies:
		// All bodies involved in the collisions are sorted by mass.
		// Each collision is resolved between the 2 objects with the lowest mass using the rules for collisions between 2 bodies described above.
		// Once resolved, the object that was moved back is discarded from the list of collisions to resolve, and the process is repeated again until
		// all collisions are resolved.
		std::vector<glm::vec3> contactPoints = body.GetContactPoints();

		body._mesh._pMesh->_pGameObject->_localTransform.RotateAroundPosition(centerOfMass, glm::normalize(body._angularVelocity), glm::length(body._angularVelocity * deltaTimeSeconds));
		body._mesh._pMesh->_pGameObject->_localTransform.Translate(body._velocity * deltaTimeSeconds);
	}

	void Ground(Engine::Scenes::GameObject& gameObject)
	{

	}
}
