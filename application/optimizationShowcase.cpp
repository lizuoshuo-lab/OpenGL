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

void OptimizationShowcase::collectSourceComponents(
	Object* object,
	std::size_t variantIndex
) {
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
				nullptr,
				variantIndex
			});
		}
	}
	for (Object* child : object->getChildren()) {
		collectSourceComponents(child, variantIndex);
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
	const std::vector<Object*>& sourceRocks
) {
	mRoot = new Object();
	mReferenceRoot = new Object();
	mInstancedRoot = new Object();
	mRoot->addChild(mReferenceRoot);
	mRoot->addChild(mInstancedRoot);
	mVariantCount = std::max<std::size_t>(sourceRocks.size(), 1);
	for (std::size_t variantIndex = 0; variantIndex < sourceRocks.size(); ++variantIndex) {
		collectSourceComponents(sourceRocks[variantIndex], variantIndex);
	}
	calculateSourceBounds();

	const glm::vec3 planetCenter(-5.0f, -7.0f, -6.0f);
	PbrMaterial* planetMaterial = createMaterial(
		environmentMaterial,
		"optimization-rock-planet",
		glm::ivec4(148, 162, 175, 255),
		0.0f,
		0.82f
	);
	planetMaterial->mAlbedo = Texture::createTexture(
		"assets/textures/MoonMeteor01/moon_meteor_01_diff_2k.jpg",
		0
	);
	planetMaterial->mNormal = Texture::createTexture(
		"assets/textures/MoonMeteor01/moon_meteor_01_nor_gl_2k.jpg",
		1
	);
	planetMaterial->mRoughness = Texture::createTexture(
		"assets/textures/MoonMeteor01/moon_meteor_01_rough_2k.jpg",
		2
	);
	planetMaterial->mAo = Texture::createTexture(
		"assets/textures/MoonMeteor01/moon_meteor_01_ao_2k.jpg",
		3
	);
	planetMaterial->mBaseColorFactor = glm::vec4(0.68f, 0.72f, 0.78f, 1.0f);
	planetMaterial->mAoChannel = 0;
	planetMaterial->mFaceCulling = true;

	Mesh* planet = new Mesh(Geometry::createSphere(18.0f, 96, 128), planetMaterial);
	planet->setPosition(planetCenter);
	planet->setAngleY(-24.0f);
	mRoot->addChild(planet);

	PbrMaterial* atmosphereMaterial = createMaterial(
		environmentMaterial,
		"optimization-atmosphere",
		glm::ivec4(92, 154, 224, 255),
		0.0f,
		0.18f
	);
	atmosphereMaterial->mBaseColorFactor = glm::vec4(0.34f, 0.62f, 1.0f, 0.10f);
	atmosphereMaterial->mOpacity = 0.10f;
	atmosphereMaterial->mBlend = true;
	atmosphereMaterial->mDepthWrite = false;
	atmosphereMaterial->mFaceCulling = true;
	Mesh* atmosphere = new Mesh(
		Geometry::createSphere(18.55f, 72, 96),
		atmosphereMaterial
	);
	atmosphere->setPosition(planetCenter);
	mRoot->addChild(atmosphere);

	constexpr std::size_t asteroidCount = 2500;
	std::mt19937 random(20260717u);
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);
	std::uniform_real_distribution<float> angle(0.0f, 360.0f);
	std::uniform_real_distribution<float> angleJitter(-0.012f, 0.012f);
	std::normal_distribution<float> radialNoise(0.0f, 7.5f);
	std::normal_distribution<float> verticalNoise(0.0f, 2.8f);
	const glm::mat4 ringTilt = glm::rotate(
		glm::rotate(
			glm::mat4(1.0f),
			glm::radians(16.0f),
			glm::vec3(1.0f, 0.0f, 0.0f)
		),
		glm::radians(-8.0f),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	mPlacements.reserve(asteroidCount);

	auto addAsteroid = [&](const glm::vec3& position, float scale,
		const glm::vec3& rotation, const glm::vec3& stretch,
		std::size_t variantIndex) {
		variantIndex %= mVariantCount;
		glm::mat4 placement = glm::translate(glm::mat4(1.0f), position);
		placement = glm::rotate(
			placement,
			glm::radians(rotation.x),
			glm::vec3(1.0f, 0.0f, 0.0f)
		);
		placement = glm::rotate(
			placement,
			glm::radians(rotation.y),
			glm::vec3(0.0f, 1.0f, 0.0f)
		);
		placement = glm::rotate(
			placement,
			glm::radians(rotation.z),
			glm::vec3(0.0f, 0.0f, 1.0f)
		);
		placement = glm::scale(placement, scale * stretch);
		mPlacements.push_back({ placement, variantIndex });
		for (const SourceComponent& component : mSourceComponents) {
			if (component.variantIndex != variantIndex) {
				continue;
			}
			Mesh* mesh = new Mesh(component.geometry, component.material);
			mesh->setLocalMatrix(placement * component.localMatrix);
			mReferenceRoot->addChild(mesh);
		}
	};

	addAsteroid(
		glm::vec3(-35.0f, 7.0f, 50.0f),
		1.85f,
		glm::vec3(18.0f, 132.0f, -11.0f),
		glm::vec3(1.12f, 0.86f, 1.0f),
		0
	);
	addAsteroid(
		glm::vec3(30.0f, -9.0f, 52.0f),
		1.45f,
		glm::vec3(-26.0f, 44.0f, 18.0f),
		glm::vec3(0.90f, 1.08f, 1.18f),
		1
	);
	addAsteroid(
		glm::vec3(-49.0f, -14.0f, 24.0f),
		1.25f,
		glm::vec3(31.0f, 210.0f, -8.0f),
		glm::vec3(1.22f, 0.82f, 0.94f),
		2
	);
	addAsteroid(
		glm::vec3(43.0f, 8.0f, 18.0f),
		1.10f,
		glm::vec3(-12.0f, 310.0f, 22.0f),
		glm::vec3(0.88f, 1.16f, 1.05f),
		0
	);

	const std::size_t generatedCount = asteroidCount - mPlacements.size();
	for (std::size_t index = 0; index < generatedCount; ++index) {
		const float theta =
			2.0f * glm::pi<float>() * static_cast<float>(index) /
			static_cast<float>(generatedCount) + angleJitter(random);
		const float radius = std::clamp(58.0f + radialNoise(random), 40.0f, 78.0f);
		const glm::vec3 localPosition(
			std::cos(theta) * radius,
			verticalNoise(random),
			std::sin(theta) * radius
		);
		const glm::vec3 position = planetCenter + glm::vec3(
			ringTilt * glm::vec4(localPosition, 1.0f)
		);
		const float sizeRoll = unit(random);
		float scale = 0.12f + unit(random) * 0.24f;
		if (sizeRoll < 0.025f) {
			scale = 0.82f + unit(random) * 0.58f;
		}
		else if (sizeRoll < 0.20f) {
			scale = 0.36f + unit(random) * 0.38f;
		}
		if (position.z > planetCenter.z + 20.0f) {
			scale *= 1.12f;
		}
		addAsteroid(
			position,
			scale,
			glm::vec3(angle(random), angle(random), angle(random)),
			glm::vec3(
				0.78f + unit(random) * 0.44f,
				0.78f + unit(random) * 0.44f,
				0.78f + unit(random) * 0.44f
			),
			static_cast<std::size_t>(unit(random) * static_cast<float>(mVariantCount))
		);
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
	mMediumGeometry = Geometry::createRock(1.0f, 12, 18, 0.26f);
	mLowGeometry = Geometry::createRock(1.0f, 5, 8, 0.32f);
	mMediumProxies.reserve(mVariantCount);
	mLowProxies.reserve(mVariantCount);
	for (std::size_t variantIndex = 0; variantIndex < mVariantCount; ++variantIndex) {
		PbrMaterial* variantMaterial = planetMaterial;
		for (const SourceComponent& component : mSourceComponents) {
			if (component.variantIndex == variantIndex) {
				variantMaterial = component.material;
				break;
			}
		}
		InstancedMesh* mediumProxy = new InstancedMesh(
			mMediumGeometry,
			variantMaterial,
			static_cast<unsigned int>(mPlacements.size())
		);
		InstancedMesh* lowProxy = new InstancedMesh(
			mLowGeometry,
			variantMaterial,
			static_cast<unsigned int>(mPlacements.size())
		);
		mediumProxy->setActiveLodForStats(1);
		lowProxy->setActiveLodForStats(2);
		mMediumProxies.push_back(mediumProxy);
		mLowProxies.push_back(lowProxy);
		mInstancedRoot->addChild(mediumProxy);
		mInstancedRoot->addChild(lowProxy);
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

	std::vector<Placement> highPlacements;
	std::vector<Placement> mediumPlacements;
	std::vector<Placement> lowPlacements;
	highPlacements.reserve(mPlacements.size());
	mediumPlacements.reserve(mPlacements.size());
	lowPlacements.reserve(mPlacements.size());
	Frustum frustum;
	if (camera != nullptr) {
		frustum.update(camera->getProjectionMatrix() * camera->getViewMatrix());
	}
	const glm::mat4 rootMatrix = mRoot->getModelMatrix();
	const glm::vec3 sourceCenter = (mSourceMinimum + mSourceMaximum) * 0.5f;
	for (const Placement& placement : mPlacements) {
		const glm::mat4 worldMatrix = rootMatrix * placement.matrix;
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
			lod = distance >= 125.0f ? 2 : (distance >= 75.0f ? 1 : 0);
		}
		(lod == 0 ? highPlacements : (lod == 1 ? mediumPlacements : lowPlacements))
			.push_back(placement);
	}

	for (std::size_t index = 0; index < mSourceComponents.size(); ++index) {
		SourceComponent& component = mSourceComponents[index];
		std::vector<glm::mat4> matrices;
		matrices.reserve(highPlacements.size());
		for (const Placement& placement : highPlacements) {
			if (placement.variantIndex == component.variantIndex) {
				matrices.push_back(placement.matrix * component.localMatrix);
			}
		}
		component.instancedMesh->setMatrices(matrices);
		component.instancedMesh->setLogicalInstanceTotal(
			index == 0 ? static_cast<unsigned int>(mPlacements.size()) : 0u
		);
		component.instancedMesh->setLogicalVisibleCount(
			index == 0 ? static_cast<unsigned int>(highPlacements.size()) : 0u
		);
	}

	for (std::size_t variantIndex = 0; variantIndex < mVariantCount; ++variantIndex) {
		std::vector<glm::mat4> mediumMatrices;
		std::vector<glm::mat4> lowMatrices;
		for (const Placement& placement : mediumPlacements) {
			if (placement.variantIndex == variantIndex) {
				mediumMatrices.push_back(createProxyMatrix(placement.matrix, mMediumGeometry));
			}
		}
		for (const Placement& placement : lowPlacements) {
			if (placement.variantIndex == variantIndex) {
				lowMatrices.push_back(createProxyMatrix(placement.matrix, mLowGeometry));
			}
		}
		InstancedMesh* mediumProxy = mMediumProxies[variantIndex];
		InstancedMesh* lowProxy = mLowProxies[variantIndex];
		mediumProxy->setMatrices(mediumMatrices);
		lowProxy->setMatrices(lowMatrices);
		mediumProxy->setLogicalInstanceTotal(0);
		lowProxy->setLogicalInstanceTotal(0);
		mediumProxy->setLogicalVisibleCount(
			static_cast<unsigned int>(mediumMatrices.size())
		);
		lowProxy->setLogicalVisibleCount(static_cast<unsigned int>(lowMatrices.size()));
	}
}
