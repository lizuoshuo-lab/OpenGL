#include "frustum.h"

#include <cmath>
#include <limits>

namespace {
glm::vec4 matrixRow(const glm::mat4& matrix, int row) {
	return glm::vec4(
		matrix[0][row],
		matrix[1][row],
		matrix[2][row],
		matrix[3][row]
	);
}

glm::vec4 normalizePlane(const glm::vec4& plane) {
	const float length = glm::length(glm::vec3(plane));
	return length > std::numeric_limits<float>::epsilon() ? plane / length : plane;
}
}

Frustum::Frustum(const glm::mat4& viewProjection) {
	update(viewProjection);
}

void Frustum::update(const glm::mat4& viewProjection) {
	const glm::vec4 row0 = matrixRow(viewProjection, 0);
	const glm::vec4 row1 = matrixRow(viewProjection, 1);
	const glm::vec4 row2 = matrixRow(viewProjection, 2);
	const glm::vec4 row3 = matrixRow(viewProjection, 3);

	mPlanes[0] = normalizePlane(row3 + row0);
	mPlanes[1] = normalizePlane(row3 - row0);
	mPlanes[2] = normalizePlane(row3 + row1);
	mPlanes[3] = normalizePlane(row3 - row1);
	mPlanes[4] = normalizePlane(row3 + row2);
	mPlanes[5] = normalizePlane(row3 - row2);
}

bool Frustum::intersectsAabb(
	const glm::vec3& localMin,
	const glm::vec3& localMax,
	const glm::mat4& modelMatrix
) const {
	const float maxValue = std::numeric_limits<float>::max();
	glm::vec3 worldMin(maxValue);
	glm::vec3 worldMax(-maxValue);

	for (int x = 0; x < 2; ++x) {
		for (int y = 0; y < 2; ++y) {
			for (int z = 0; z < 2; ++z) {
				const glm::vec3 corner(
					x == 0 ? localMin.x : localMax.x,
					y == 0 ? localMin.y : localMax.y,
					z == 0 ? localMin.z : localMax.z
				);
				const glm::vec3 worldCorner = glm::vec3(modelMatrix * glm::vec4(corner, 1.0f));
				worldMin = glm::min(worldMin, worldCorner);
				worldMax = glm::max(worldMax, worldCorner);
			}
		}
	}

	for (const glm::vec4& plane : mPlanes) {
		const glm::vec3 normal(plane);
		const glm::vec3 positive(
			normal.x >= 0.0f ? worldMax.x : worldMin.x,
			normal.y >= 0.0f ? worldMax.y : worldMin.y,
			normal.z >= 0.0f ? worldMax.z : worldMin.z
		);
		if (glm::dot(normal, positive) + plane.w < 0.0f) {
			return false;
		}
	}

	return true;
}
