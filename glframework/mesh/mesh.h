#pragma once
#include"../object.h"
#include "../geometry.h"
#include "../material/material.h"

class Mesh : public Object {
public:
	static constexpr std::size_t kMaxBoneMatrices = 128;

	Mesh(Geometry* geometry, Material* material);
	~Mesh();

	void addLod(Geometry* geometry, float minDistance);
	void updateLod(float cameraDistance, bool enabled);
	int getActiveLod() const { return mActiveLod; }
	void setActiveLodForStats(int lod) { mActiveLod = lod; }
	void setBoneMatrices(const std::vector<glm::mat4>& boneMatrices);
	std::vector<glm::mat4>& getBoneMatrices() { return mBoneMatrices; }
	const std::vector<glm::mat4>& getBoneMatrices() const { return mBoneMatrices; }
	bool isSkinned() const;

public:
	Geometry* mGeometry{ nullptr };
	Material* mMaterial{ nullptr };

private:
	struct LodLevel {
		Geometry* geometry{ nullptr };
		float minDistance{ 0.0f };
	};

	Geometry* mBaseGeometry{ nullptr };
	std::vector<LodLevel> mLods;
	std::vector<glm::mat4> mBoneMatrices;
	int mActiveLod{ 0 };
};
