#pragma once

namespace Engine::Scenes
{
	/**
	 * @brief Represents an infinitesimally small light source.
	 */
	class PointLight : public Scenes::GameObject
	{
	public:

		/**
		 * @brief X, Y, Z represent red, blue and green for light color, while the W component represents
		 * light intensity.
		 */
		glm::vec4 _colorIntensity;
	};
}
