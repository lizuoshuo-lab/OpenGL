#include "optimizationShowcase.h"

#include "../glframework/renderer/frustum.h"

#include <algorithm>
#include <array>
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
				nullptr,
				nullptr,
				nullptr,
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

	mPlanetMaterial = createMaterial(
		environmentMaterial,
		"optimization-geological-planet",
		glm::ivec4(170, 105, 72, 255),
		0.0f,
		1.0f
	);
	mPlanetMaterial->mAlbedo = Texture::createTexture(
		"assets/textures/mars_viking_mdim21_color_4k.jpg",
		0
	);
	mPlanetMaterial->mNormal = Texture::createTexture(
		"assets/textures/MoonMeteor01/moon_meteor_01_nor_gl_2k.jpg",
		1
	);
	mPlanetMaterial->mBaseColorFactor = glm::vec4(1.0f, 0.82f, 0.68f, 1.0f);
	mPlanetMaterial->mMetallicScale = 0.0f;
	mPlanetMaterial->mRoughnessScale = 1.0f;
	mPlanetMaterial->mNormalStrength = 0.38f;
	mPlanetMaterial->mSurfaceVariation = 0.0f;
	mPlanetMaterial->mFaceCulling = true;

	mPlanet = new Mesh(
		Geometry::createSphere(18.0f, 128, 192),
		mPlanetMaterial
	);
	mPlanet->setPosition(mPlanetCenter);
	mPlanet->setAngleY(mPlanetSpinAngle);
	mPlanet->setAngleZ(11.5f);
	mRoot->addChild(mPlanet);

	constexpr std::size_t asteroidCount = 5000;
	std::mt19937 random(20260717u);
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);
	std::uniform_real_distribution<float> angle(0.0f, 360.0f);
	std::uniform_real_distribution<float> signedUnit(-1.0f, 1.0f);
	std::uniform_real_distribution<float> angleJitter(-0.012f, 0.012f);
	std::normal_distribution<float> radialNoise(0.0f, 6.8f);
	std::normal_distribution<float> verticalNoise(0.0f, 2.6f);
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
		glm::mat4 baseTransform = glm::translate(glm::mat4(1.0f), position);
		baseTransform = glm::rotate(
			baseTransform,
			glm::radians(rotation.x),
			glm::vec3(1.0f, 0.0f, 0.0f)
		);
		baseTransform = glm::rotate(
			baseTransform,
			glm::radians(rotation.y),
			glm::vec3(0.0f, 1.0f, 0.0f)
		);
		baseTransform = glm::rotate(
			baseTransform,
			glm::radians(rotation.z),
			glm::vec3(0.0f, 0.0f, 1.0f)
		);
		const glm::vec3 placementScale = scale * stretch;
		const glm::mat4 placement = glm::scale(baseTransform, placementScale);
		glm::vec3 spinAxis(signedUnit(random), signedUnit(random), signedUnit(random));
		if (glm::length(spinAxis) < 0.001f) {
			spinAxis = glm::vec3(0.0f, 1.0f, 0.0f);
		}
		else {
			spinAxis = glm::normalize(spinAxis);
		}
		const float spinDirection = unit(random) < 0.5f ? -1.0f : 1.0f;
		const float spinSpeed = spinDirection * (4.0f + unit(random) * 22.0f);
		Object* referenceObject = new Object();
		referenceObject->setLocalMatrix(placement);
		mReferenceRoot->addChild(referenceObject);
		mPlacements.push_back({
			placement,
			baseTransform,
			placementScale,
			spinAxis,
			spinSpeed,
			0.0f,
			referenceObject,
			variantIndex
		});
		for (const SourceComponent& component : mSourceComponents) {
			if (component.variantIndex != variantIndex) {
				continue;
			}
			Mesh* mesh = new Mesh(component.geometry, component.material);
			mesh->setLocalMatrix(component.localMatrix);
			referenceObject->addChild(mesh);
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
		const float radius = std::clamp(58.0f + radialNoise(random), 42.0f, 75.0f);
		const glm::vec3 localPosition(
			std::cos(theta) * radius,
			verticalNoise(random),
			std::sin(theta) * radius
		);
		const glm::vec3 position = mPlanetCenter + glm::vec3(
			ringTilt * glm::vec4(localPosition, 1.0f)
		);
		const float sizeRoll = unit(random);
		float scale = 0.12f + unit(random) * 0.24f;
		if (sizeRoll < 0.03f) {
			scale = 0.82f + unit(random) * 0.58f;
		}
		else if (sizeRoll < 0.24f) {
			scale = 0.36f + unit(random) * 0.38f;
		}
		if (position.z > mPlanetCenter.z + 20.0f) {
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

		component.mediumGeometry = component.geometry->createSimplified(0.45f, 0.015f);
		if (component.mediumGeometry == nullptr) {
			component.mediumGeometry = component.geometry;
		}
		component.lowGeometry = component.mediumGeometry->createSimplified(0.34f, 0.035f);
		if (component.lowGeometry == nullptr) {
			component.lowGeometry = component.mediumGeometry;
		}
		component.mediumMesh = new InstancedMesh(
			component.mediumGeometry,
			component.material,
			static_cast<unsigned int>(mPlacements.size())
		);
		component.lowMesh = new InstancedMesh(
			component.lowGeometry,
			component.material,
			static_cast<unsigned int>(mPlacements.size())
		);
		component.mediumMesh->setActiveLodForStats(1);
		component.lowMesh->setActiveLodForStats(2);
		mInstancedRoot->addChild(component.mediumMesh);
		mInstancedRoot->addChild(component.lowMesh);
	}

	mReferenceRoot->setVisible(false);
	update(nullptr, true, false, false);
}

