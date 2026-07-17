#include "optimizationShowcase.h"

#include "../glframework/renderer/frustum.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <string>

PbrMaterial* OptimizationShowcase::createMaterial(
	PbrMaterial* environmentMaterial,
	const char* name,
	const glm::ivec4& color,
	float metallic,
	float roughness
) {
	PbrMaterial* material = new PbrMaterial();
	material->mAlbedo = Texture::createSolidTexture(
		std::string(name) + "-albedo",
		color.r,
		color.g,
		color.b,
		color.a,
		GL_SRGB_ALPHA
	);
	material->mNormal = Texture::createSolidTexture(
		std::string(name) + "-normal",
		128,
		128,
		255
	);
	material->mMetallic = Texture::createSolidTexture(
		std::string(name) + "-metallic",
		255,
		255,
		255
	);
	material->mRoughness = Texture::createSolidTexture(
		std::string(name) + "-roughness",
		255,
		255,
		255
	);
	material->mAo = Texture::createSolidTexture(
		std::string(name) + "-ao",
		255,
		255,
		255
	);
	material->mIrradianceMap = environmentMaterial->mIrradianceMap;
	material->mPrefilterMap = environmentMaterial->mPrefilterMap;
	material->mBrdfLut = environmentMaterial->mBrdfLut;
	material->mMetallicScale = metallic;
	material->mRoughnessScale = roughness;
	material->mFaceCulling = true;
	mSupplementalMaterials.push_back(material);
	return material;
}

void OptimizationShowcase::collectSourceComponents(Object* object) {
	if (object == nullptr) {
		return;
	}
	if (object->getType() == ObjectType::Mesh ||
		object->getType() == ObjectType::InstancedMesh) {
		Mesh* mesh = static_cast<Mesh*>(object);
		if (mesh->mGeometry != nullptr && mesh->mMaterial != nullptr &&
			mesh->mMaterial->mType == MaterialType::PbrMaterial) {
			mSourceComponents.push_back({
				mesh->mGeometry,
				static_cast<PbrMaterial*>(mesh->mMaterial),
				mesh->getModelMatrix(),
				nullptr
			});
		}
	}
	for (Object* child : object->getChildren()) {
		collectSourceComponents(child);
	}
}

void OptimizationShowcase::calculateSourceBounds() {
	const float limit = std::numeric_limits<float>::max();
	mSourceMinimum = glm::vec3(limit);
	mSourceMaximum = glm::vec3(-limit);
	for (const SourceComponent& component : mSourceComponents) {
		const glm::vec3 minimum = component.geometry->getMinBounds();
		const glm::vec3 maximum = component.geometry->getMaxBounds();
		for (int x = 0; x < 2; ++x) {
			for (int y = 0; y < 2; ++y) {
				for (int z = 0; z < 2; ++z) {
					const glm::vec3 corner(
						x == 0 ? minimum.x : maximum.x,
						y == 0 ? minimum.y : maximum.y,
						z == 0 ? minimum.z : maximum.z
					);
					const glm::vec3 transformed = glm::vec3(
						component.localMatrix * glm::vec4(corner, 1.0f)
					);
					mSourceMinimum = glm::min(mSourceMinimum, transformed);
					mSourceMaximum = glm::max(mSourceMaximum, transformed);
				}
			}
		}
	}
	if (mSourceComponents.empty()) {
		mSourceMinimum = glm::vec3(-0.5f);
		mSourceMaximum = glm::vec3(0.5f);
	}
}

glm::mat4 OptimizationShowcase::createProxyMatrix(
	const glm::mat4& placement,
	Geometry* proxyGeometry
) const {
	const glm::vec3 sourceCenter = (mSourceMinimum + mSourceMaximum) * 0.5f;
	const glm::vec3 sourceSize = mSourceMaximum - mSourceMinimum;
	const glm::vec3 proxyCenter =
		(proxyGeometry->getMinBounds() + proxyGeometry->getMaxBounds()) * 0.5f;
	const glm::vec3 proxySize = glm::max(
		proxyGeometry->getMaxBounds() - proxyGeometry->getMinBounds(),
		glm::vec3(0.001f)
	);
	glm::mat4 matrix = glm::translate(placement, sourceCenter);
	matrix = glm::scale(matrix, sourceSize / proxySize);
	return glm::translate(matrix, -proxyCenter);
}

