#pragma once
#include"../object.h"
#include "../geometry.h"
#include "../material/material.h"

class Mesh : public Object {
public:
	Mesh(Geometry* geometry, Material* material);
	~Mesh();

	void addLod(Geometry* geometry, float minDistance);
	void updateLod(float cameraDistance, bool enabled);
	int getActiveLod() const { return mActiveLod; }
	void setActiveLodForStats(int lod) { mActiveLod = lod; }

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
	int mActiveLod{ 0 };
};
