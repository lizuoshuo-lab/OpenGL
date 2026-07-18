#include "mesh.h"
#include <algorithm>

Mesh::Mesh(Geometry* geometry, Material* material) {
	mGeometry = geometry;
	mBaseGeometry = geometry;
	mMaterial = material;
	mType = ObjectType::Mesh;
}

Mesh::~Mesh(){

}

void Mesh::addLod(Geometry* geometry, float minDistance) {
	if (geometry == nullptr || minDistance < 0.0f) {
		return;
	}

	mLods.push_back({ geometry, minDistance });
	std::sort(mLods.begin(), mLods.end(), [](const LodLevel& a, const LodLevel& b) {
		return a.minDistance < b.minDistance;
	});
}

void Mesh::updateLod(float cameraDistance, bool enabled) {
	mGeometry = mBaseGeometry;
	mActiveLod = 0;
	if (!enabled) {
		return;
	}

	for (std::size_t i = 0; i < mLods.size(); ++i) {
		if (cameraDistance >= mLods[i].minDistance) {
			mGeometry = mLods[i].geometry;
			mActiveLod = static_cast<int>(i) + 1;
		}
	}
}

void Mesh::setBoneMatrices(const std::vector<glm::mat4>& boneMatrices) {
	const std::size_t count = std::min(boneMatrices.size(), kMaxBoneMatrices);
	mBoneMatrices.assign(boneMatrices.begin(), boneMatrices.begin() + count);
}

bool Mesh::isSkinned() const {
	return mGeometry != nullptr &&
		mGeometry->hasSkinningData() &&
		!mBoneMatrices.empty();
}
