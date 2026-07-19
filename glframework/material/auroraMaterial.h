#pragma once

#include "material.h"

class AuroraMaterial : public Material {
public:
	AuroraMaterial();
	~AuroraMaterial();
	void resetAppearance();

public:
	bool mAnimate{ true };
	float mTimeSeconds{ 0.0f };
	float mIntensity{ 0.96f };
	float mMotionSpeed{ 0.65f };
	float mLowerHeight{ 6.0f };
	float mUpperHeight{ 40.0f };
	float mCurtainDistance{ 42.0f };
	float mCurtainSpread{ 15.0f };
	float mCurtainSpan{ 76.0f };
	float mCurtainThickness{ 0.58f };
	float mFoldScale{ 0.055f };
	float mFoldStrength{ 14.0f };
	float mTurbulence{ 0.95f };
	float mBandVariation{ 0.95f };
	float mRayDetail{ 0.08f };
	float mDiffuseSkyIntensity{ 0.65f };
	float mRedEmission{ 0.06f };
	float mBlueEmission{ 0.08f };
	int mRaymarchSteps{ 64 };
	glm::vec3 mGreenColor{ 0.10f, 0.85f, 0.30f };
	glm::vec3 mRedColor{ 1.0f, 0.08f, 0.16f };
	glm::vec3 mBlueColor{ 0.24f, 0.34f, 1.0f };
};
