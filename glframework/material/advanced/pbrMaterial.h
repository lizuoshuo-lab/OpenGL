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
	float mEnvIntensity{ 1.0f };
	float mMaxReflectionLod{ 4.0f };
	float mMetallicScale{ 1.0f };
	float mRoughnessScale{ 1.0f };
	float mAoScale{ 1.0f };
	float mNormalStrength{ 1.0f };
	int mDebugView{ 0 };

	//float mRoughness{ 0.1f };
	//float mMetallic{ 0.6f };

	
};