void OptimizationShowcase::animate(
	float deltaTime,
	bool motionEnabled,
	float orbitSpeed,
	float planetSpinSpeed,
	float asteroidSpinScale
) {
	if (!motionEnabled || deltaTime <= 0.0f) {
		return;
	}

	mOrbitAngle = std::fmod(mOrbitAngle + orbitSpeed * deltaTime, 360.0f);
	mPlanetSpinAngle = std::fmod(
		mPlanetSpinAngle + planetSpinSpeed * deltaTime,
		360.0f
	);
	if (mPlanet != nullptr) {
		mPlanet->setAngleY(mPlanetSpinAngle);
	}

	glm::mat4 orbitMatrix = glm::translate(glm::mat4(1.0f), mPlanetCenter);
	orbitMatrix = glm::rotate(
		orbitMatrix,
		glm::radians(mOrbitAngle),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	orbitMatrix = glm::translate(orbitMatrix, -mPlanetCenter);
	mReferenceRoot->setLocalMatrix(orbitMatrix);
	mInstancedRoot->setLocalMatrix(orbitMatrix);

	for (Placement& placement : mPlacements) {
		placement.spinAngle = std::fmod(
			placement.spinAngle +
				placement.spinSpeed * asteroidSpinScale * deltaTime,
			360.0f
		);
		placement.matrix = glm::rotate(
			placement.baseTransform,
			glm::radians(placement.spinAngle),
			placement.spinAxis
		);
		placement.matrix = glm::scale(placement.matrix, placement.scale);
		if (placement.referenceObject != nullptr) {
			placement.referenceObject->setLocalMatrix(placement.matrix);
		}
	}
}

void OptimizationShowcase::resetMotion() {
	mOrbitAngle = 0.0f;
	mPlanetSpinAngle = -24.0f;
	if (mPlanet != nullptr) {
		mPlanet->setAngleY(mPlanetSpinAngle);
	}
	mReferenceRoot->setLocalMatrix(glm::mat4(1.0f));
	mInstancedRoot->setLocalMatrix(glm::mat4(1.0f));
	for (Placement& placement : mPlacements) {
		placement.spinAngle = 0.0f;
		placement.matrix = glm::scale(placement.baseTransform, placement.scale);
		if (placement.referenceObject != nullptr) {
			placement.referenceObject->setLocalMatrix(placement.matrix);
		}
	}
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

	struct LodPlacement {
		const Placement* placement{ nullptr };
		float fade{ 1.0f };
	};
	std::vector<LodPlacement> highPlacements;
	std::vector<LodPlacement> mediumPlacements;
	std::vector<LodPlacement> lowPlacements;
	std::array<unsigned int, 3> logicalLodCounts{};
	highPlacements.reserve(mPlacements.size());
	mediumPlacements.reserve(mPlacements.size());
	lowPlacements.reserve(mPlacements.size());
	auto smoothRange = [](float value, float start, float end) {
		const float normalized = std::clamp((value - start) / (end - start), 0.0f, 1.0f);
		return normalized * normalized * (3.0f - 2.0f * normalized);
	};
	auto appendTransition = [](
		std::vector<LodPlacement>& nearPlacements,
		std::vector<LodPlacement>& farPlacements,
		const Placement& placement,
		float blend
	) {
		if (blend <= 0.0f) {
			nearPlacements.push_back({ &placement, 1.0f });
			return;
		}
		if (blend >= 1.0f) {
			farPlacements.push_back({ &placement, 1.0f });
			return;
		}
		nearPlacements.push_back({ &placement, 1.0f + blend });
		farPlacements.push_back({ &placement, blend });
	};
	Frustum frustum;
	if (camera != nullptr) {
		frustum.update(camera->getProjectionMatrix() * camera->getViewMatrix());
	}
	const glm::mat4 rootMatrix = mInstancedRoot->getModelMatrix();
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
		if (camera == nullptr || !lodEnabled) {
			highPlacements.push_back({ &placement, 1.0f });
			++logicalLodCounts[0];
			continue;
		}
		const glm::vec3 worldCenter = glm::vec3(
			worldMatrix * glm::vec4(sourceCenter, 1.0f)
		);
		const float distance = glm::distance(camera->mPosition, worldCenter);
		const float largestScale = std::max({
			placement.scale.x,
			placement.scale.y,
			placement.scale.z,
			0.05f
		});
		const float projectedDistance = distance / largestScale;
		if (projectedDistance < 260.0f) {
			++logicalLodCounts[projectedDistance < 215.0f ? 0 : 1];
			appendTransition(
				highPlacements,
				mediumPlacements,
				placement,
				smoothRange(projectedDistance, 170.0f, 260.0f)
			);
		}
		else if (projectedDistance < 420.0f) {
			++logicalLodCounts[1];
			mediumPlacements.push_back({ &placement, 1.0f });
		}
		else {
			++logicalLodCounts[projectedDistance < 520.0f ? 1 : 2];
			appendTransition(
				mediumPlacements,
				lowPlacements,
				placement,
				smoothRange(projectedDistance, 420.0f, 620.0f)
			);
		}
	}

	for (std::size_t index = 0; index < mSourceComponents.size(); ++index) {
		SourceComponent& component = mSourceComponents[index];
		auto buildInstances = [&](
			const std::vector<LodPlacement>& placements,
			std::vector<glm::mat4>& matrices,
			std::vector<float>& fades
		) {
			matrices.reserve(placements.size());
			fades.reserve(placements.size());
			for (const LodPlacement& lodPlacement : placements) {
				if (lodPlacement.placement->variantIndex != component.variantIndex) {
					continue;
				}
				matrices.push_back(
					lodPlacement.placement->matrix * component.localMatrix
				);
				fades.push_back(lodPlacement.fade);
			}
		};

		std::vector<glm::mat4> highMatrices;
		std::vector<glm::mat4> mediumMatrices;
		std::vector<glm::mat4> lowMatrices;
		std::vector<float> highFades;
		std::vector<float> mediumFades;
		std::vector<float> lowFades;
		buildInstances(highPlacements, highMatrices, highFades);
		buildInstances(mediumPlacements, mediumMatrices, mediumFades);
		buildInstances(lowPlacements, lowMatrices, lowFades);

		component.instancedMesh->setInstances(highMatrices, highFades);
		component.mediumMesh->setInstances(mediumMatrices, mediumFades);
		component.lowMesh->setInstances(lowMatrices, lowFades);
		component.instancedMesh->setLogicalInstanceTotal(
			index == 0 ? static_cast<unsigned int>(mPlacements.size()) : 0u
		);
		component.instancedMesh->setLogicalVisibleCount(
			index == 0 ? logicalLodCounts[0] : 0u
		);
		component.mediumMesh->setLogicalInstanceTotal(0);
		component.lowMesh->setLogicalInstanceTotal(0);
		component.mediumMesh->setLogicalVisibleCount(
			index == 0 ? logicalLodCounts[1] : 0u
		);
		component.lowMesh->setLogicalVisibleCount(
			index == 0 ? logicalLodCounts[2] : 0u
		);
	}
}
