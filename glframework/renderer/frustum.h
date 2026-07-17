#pragma once

#include "../core.h"
#include <array>

class Frustum {
public:
	Frustum() = default;
	explicit Frustum(const glm::mat4& viewProjection);

	void update(const glm::mat4& viewProjection);
	bool intersectsAabb(
		const glm::vec3& localMin,
		const glm::vec3& localMax,
		const glm::mat4& modelMatrix
	) const;

private:
	std::array<glm::vec4, 6> mPlanes{};
};
