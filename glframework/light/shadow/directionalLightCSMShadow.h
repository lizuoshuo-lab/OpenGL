#pragma once
#include "shadow.h"

class DirectionalLightCSMShadow :public Shadow {
public:
	DirectionalLightCSMShadow();
	~DirectionalLightCSMShadow();

	void generateCascadeLayers(std::vector<float>& layers, float near, float far);

	void setRenderTargetSize(int width, int height)override;

	glm::mat4 getLightMatrix(Camera* camera, glm::vec3 lightDir, float near, float far);
	std::vector<glm::mat4> getLightMatrices(Camera* camera, glm::vec3 lightDir, const std::vector<float>& clips);
public:
	int mLayerCount = 7;
};