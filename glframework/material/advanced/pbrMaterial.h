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

	//float mRoughness{ 0.1f };
	//float mMetallic{ 0.6f };

	
};
