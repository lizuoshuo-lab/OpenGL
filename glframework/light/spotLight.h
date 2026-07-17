#pragma once
#include "light.h"

class SpotLight : public Light {
public:
	SpotLight();
	~SpotLight();

public:
	float		mInnerAngle{ 0.0f };
	float		mOuterAngle{ 0.0f };
};
