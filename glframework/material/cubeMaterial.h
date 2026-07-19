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
	bool mFixedBackground{ false };
	bool mStarTwinkleEnabled{ true };
	float mStarTwinkleFraction{ 0.06f };
	float mStarTwinkleStrength{ 0.18f };
	float mStarTwinkleSpeed{ 1.15f };
	float mTimeSeconds{ 0.0f };
};
