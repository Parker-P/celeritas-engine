#include <glm/glm.hpp>
#include "MathUtils.hpp"

namespace Engine::Math
{
	bool IsRayIntersectingTriangle(glm::vec3 rayOrigin, glm::vec3 rayVector, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, glm::vec3& outIntersectionPoint)
	{
		/**
		 * This is an implementation of the Trumbore-Moller ray-triangle intersection algorithm, source is under the docs folder.
		 *
		 * Say that you have a triangle v1, v2, v3, and you want to know if a vector R with origin O intersects said triangle.
		 *
		 * You can solve this with vector math alone, but it becomes more computationally expensive than solving using triangle-space coordinates (a.k.a. barycentric coordinates)
		 * because it involves normalizing vectors, which involves square roots that take many CPU cycles to calculate.
		 *
		 * The Moller-Trumbore algorithm gets around computing square roots, and uses the smart approach of defining, via equations, the desired result, and then
		 * using whatever mathematically correct method allows the resolution of the unknowns in the equation.
		 *
		 * So this is the way they approached the problem:
		 * they observed that any point P on the plane formed by v1, v2, v3 can be represented by a linear combination of 2 edges of the triangle, the same
		 * way we define cartesian coordinates in 2D space with the X and Y axes, except that now the X and Y axes are replaced by 2 edges of the triangle.
		 *
		 * So essentially, it's like treating the 2 edges as brand new cartesian axes, effectively defining a new coordinate system, where 1 unit in each axis
		 * is the length of the respective edge of the triangle chosen to be said axis. We will call this coordinate system "triangle space" from now on.
		 *
		 * This means that, in triangle space, if the X axis is edge e2 (v2 - v1) and the Y axis is edge e3 (v3 - v1) the origin (point 0,0) is the vertex that connects
		 * the 2 edges chosen as axes, in this case v1. It also means that point (1,0) in triangle space is equal to v2 in local space, and point (0,1) in triangle space is v3 in local space.
		 *
		 * This means that, in triangle space, any coordinate with X and Y both greater than zero and less than 1 is a point in the triangle.
		 *
		 * They also obviously noticed that the point of intersection P, in local space, will always be the ray scaled by some unknown value.
		 * If they could find that unknown value, they would be able to know exactly by how much to scale the ray so it points to the intersection position P.
		 *
		 * So, summarizing, point P can be represented both by a linear combination of 2 edges, that we call "triangle space", and the ray R scaled by some unknown scalar value.
		 *
		 * Trumbore and Moller, therefore, came up with the following equation, and used the above concepts to solve it:
		 * Point P = v1 + (w2 * e2) + (w3 * e3)
		 * where:
		 * All positions and vectors are defined in local space.
		 * P is the unknown position of intersection,
		 * v1 is the position of vertex 1,
		 * e2 is the vector (in local space) that goes from vertex 1 to vertex 2, which represents the X axis in triangle space,
		 * e3 is the vector (in local space) that goes from vertex 1 to vertex 3, which represents the Y axis in triangle space,
		 * w2 (stands for weight 2) is the unknown number that represents the translation amount along the X axis, in triangle space, to reach point P,
		 * w3 (stands for weight 3) is the unknown number that represents the translation amount along the Y axis, in triangle space, to reach point P.
		 *
		 * Now they started substituting/rearranging the values in the equation to make it easier to solve for the 3 unknowns P, w2, w3.
		 *
		 * First and most obvious is to substitute P with (O + s * R) where:
		 * O is the origin of the ray,
		 * R is the ray vector,
		 * s is the unknown scalar value by which to scale R so it points to point P.
		 *
		 * This simplifies P as unknown because now it's not the entirety of P that is unknown, it's just the scalar value by which to multiply the ray vector
		 * to reach point P.
		 *
		 * So now the equation becomes:
		 * (O + s * R) = v1 + (w2 * e2) + (w3 * e3)
		 *
		 * They now still had 3 unknowns, of which one simpler: the scalars for the triangle-space coordinates (w2 and w3) and the scalar for ray R (s).
		 * If they could solve this equation, they could use w2 and w3 to know whether the point is inside the triangle, and s to calculate the intersection position.
		 *
		 * To simplify this further, they decomposed this equation into 3 equations by rewriting each vector quantity into its individual components (x,y,z),
		 * which would allow them to have a system of linear equations that is easily solvable with already-known straightforward methods, such as Cramer's rule.
		 *
		 * Decomposed, this becomes:
		 * (Ox + s * Rx) = v1x + (w2 * e2x) + (w3 * e3x)
		 * (Oy + s * Ry) = v1y + (w2 * e2y) + (w3 * e3y)
		 * (Oz + s * Rz) = v1z + (w2 * e2z) + (w3 * e3z)
		 *
		 * Notice how each equation contains only one component of each vector quantity, and the unknown scalar values are the same in each equation,
		 * because a scalar affects all components of a vector equally, so the equations are still valid.
		 *
		 * By following the matrix multiplcation rules in reverse, they came up with the following, equivalent, matrix multiplication equation:
		 * | e2x  e3x  Rx |   | w2 |   |v1x - Ox|
		 * | e2y  e3y  Ry | * | w3 | = |v1y - Oy|
		 * | e2z  e3z  Rz |   | s  |   |v1z - Oz|
		 *
		 * They came up with a system of equations as a matrix multiplication, such that one matrix contains only unknowns, so they could resolve for the unknowns
		 * by substitution, like Ax = b, where x is the only unknown. This means that x = b / A.
		 *
		 * Obviously dividing by a matrix is not a defined operation, but it is possible to multiply by its inverse.
		 * The equation, therefore becomes:
		 * x = inverse(A) * b (order switched because glm uses column-major order for matrix multiplication).
		 *
		 * Lets write it down:
		 * | w2 |           | e2x  e3x  Rx |    |v1x - Ox|
		 * | w3 | = inverse(| e2y  e3y  Ry |) * |v1y - Oy|
		 * | s  |           | e2z  e3z  Rz |    |v1z - Oz|
		 *
		 * Once this is solved, the triangle space coordinates are found, along with the scalar value by which to scale ray R to reach intersection point P.
		 * It is now possible to convert triangle space coordinates into local space coordinates to get the position of intersection.
		 *
		 * In this specific case, all calculations are done via glm, which performs calculations assuming we use a right-handed coordinate system, so we just switch the
		 * X component of the result to be negated.
		 */

		constexpr float epsilon = std::numeric_limits<float>::epsilon();

		glm::vec3 edge2 = v2 - v1;
		glm::vec3 edge3 = v3 - v1;
		// This represents the incidence of the ray on the plane formed by the triangle. 
		float incidence = glm::dot(rayVector, glm::cross(edge2, edge3));

		// If the incidence is 0, or very close to 0, it means the ray is parallel/very close to parallel to the plane, which means that the ray will never intersect the plane, so we just return.
		if (incidence > -epsilon && incidence < epsilon)
			return false;    // This ray is parallel to this triangle.

		// Construct inverse(A) in the "x = inverse(A) * b" equation.
		glm::mat3x3 inverseA = glm::inverse(glm::mat3x3(
			edge2.x, edge2.y, edge2.z,				// Column 1
			edge3.x, edge3.y, edge3.z,				// Column 2
			rayVector.x, rayVector.y, rayVector.z	// Column 3
		));

		// Construct b in the "x = inverse(A) * b" equation.
		glm::vec3 b = glm::vec3(v1.x - rayOrigin.x, v1.y - rayOrigin.y, v1.z - rayOrigin.z);

		// Compute the result.
		auto result = inverseA * b;

		// Make sure the triangle space coordinates fall into the 0-1 domain.
		if (-result.x < 0 || -result.x > 1)
			return false; // Triangle-space X axis coordinate is outside the triangle.

		if (-result.y < 0 || -result.y > 1)
			return false; // Triangle-space Y axis coordinate is outside the triangle.

		// Return the intersection point.
		outIntersectionPoint = (rayOrigin + (result.z * rayVector));
		return true;
	}
}
