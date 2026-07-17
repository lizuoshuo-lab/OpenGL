#pragma once
#include "../material.h"
#include "../../texture.h"

class PbrMaterial :public Material {
public:
	PbrMaterial();
	~PbrMaterial();

public:
	Texture* mAlbedo{ nullptr };
	Texture* mRoughness{ nullptr };
	Texture* mNormal{ nullptr };
	Texture* mMetallic{ nullptr };
	Texture* mAo{ nullptr };
	Texture* mIrradianceMap{ nullptr };
	Texture* mPrefilterMap{ nullptr };
	Texture* mBrdfLut{ nullptr };
	glm::vec4 mBaseColorFactor{ 1.0f };
	float mEnvIntensity{ 1.0f };
	float mMaxReflectionLod{ 4.0f };
	float mMetallicScale{ 1.0f };
	float mRoughnessScale{ 1.0f };
	float mAoScale{ 1.0f };
	float mNormalStrength{ 1.0f };
	float mSurfaceVariation{ 0.0f };
	int mMetallicChannel{ 0 };
	int mRoughnessChannel{ 0 };
	int mAoChannel{ 0 };
	int mDebugView{ 0 };
};
