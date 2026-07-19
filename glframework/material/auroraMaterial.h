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
	float mIntensity{ 1.15f };
	float mMotionSpeed{ 0.18f };
	float mLowerHeight{ 5.5f };
	float mUpperHeight{ 42.0f };
	float mCurtainDistance{ 42.0f };
	float mCurtainSpread{ 9.0f };
	float mCurtainSpan{ 76.0f };
	float mCurtainThickness{ 0.92f };
	float mFoldScale{ 0.085f };
	float mFoldStrength{ 7.5f };
	float mTurbulence{ 0.58f };
	float mRedEmission{ 0.30f };
	float mBlueEmission{ 0.20f };
	int mRaymarchSteps{ 64 };
	glm::vec3 mGreenColor{ 0.16f, 1.0f, 0.42f };
	glm::vec3 mRedColor{ 1.0f, 0.08f, 0.16f };
	glm::vec3 mBlueColor{ 0.24f, 0.34f, 1.0f };
};
