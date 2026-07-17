#pragma once
#include "material.h"
#include "../texture.h"

class CubeMaterial :public Material {
public:
	CubeMaterial();
	~CubeMaterial();

public:
	Texture* mDiffuse{ nullptr };
	float mIntensity{ 1.0f };
	float mBlackLevel{ 0.0f };
	float mStarIntensity{ 0.0f };
};
