#include <glm/glm.hpp>
#include "MathUtils.hpp"

namespace Engine::Math
{
	bool IsRayIntersectingTriangle(glm::vec3 rayOrigin, glm::vec3 rayVector, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, glm::vec3& outIntersectionPoint)
	{
		/**
		 * This is an implementation of the Trumbore-Moller ray-triangle intersection algorithm, source paper on the study is under the docs folder.
		 *
		 * In summary, the algorithm is based around the idea of creating a transformation matrix to create a new ad-hoc coordinate system so it's easier
		 * to calculate the point of intersection. In code, this is not apparent, as the algorithm also uses some careful and clever optimizations based on
		 * linear algebra concepts after some careful observations that Trumbore and Moller made when analyzing the math involved.
		 * The paper shows how these optimizations were derived at section 2.5 and 2.6.
		 *
		 * After an initial implementation I made myself from their theory, I decided to take their implementation directly, as it uses some extra optimizations
		 * as described above.
		 * This implementation is copy-pasted from Wikipedia, but it's pretty much exactly the same as the one in the original paper.
		 * 
		 * The only addition to make the function more useful would be to add the ability to choose whether to ignore back-facing triangles.
		 */

		constexpr float epsilon = std::numeric_limits<float>::epsilon();

		glm::vec3 edge1 = v2 - v1;
		glm::vec3 edge2 = v3 - v1;

		glm::vec3 rayCrossE2 = glm::cross(rayVector, edge2);
		float determinant = dot(edge1, rayCrossE2);

		if (determinant > -epsilon && determinant < epsilon)
			return false;    // This ray is parallel to this triangle.

		float inverseDeterminant = 1.0f / determinant;
		glm::vec3 s = rayOrigin - v1;
		float u = inverseDeterminant * dot(s, rayCrossE2);

		if (u < 0 || u > 1)
			return false;

		glm::vec3 sCrossE1 = cross(s, edge1);
		float v = inverseDeterminant * glm::dot(rayVector, sCrossE1);

		if (v < 0 || u + v > 1)
			return false;

		// At this stage we can compute t to find out where the intersection point is along the ray.
		float t = inverseDeterminant * dot(edge2, sCrossE1);

		if (t > epsilon)
		{
			outIntersectionPoint = rayOrigin + rayVector * t;
			return true;
		}
		else // This means that the origin of the ray is inside the triangle.
			return false;
	}
}
