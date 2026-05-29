#include"directionalLightCSMShadow.h"
#include "../../../application/camera/perspectiveCamera.h"
#include "../../tools/tools.h"

DirectionalLightCSMShadow::DirectionalLightCSMShadow() {
	mRenderTarget = Framebuffer::createCSMShadowFbo(2048, 2048, mLayerCount);
}

DirectionalLightCSMShadow::~DirectionalLightCSMShadow() {
	delete mRenderTarget;
}

void DirectionalLightCSMShadow::setRenderTargetSize(int width, int height) {
	if (mRenderTarget != nullptr) {
		delete mRenderTarget;
	}

	mRenderTarget  = Framebuffer::createCSMShadowFbo(width, height, mLayerCount);
}

void DirectionalLightCSMShadow::generateCascadeLayers(std::vector<float>& layers, float near, float far) {
	layers.clear();

	for (int i = 0; i <= mLayerCount; i++) {
		float layer = near * glm::pow(far / near, (float)i / (float)mLayerCount);
		layers.push_back(layer);
	}
}

glm::mat4 DirectionalLightCSMShadow::getLightMatrix(Camera* camera, glm::vec3 lightDir, float near, float far) {
	//1 求当前子视锥体的八个角点世界坐标系的值
	auto perpCamera = (PerspectiveCamera*)camera;
	auto perpViewMatrix = perpCamera->getViewMatrix();
	auto perpProjectionMatrix = glm::perspective(glm::radians(perpCamera->mFovy), perpCamera->mAspect, near, far);
	auto corners = Tools::getFrustumCornersWorldSpace(perpProjectionMatrix * perpViewMatrix);

	//2 八个角点位置的平均值作为光源的位置，得到lightViewMatrix
	glm::vec3 center = glm::vec3(0.0f);
	for (int i = 0; i < corners.size(); i++) {
		center += glm::vec3(corners[i]);
	}
	center /= corners.size();
	
	auto lightViewMatrix = glm::lookAt(center, center + lightDir, glm::vec3(0.0, 1.0, 0.0));;
	
	//3 将八个角点转化到光源坐标系，并求最小的AABB包围盒
	float minX = std::numeric_limits<float>::max();
	float maxX = std::numeric_limits<float>::min();
	float minY = std::numeric_limits<float>::max();
	float maxY = std::numeric_limits<float>::min();
	float minZ = std::numeric_limits<float>::max();
	float maxZ = std::numeric_limits<float>::min();

	for (const auto& v : corners) {
		//将v点从世界坐标系下转换到了光源相机坐标系下
		const auto pt = lightViewMatrix * v;
		minX = std::min(minX, pt.x);
		maxX = std::max(maxX, pt.x);
		minY = std::min(minY, pt.y);
		maxY = std::max(maxY, pt.y);
		minZ = std::min(minZ, pt.z);
		maxZ = std::max(maxZ, pt.z);
	}

	//4 调整(包围盒以外的物体，也能够影响到其内部物体的阴影遮挡效果）
	maxZ *= 5;
	minZ *= 5;

	//5 计算当前光源的投影矩阵
	auto lightProjectionMatrix = glm::ortho(minX, maxX, minY,maxY, -maxZ, -minZ);

	return lightProjectionMatrix * lightViewMatrix;
}

std::vector<glm::mat4> DirectionalLightCSMShadow::getLightMatrices(Camera* camera, glm::vec3 lightDir, const std::vector<float>& clips) {
	std::vector<glm::mat4> matrices;
	for (int i = 0; i < clips.size() - 1; i++) {
		auto lightMatrix = getLightMatrix(camera, lightDir, clips[i], clips[i + 1]);
		matrices.push_back(lightMatrix);
	}
	
	return matrices;
}