OptimizationShowcase::OptimizationShowcase(
	PbrMaterial* environmentMaterial,
	Object* sourceRock
) {
	mRoot = new Object();
	mReferenceRoot = new Object();
	mInstancedRoot = new Object();
	mRoot->addChild(mReferenceRoot);
	mRoot->addChild(mInstancedRoot);
	collectSourceComponents(sourceRock);
	calculateSourceBounds();

	PbrMaterial* groundMaterial = createMaterial(
		environmentMaterial,
		"optimization-moss-ground",
		glm::ivec4(92, 102, 54, 255),
		0.0f,
		0.88f
	);
	groundMaterial->mAlbedo = Texture::createTexture("assets/textures/rocky.jpg", 0);
	groundMaterial->mNormal = Texture::createTexture("assets/textures/rocky3.jpg", 1);
	groundMaterial->mRoughness = Texture::createTexture("assets/textures/rocky2.png", 2);
	PbrMaterial* pathMaterial = createMaterial(
		environmentMaterial,
		"optimization-stone-path",
		glm::ivec4(112, 66, 37, 255),
		0.0f,
		0.92f
	);

	std::mt19937 random(20260717u);
	std::uniform_real_distribution<float> jitter(-1.0f, 1.0f);
	std::uniform_real_distribution<float> scaleDistribution(0.48f, 1.25f);
	std::uniform_real_distribution<float> angle(0.0f, 360.0f);
	std::uniform_real_distribution<float> tilt(-8.0f, 8.0f);
	constexpr int gridSize = 21;
	constexpr float spacing = 3.6f;
	for (int z = 0; z < gridSize; ++z) {
		for (int x = 0; x < gridSize; ++x) {
			const float worldZ =
				(static_cast<float>(z) - (gridSize - 1) * 0.5f) * spacing + jitter(random);
			const float pathCenter = std::sin(worldZ * 0.07f) * 2.4f;
			const float worldX =
				(static_cast<float>(x) - (gridSize - 1) * 0.5f) * spacing + jitter(random);
			if (std::abs(worldX - pathCenter) < 4.2f) {
				continue;
			}

			const float scale = scaleDistribution(random) *
				(std::abs(worldX - pathCenter) < 8.0f ? 1.12f : 1.0f);
			glm::mat4 placement = glm::translate(
				glm::mat4(1.0f),
				glm::vec3(worldX, -mSourceMinimum.y * scale, worldZ)
			);
			placement = glm::rotate(
				placement,
				glm::radians(angle(random)),
				glm::vec3(0.0f, 1.0f, 0.0f)
			);
			placement = glm::rotate(
				placement,
				glm::radians(tilt(random)),
				glm::vec3(1.0f, 0.0f, 0.0f)
			);
			placement = glm::scale(placement, glm::vec3(
				scale * (0.85f + 0.25f * std::abs(jitter(random))),
				scale,
				scale * (0.85f + 0.25f * std::abs(jitter(random)))
			));
			mPlacements.push_back(placement);

			for (const SourceComponent& component : mSourceComponents) {
				Mesh* mesh = new Mesh(component.geometry, component.material);
				mesh->setLocalMatrix(placement * component.localMatrix);
				mReferenceRoot->addChild(mesh);
			}
		}
	}

	for (SourceComponent& component : mSourceComponents) {
		component.instancedMesh = new InstancedMesh(
			component.geometry,
			component.material,
			static_cast<unsigned int>(mPlacements.size())
		);
		component.instancedMesh->setActiveLodForStats(0);
		mInstancedRoot->addChild(component.instancedMesh);
	}
	PbrMaterial* rockMaterial = !mSourceComponents.empty()
		? mSourceComponents.front().material
		: pathMaterial;
	mMediumGeometry = Geometry::createRock(1.0f, 12, 18, 0.26f);
	mLowGeometry = Geometry::createRock(1.0f, 5, 8, 0.32f);
	mMediumProxy = new InstancedMesh(
		mMediumGeometry,
		rockMaterial,
		static_cast<unsigned int>(mPlacements.size())
	);
	mLowProxy = new InstancedMesh(
		mLowGeometry,
		rockMaterial,
		static_cast<unsigned int>(mPlacements.size())
	);
	mMediumProxy->setActiveLodForStats(1);
	mLowProxy->setActiveLodForStats(2);
	mInstancedRoot->addChild(mMediumProxy);
	mInstancedRoot->addChild(mLowProxy);

	Mesh* ground = new Mesh(Geometry::createPlane(82.0f, 82.0f), groundMaterial);
	ground->setAngleX(-90.0f);
	ground->setPosition(glm::vec3(0.0f, -0.035f, 0.0f));
	mRoot->addChild(ground);
	Geometry* pathGeometry = Geometry::createPlane(7.0f, 7.2f);
	for (int segment = -5; segment <= 5; ++segment) {
		const float z = static_cast<float>(segment) * 7.0f;
		const float x = std::sin(z * 0.07f) * 2.4f;
		const float derivative = std::cos(z * 0.07f) * 0.168f;
		const float yaw = std::atan(derivative);
		glm::mat4 pathMatrix = glm::translate(
			glm::mat4(1.0f),
			glm::vec3(x, -0.015f, z)
		);
		pathMatrix = glm::rotate(pathMatrix, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
		pathMatrix = glm::rotate(
			pathMatrix,
			glm::radians(-90.0f),
			glm::vec3(1.0f, 0.0f, 0.0f)
		);
		Mesh* path = new Mesh(pathGeometry, pathMaterial);
		path->setLocalMatrix(pathMatrix);
		mRoot->addChild(path);
	}

	mReferenceRoot->setVisible(false);
	update(nullptr, true, false, false);
}

void OptimizationShowcase::update(
	Camera* camera,
	bool instancingEnabled,
	bool frustumCullingEnabled,
	bool lodEnabled
) {
	mReferenceRoot->setVisible(!instancingEnabled);
	mInstancedRoot->setVisible(instancingEnabled);
	if (!instancingEnabled) {
		return;
	}

	std::vector<glm::mat4> highPlacements;
	std::vector<glm::mat4> mediumPlacements;
	std::vector<glm::mat4> lowPlacements;
	highPlacements.reserve(mPlacements.size());
	mediumPlacements.reserve(mPlacements.size());
	lowPlacements.reserve(mPlacements.size());
	Frustum frustum;
	if (camera != nullptr) {
		frustum.update(camera->getProjectionMatrix() * camera->getViewMatrix());
	}
	const glm::mat4 rootMatrix = mRoot->getModelMatrix();
	const glm::vec3 sourceCenter = (mSourceMinimum + mSourceMaximum) * 0.5f;
	for (const glm::mat4& placement : mPlacements) {
		const glm::mat4 worldMatrix = rootMatrix * placement;
		if (camera != nullptr && frustumCullingEnabled && !frustum.intersectsAabb(
			mSourceMinimum,
			mSourceMaximum,
			worldMatrix
		)) {
			continue;
		}
		int lod = 0;
		if (camera != nullptr && lodEnabled) {
			const glm::vec3 worldCenter = glm::vec3(
				worldMatrix * glm::vec4(sourceCenter, 1.0f)
			);
			const float distance = glm::distance(camera->mPosition, worldCenter);
			lod = distance >= 55.0f ? 2 : (distance >= 30.0f ? 1 : 0);
		}
		(lod == 0 ? highPlacements : (lod == 1 ? mediumPlacements : lowPlacements))
			.push_back(placement);
	}

	for (std::size_t index = 0; index < mSourceComponents.size(); ++index) {
		SourceComponent& component = mSourceComponents[index];
		std::vector<glm::mat4> matrices;
		matrices.reserve(highPlacements.size());
		for (const glm::mat4& placement : highPlacements) {
			matrices.push_back(placement * component.localMatrix);
		}
		component.instancedMesh->setMatrices(matrices);
		component.instancedMesh->setLogicalInstanceTotal(
			index == 0 ? static_cast<unsigned int>(mPlacements.size()) : 0u
		);
		component.instancedMesh->setLogicalVisibleCount(
			index == 0 ? static_cast<unsigned int>(highPlacements.size()) : 0u
		);
	}

	std::vector<glm::mat4> mediumMatrices;
	std::vector<glm::mat4> lowMatrices;
	mediumMatrices.reserve(mediumPlacements.size());
	lowMatrices.reserve(lowPlacements.size());
	for (const glm::mat4& placement : mediumPlacements) {
		mediumMatrices.push_back(createProxyMatrix(placement, mMediumGeometry));
	}
	for (const glm::mat4& placement : lowPlacements) {
		lowMatrices.push_back(createProxyMatrix(placement, mLowGeometry));
	}
	mMediumProxy->setMatrices(mediumMatrices);
	mLowProxy->setMatrices(lowMatrices);
	mMediumProxy->setLogicalInstanceTotal(0);
	mLowProxy->setLogicalInstanceTotal(0);
	mMediumProxy->setLogicalVisibleCount(
		static_cast<unsigned int>(mediumPlacements.size())
	);
	mLowProxy->setLogicalVisibleCount(static_cast<unsigned int>(lowPlacements.size()));
}
