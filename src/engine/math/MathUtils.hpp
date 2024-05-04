#pragma once

namespace Engine::Math
{
	/**
	 * @brief Returns true if rayVector intersects the plane formed by the triangle formed by v1, v2, v3 and the intersection point falls within said triangle.
	 * @param rayOrigin The origin point of the ray, in local space.
	 * @param rayVector The ray vector. Can be normalized or not.
	 * @param v1 Coordinates of vertex 1 in local space.
	 * @param v2 Coordinates of vertex 2 in local space.
	 * @param v3 Coordinates of vertex 3 in local space.
	 * @param outIntersectionPoint Intersection point in local space. This will remain unchanged if the function returns false.
	 */
	bool IsRayIntersectingTriangle(glm::vec3 rayOrigin, glm::vec3 rayVector, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, glm::vec3& outIntersectionPoint);
}