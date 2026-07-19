#include "auroraMaterial.h"

AuroraMaterial::AuroraMaterial() {
	mType = MaterialType::AuroraMaterial;
	mDepthTest = true;
	mDepthFunc = GL_LEQUAL;
	mDepthWrite = false;
	mBlend = true;
	mSFactor = GL_ONE;
	mDFactor = GL_ONE;
	mFaceCulling = false;
	resetAppearance();
}

AuroraMaterial::~AuroraMaterial() = default;

void AuroraMaterial::resetAppearance() {
	mAnimate = true;
	mTimeSeconds = 0.0f;
	mIntensity = 1.15f;
	mMotionSpeed = 0.18f;
	mLowerHeight = 5.5f;
	mUpperHeight = 42.0f;
	mCurtainDistance = 42.0f;
	mCurtainSpread = 9.0f;
	mCurtainSpan = 76.0f;
	mCurtainThickness = 0.92f;
	mFoldScale = 0.085f;
	mFoldStrength = 7.5f;
	mTurbulence = 0.58f;
	mRedEmission = 0.30f;
	mBlueEmission = 0.20f;
	mRaymarchSteps = 64;
	mGreenColor = glm::vec3(0.16f, 1.0f, 0.42f);
	mRedColor = glm::vec3(1.0f, 0.08f, 0.16f);
	mBlueColor = glm::vec3(0.24f, 0.34f, 1.0f);
}
