#pragma once
#include "../material.h"
#include "../../texture.h"

class PhongCSMShadowMaterial :public Material {
public:
	PhongCSMShadowMaterial();
	~PhongCSMShadowMaterial();

public:
	Texture*	mDiffuse{ nullptr };
	Texture*	mSpecularMask{ nullptr };
	float		mShiness{ 1.0f };
};