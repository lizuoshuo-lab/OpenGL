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
	mIntensity = 0.96f;
	mMotionSpeed = 0.30f;
	mLowerHeight = 6.0f;
	mUpperHeight = 40.0f;
	mCurtainDistance = 42.0f;
	mCurtainSpread = 15.0f;
	mCurtainSpan = 76.0f;
	mCurtainThickness = 0.58f;
	mFoldScale = 0.055f;
	mFoldStrength = 14.0f;
	mTurbulence = 0.95f;
	mBandVariation = 0.95f;
	mRayDetail = 0.08f;
	mRedEmission = 0.06f;
	mBlueEmission = 0.08f;
	mRaymarchSteps = 64;
	mGreenColor = glm::vec3(0.10f, 0.85f, 0.30f);
	mRedColor = glm::vec3(1.0f, 0.08f, 0.16f);
	mBlueColor = glm::vec3(0.24f, 0.34f, 1.0f);
}
