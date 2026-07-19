#pragma once

#include "camera/camera.h"

#include "../glframework/material/advanced/pbrMaterial.h"
#include "../glframework/mesh/instancedMesh.h"

#include <vector>

class OptimizationShowcase {
public:
	OptimizationShowcase(
		PbrMaterial* environmentMaterial,
		const std::vector<Object*>& sourceRocks
	);

	Object* root() const { return mRoot; }
	int objectCount() const { return static_cast<int>(mPlacements.size()); }
	PbrMaterial* planetMaterial() const { return mPlanetMaterial; }
	const std::vector<PbrMaterial*>& supplementalMaterials() const {
		return mSupplementalMaterials;
	}
	void animate(
		float deltaTime,
		bool motionEnabled,
		float orbitSpeed,
		float planetSpinSpeed,
		float asteroidSpinScale
	);
	void resetMotion();

	void update(
		Camera* camera,
		bool instancingEnabled,
		bool frustumCullingEnabled,
		bool lodEnabled
	);

private:
	struct SourceComponent {
		Geometry* geometry{ nullptr };
		PbrMaterial* material{ nullptr };
		glm::mat4 localMatrix{ 1.0f };
		InstancedMesh* instancedMesh{ nullptr };
		Geometry* mediumGeometry{ nullptr };
		Geometry* lowGeometry{ nullptr };
		InstancedMesh* mediumMesh{ nullptr };
		InstancedMesh* lowMesh{ nullptr };
		std::size_t variantIndex{ 0 };
	};

	struct Placement {
		glm::mat4 matrix{ 1.0f };
		glm::mat4 baseTransform{ 1.0f };
		glm::vec3 scale{ 1.0f };
		glm::vec3 spinAxis{ 0.0f, 1.0f, 0.0f };
		float spinSpeed{ 0.0f };
		float spinAngle{ 0.0f };
		Object* referenceObject{ nullptr };
		std::size_t variantIndex{ 0 };
	};

	PbrMaterial* createMaterial(
		PbrMaterial* environmentMaterial,
		const char* name,
		const glm::ivec4& color,
		float metallic,
		float roughness
	);
	void collectSourceComponents(Object* object, std::size_t variantIndex);
	void calculateSourceBounds();

	Object* mRoot{ nullptr };
	Object* mReferenceRoot{ nullptr };
	Object* mInstancedRoot{ nullptr };
	Mesh* mPlanet{ nullptr };
	PbrMaterial* mPlanetMaterial{ nullptr };
	glm::vec3 mPlanetCenter{ -5.0f, -7.0f, -6.0f };
	float mOrbitAngle{ 0.0f };
	float mPlanetSpinAngle{ -24.0f };
	std::vector<SourceComponent> mSourceComponents;
	std::vector<PbrMaterial*> mSupplementalMaterials;
	std::vector<Placement> mPlacements;
	std::size_t mVariantCount{ 1 };
	glm::vec3 mSourceMinimum{ -0.5f };
	glm::vec3 mSourceMaximum{ 0.5f };
};
