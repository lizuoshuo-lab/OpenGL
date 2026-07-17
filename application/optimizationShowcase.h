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
	const std::vector<PbrMaterial*>& supplementalMaterials() const {
		return mSupplementalMaterials;
	}

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
		std::size_t variantIndex{ 0 };
	};

	struct Placement {
		glm::mat4 matrix{ 1.0f };
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
	glm::mat4 createProxyMatrix(
		const glm::mat4& placement,
		Geometry* proxyGeometry
	) const;

	Object* mRoot{ nullptr };
	Object* mReferenceRoot{ nullptr };
	Object* mInstancedRoot{ nullptr };
	std::vector<SourceComponent> mSourceComponents;
	std::vector<PbrMaterial*> mSupplementalMaterials;
	std::vector<Placement> mPlacements;
	std::size_t mVariantCount{ 1 };
	glm::vec3 mSourceMinimum{ -0.5f };
	glm::vec3 mSourceMaximum{ 0.5f };
	Geometry* mMediumGeometry{ nullptr };
	Geometry* mLowGeometry{ nullptr };
	std::vector<InstancedMesh*> mMediumProxies;
	std::vector<InstancedMesh*> mLowProxies;
};
