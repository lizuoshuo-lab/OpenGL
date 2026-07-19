#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "application/Application.h"
#include "application/assimpLoader.h"
#include "application/camera/GameCameraControl.h"
#include "application/camera/perspectiveCamera.h"
#include "application/frameCapture.h"
#include "application/optimizationShowcase.h"
#include "application/skinnedModel.h"

#include "glframework/framebuffer/framebuffer.h"
#include "glframework/geometry.h"
#include "glframework/light/pointLight.h"
#include "glframework/material/advanced/pbrMaterial.h"
#include "glframework/material/auroraMaterial.h"
#include "glframework/material/cubeMaterial.h"
#include "glframework/mesh/mesh.h"
#include "glframework/renderer/renderPipeline.h"
#include "glframework/renderer/renderer.h"
#include "glframework/scene.h"
#include "glframework/texture.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

namespace {
Renderer* renderer = nullptr;
RenderPipeline* renderPipeline = nullptr;
Scene* scene = nullptr;
Framebuffer* softShadowFbo = nullptr;
PbrMaterial* pbrMaterial = nullptr;
OptimizationShowcase* optimizationShowcase = nullptr;
SkinnedModel* skeletalShowcase = nullptr;
CubeMaterial* environmentMaterial = nullptr;
AuroraMaterial* auroraMaterial = nullptr;
Texture* studioEnvironmentTexture = nullptr;
Texture* studioIrradianceMap = nullptr;
Texture* studioPrefilterMap = nullptr;
Texture* outdoorEnvironmentTexture = nullptr;
Texture* outdoorIrradianceMap = nullptr;
Texture* outdoorPrefilterMap = nullptr;
Texture* starfieldBackgroundTexture = nullptr;
Texture* skeletalBackgroundTexture = nullptr;

std::vector<PbrMaterial*> pbrMaterials;
std::vector<PbrMaterial*> materialMatrixMaterials;
std::vector<PbrMaterial*> asteroidMaterials;
std::vector<PointLight*> pointLights;

struct ShowcaseEntry {
	std::string name;
	std::string attribution;
	Object* object{ nullptr };
	int meshCount{ 0 };
	int materialCount{ 0 };
	bool supportsSoftShadow{ false };
	glm::vec3 cameraPosition{ 0.0f, 0.0f, 8.0f };
	glm::vec3 cameraTarget{ 0.0f };
};

std::vector<ShowcaseEntry> showcases;
int selectedShowcase = 0;
int optimizationShowcaseIndex = -1;
int ssaoShowcaseIndex = -1;
int skeletalShowcaseIndex = -1;
int auroraShowcaseIndex = -1;
bool ssaoRestoreStateValid = false;
PipelineMode ssaoRestoreMode = PipelineMode::Deferred;
PipelineDebugView ssaoRestoreDebugView = PipelineDebugView::Final;
bool ssaoRestoreBloomEnabled = true;
bool autoRotateShowcase = true;
float showcaseRotation = 0.0f;
float showcaseScale = 1.0f;
float showcaseRotationSpeed = 18.0f;
bool orbitalMotionEnabled = true;
float asteroidOrbitSpeed = 3.5f;
float planetSpinSpeed = 5.0f;
float asteroidSpinScale = 1.0f;
bool cameraOrbitEnabled = true;
float cameraOrbitSpeed = 1.8f;
float cameraOrbitAngle = 82.45f;
float cameraOrbitRadius = 98.86f;
float cameraOrbitHeight = 22.0f;
const glm::vec3 optimizationCameraCenter(-5.0f, -7.0f, -6.0f);
bool demoLoopEnabled = false;
double demoLoopStartTime = 0.0;
double demoLoopElapsed = 0.0;
int demoLoopStage = -1;

bool softShadowEnabled = true;
float softShadowStrength = 1.0f;
float softShadowBias = 0.0007f;
float softShadowRadius = 4.0f;

constexpr int kMaxDemoLights = 4;
bool directLightingEnabled = true;
bool lightEnabled[kMaxDemoLights]{ true, true, false, false };
glm::vec3 lightUiTint[kMaxDemoLights]{
	glm::vec3(1.0f),
	glm::vec3(1.0f),
	glm::vec3(1.0f),
	glm::vec3(1.0f)
};
float lightUiPower[kMaxDemoLights]{ 80.0f, 80.0f, 80.0f, 80.0f };

int windowWidth = 1600;
int windowHeight = 900;
float cameraMoveSpeed = 0.1f;
float cameraMouseSensitivity = 0.1f;
Camera* camera = nullptr;
GameCameraControl* cameraControl = nullptr;
glm::vec3 clearColor{ 0.1f, 0.13f, 0.25f };

struct BenchmarkAverage {
	double cpuMs{ 0.0 };
	double gpuMs{ 0.0 };
	double drawCalls{ 0.0 };
	double triangles{ 0.0 };
	int samples{ 0 };

	void add(const PipelineFrameStats& stats) {
		cpuMs += stats.cpuFrameMs;
		gpuMs += stats.gpuFrameMs;
		drawCalls += static_cast<double>(stats.drawCalls);
		triangles += static_cast<double>(stats.triangles);
		++samples;
	}

	BenchmarkAverage averaged() const {
		BenchmarkAverage result = *this;
		if (samples > 0) {
			result.cpuMs /= samples;
			result.gpuMs /= samples;
			result.drawCalls /= samples;
			result.triangles /= samples;
		}
		return result;
	}
};

class OptimizationBenchmark {
public:
	void start(const PipelineSettings& settings) {
		mSavedSettings = settings;
		mPhase = Phase::ReferenceWarmup;
		mFrame = 0;
		mReference = {};
		mOptimized = {};
		mHasResults = false;
	}

	void cancel(PipelineSettings& settings) {
		if (!running()) {
			return;
		}
		settings = mSavedSettings;
		mPhase = Phase::Idle;
	}

	void apply(PipelineSettings& settings) const {
		if (!running()) {
			return;
		}
		const bool optimized =
			mPhase == Phase::OptimizedWarmup || mPhase == Phase::OptimizedSample;
		settings.mode = PipelineMode::Deferred;
		settings.debugView = PipelineDebugView::Final;
		settings.frustumCulling = optimized;
		settings.lodEnabled = optimized;
		settings.instancingEnabled = optimized;
		settings.showBounds = false;
	}

	void collect(const PipelineFrameStats& stats, PipelineSettings& settings) {
		if (!running()) {
			return;
		}

		++mFrame;
		switch (mPhase) {
		case Phase::ReferenceWarmup:
			if (mFrame >= kWarmupFrames) {
				mPhase = Phase::ReferenceSample;
				mFrame = 0;
			}
			break;
		case Phase::ReferenceSample:
			mReference.add(stats);
			if (mFrame >= kSampleFrames) {
				mPhase = Phase::OptimizedWarmup;
				mFrame = 0;
			}
			break;
		case Phase::OptimizedWarmup:
			if (mFrame >= kWarmupFrames) {
				mPhase = Phase::OptimizedSample;
				mFrame = 0;
			}
			break;
		case Phase::OptimizedSample:
			mOptimized.add(stats);
			if (mFrame >= kSampleFrames) {
				mPhase = Phase::Idle;
				mHasResults = true;
				settings = mSavedSettings;
				printResults();
			}
			break;
		default:
			break;
		}
	}

	bool running() const { return mPhase != Phase::Idle; }
	bool hasResults() const { return mHasResults; }
	BenchmarkAverage reference() const { return mReference.averaged(); }
	BenchmarkAverage optimized() const { return mOptimized.averaged(); }

	const char* phaseName() const {
		switch (mPhase) {
		case Phase::ReferenceWarmup:
			return "Reference warmup";
		case Phase::ReferenceSample:
			return "Reference sampling";
		case Phase::OptimizedWarmup:
			return "Optimized warmup";
		case Phase::OptimizedSample:
			return "Optimized sampling";
		default:
			return "Idle";
		}
	}

	float progress() const {
		const int stageSize = kWarmupFrames + kSampleFrames;
		int completed = 0;
		switch (mPhase) {
		case Phase::ReferenceWarmup:
			completed = mFrame;
			break;
		case Phase::ReferenceSample:
			completed = kWarmupFrames + mFrame;
			break;
		case Phase::OptimizedWarmup:
			completed = stageSize + mFrame;
			break;
		case Phase::OptimizedSample:
			completed = stageSize + kWarmupFrames + mFrame;
			break;
		default:
			return mHasResults ? 1.0f : 0.0f;
		}
		return static_cast<float>(completed) /
			static_cast<float>(stageSize * 2);
	}

private:
	enum class Phase {
		Idle,
		ReferenceWarmup,
		ReferenceSample,
		OptimizedWarmup,
		OptimizedSample
	};

	void printResults() const {
		const BenchmarkAverage referenceAverage = reference();
		const BenchmarkAverage optimizedAverage = optimized();
		std::cout << std::fixed << std::setprecision(3)
			<< "P0_BENCHMARK reference_cpu_ms=" << referenceAverage.cpuMs
			<< " reference_gpu_ms=" << referenceAverage.gpuMs
			<< " reference_draw_calls=" << referenceAverage.drawCalls
			<< " reference_triangles=" << referenceAverage.triangles
			<< " optimized_cpu_ms=" << optimizedAverage.cpuMs
			<< " optimized_gpu_ms=" << optimizedAverage.gpuMs
			<< " optimized_draw_calls=" << optimizedAverage.drawCalls
			<< " optimized_triangles=" << optimizedAverage.triangles
			<< std::endl;
	}

	static constexpr int kWarmupFrames = 30;
	static constexpr int kSampleFrames = 90;
	Phase mPhase{ Phase::Idle };
	int mFrame{ 0 };
	bool mHasResults{ false };
	PipelineSettings mSavedSettings{};
	BenchmarkAverage mReference{};
	BenchmarkAverage mOptimized{};
};

OptimizationBenchmark optimizationBenchmark;

double reductionPercent(double reference, double optimized) {
	return reference > 0.0 ? (reference - optimized) * 100.0 / reference : 0.0;
}

void setCameraPose(const glm::vec3& position, const glm::vec3& target) {
	if (camera == nullptr) {
		return;
	}

	const glm::vec3 front = glm::normalize(target - position);
	glm::vec3 right = glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f));
	if (glm::length(right) < 0.0001f) {
		right = glm::vec3(1.0f, 0.0f, 0.0f);
	}
	else {
		right = glm::normalize(right);
	}

	camera->mPosition = position;
	camera->mRight = right;
	camera->mUp = glm::normalize(glm::cross(right, front));
}

void resetCamera() {
	if (camera == nullptr) {
		return;
	}

	glm::vec3 position(0.0f, 0.0f, 8.0f);
	glm::vec3 target(0.0f);
	if (!showcases.empty()) {
		position = showcases[selectedShowcase].cameraPosition;
		target = showcases[selectedShowcase].cameraTarget;
	}

	setCameraPose(position, target);
	if (selectedShowcase == optimizationShowcaseIndex) {
		const glm::vec3 offset = position - optimizationCameraCenter;
		cameraOrbitRadius = glm::length(glm::vec2(offset.x, offset.z));
		cameraOrbitHeight = offset.y;
		cameraOrbitAngle = glm::degrees(std::atan2(offset.z, offset.x));
	}
}

void updateCameraOrbit(float deltaTime) {
	if (camera == nullptr || deltaTime <= 0.0f) {
		return;
	}

	cameraOrbitAngle = std::fmod(
		cameraOrbitAngle + cameraOrbitSpeed * deltaTime,
		360.0f
	);
	const float angleRadians = glm::radians(cameraOrbitAngle);
	const glm::vec3 position = optimizationCameraCenter + glm::vec3(
		std::cos(angleRadians) * cameraOrbitRadius,
		cameraOrbitHeight,
		std::sin(angleRadians) * cameraOrbitRadius
	);
	setCameraPose(position, optimizationCameraCenter);
}

int countMeshes(Object* object) {
	if (object == nullptr) {
		return 0;
	}

	int count = object->getType() == ObjectType::Mesh ||
		object->getType() == ObjectType::InstancedMesh ? 1 : 0;
	for (Object* child : object->getChildren()) {
		count += countMeshes(child);
	}
	return count;
}

bool expandWorldBounds(Object* object, glm::vec3& minimum, glm::vec3& maximum) {
	if (object == nullptr) {
		return false;
	}

	bool hasBounds = false;
	if (object->getType() == ObjectType::Mesh ||
		object->getType() == ObjectType::InstancedMesh) {
		Mesh* mesh = static_cast<Mesh*>(object);
		const glm::vec3 localMinimum = mesh->mGeometry->getMinBounds();
		const glm::vec3 localMaximum = mesh->mGeometry->getMaxBounds();
		const glm::mat4 modelMatrix = mesh->getModelMatrix();
		for (int x = 0; x < 2; ++x) {
			for (int y = 0; y < 2; ++y) {
				for (int z = 0; z < 2; ++z) {
					const glm::vec3 corner(
						x == 0 ? localMinimum.x : localMaximum.x,
						y == 0 ? localMinimum.y : localMaximum.y,
						z == 0 ? localMinimum.z : localMaximum.z
					);
					const glm::vec3 worldCorner = glm::vec3(
						modelMatrix * glm::vec4(corner, 1.0f)
					);
					minimum = glm::min(minimum, worldCorner);
					maximum = glm::max(maximum, worldCorner);
					hasBounds = true;
				}
			}
		}
	}

	for (Object* child : object->getChildren()) {
		hasBounds = expandWorldBounds(child, minimum, maximum) || hasBounds;
	}
	return hasBounds;
}

void centerImportedShowcase(Object* object) {
	const float limit = std::numeric_limits<float>::max();
	glm::vec3 minimum(limit);
	glm::vec3 maximum(-limit);
	if (!expandWorldBounds(object, minimum, maximum)) {
		return;
	}
	object->setPosition(object->getPosition() - (minimum + maximum) * 0.5f);
}

void selectShowcase(int index) {
	if (showcases.empty()) {
		return;
	}
	if (renderPipeline != nullptr && optimizationBenchmark.running()) {
		optimizationBenchmark.cancel(renderPipeline->settings());
	}

	const bool previousSsaoScene = selectedShowcase == ssaoShowcaseIndex;
	selectedShowcase = std::clamp(index, 0, static_cast<int>(showcases.size()) - 1);
	for (int i = 0; i < static_cast<int>(showcases.size()); ++i) {
		showcases[i].object->setVisible(i == selectedShowcase);
	}

	showcaseRotation = 0.0f;
	showcaseScale = 1.0f;
	showcases[selectedShowcase].object->setAngleY(0.0f);
	showcases[selectedShowcase].object->setScale(glm::vec3(1.0f));
	const bool shadowScene = showcases[selectedShowcase].supportsSoftShadow;
	const bool outdoorScene = selectedShowcase == optimizationShowcaseIndex;
	const bool ssaoScene = selectedShowcase == ssaoShowcaseIndex;
	const bool skeletalScene = selectedShowcase == skeletalShowcaseIndex;
	const bool auroraScene = selectedShowcase == auroraShowcaseIndex;
	const bool chessShadowScene = shadowScene && !outdoorScene && !skeletalScene;
	if (PerspectiveCamera* perspective = dynamic_cast<PerspectiveCamera*>(camera)) {
		perspective->mFovy = outdoorScene
			? 44.0f
			: (ssaoScene ? 48.0f : (skeletalScene ? 46.0f : (auroraScene ? 58.0f : 60.0f)));
	}
	autoRotateShowcase = !shadowScene && !outdoorScene && !ssaoScene &&
		!skeletalScene && !auroraScene;
	lightEnabled[0] = true;
	lightEnabled[1] = outdoorScene || !shadowScene || skeletalScene || auroraScene;
	lightUiPower[0] = outdoorScene
		? 340000.0f
		: (chessShadowScene ? 200.0f : (skeletalScene ? 115.0f : (auroraScene ? 60.0f : 80.0f)));
	lightUiPower[1] = outdoorScene
		? 85000.0f
		: (skeletalScene ? 55.0f : (auroraScene ? 18.0f : 80.0f));
	lightUiTint[0] = outdoorScene
		? glm::vec3(1.0f, 0.80f, 0.62f)
		: (skeletalScene
			? glm::vec3(1.0f, 0.84f, 0.70f)
			: (auroraScene ? glm::vec3(0.26f, 1.0f, 0.62f) : glm::vec3(1.0f)));
	lightUiTint[1] = outdoorScene
		? glm::vec3(0.28f, 0.48f, 1.0f)
		: (skeletalScene
			? glm::vec3(0.45f, 0.65f, 1.0f)
			: (auroraScene ? glm::vec3(0.28f, 0.42f, 1.0f) : glm::vec3(1.0f)));
	if (!pointLights.empty()) {
		pointLights[0]->setPosition(
			outdoorScene
				? glm::vec3(-240.0f, 160.0f, 300.0f)
				: (skeletalScene
					? glm::vec3(-3.8f, 5.2f, 5.0f)
					: (auroraScene ? glm::vec3(-12.0f, 9.0f, 4.0f) : glm::vec3(-4.5f, 6.5f, 3.0f)))
		);
	}
	if (pointLights.size() > 1) {
		pointLights[1]->setPosition(
			outdoorScene
				? glm::vec3(220.0f, -120.0f, 180.0f)
				: (skeletalScene
					? glm::vec3(4.5f, 2.0f, -4.0f)
					: (auroraScene ? glm::vec3(14.0f, 4.0f, -18.0f) : glm::vec3(5.0f, 5.5f, -5.0f)))
		);
	}
	if (pbrMaterial != nullptr) {
		pbrMaterial->mEnvIntensity = chessShadowScene
			? 0.4f
			: (outdoorScene ? 0.38f : (skeletalScene ? 0.82f : (auroraScene ? 0.16f : 1.0f)));
		pbrMaterial->mNormalStrength = outdoorScene ? 0.82f : (auroraScene ? 0.72f : 1.0f);
		pbrMaterial->mSurfaceVariation = outdoorScene ? 0.10f : 0.0f;
		if ((outdoorScene || auroraScene) && outdoorIrradianceMap != nullptr &&
			outdoorPrefilterMap != nullptr) {
			pbrMaterial->mIrradianceMap = outdoorIrradianceMap;
			pbrMaterial->mPrefilterMap = outdoorPrefilterMap;
		}
		else if (studioIrradianceMap != nullptr && studioPrefilterMap != nullptr) {
			pbrMaterial->mIrradianceMap = studioIrradianceMap;
			pbrMaterial->mPrefilterMap = studioPrefilterMap;
		}
	}
	if (environmentMaterial != nullptr) {
		if ((outdoorScene || auroraScene) && starfieldBackgroundTexture != nullptr) {
			environmentMaterial->mDiffuse = starfieldBackgroundTexture;
		}
		else if (skeletalScene && skeletalBackgroundTexture != nullptr) {
			environmentMaterial->mDiffuse = skeletalBackgroundTexture;
		}
		else {
			environmentMaterial->mDiffuse = studioEnvironmentTexture;
		}
		environmentMaterial->mIntensity = auroraScene ? 0.14f : 1.0f;
		environmentMaterial->mBlackLevel = 0.0f;
		environmentMaterial->mStarIntensity = outdoorScene ? 0.78f : (auroraScene ? 0.54f : 0.0f);
		environmentMaterial->mFixedBackground = outdoorScene || auroraScene;
		environmentMaterial->mStarTwinkleEnabled = outdoorScene || auroraScene;
	}
	clearColor = auroraScene ? glm::vec3(0.001f, 0.002f, 0.006f) :
		glm::vec3(0.1f, 0.13f, 0.25f);
	if (renderPipeline != nullptr) {
		PipelineSettings& settings = renderPipeline->settings();
		settings.exposure = auroraScene ? 1.12f : 1.0f;
		settings.bloomIntensity = outdoorScene
			? 0.05f
			: (skeletalScene ? 0.08f : (auroraScene ? 0.12f : 0.12f));
		settings.bloomThreshold = auroraScene ? 0.85f : 1.0f;
		settings.ssaoEnabled = true;
		settings.ssaoSamples = ssaoScene ? 64 : (auroraScene ? 24 : 32);
		settings.ssaoRadius = ssaoScene
			? 1.0f
			: (outdoorScene ? 0.82f : (skeletalScene ? 0.70f : 0.55f));
		settings.ssaoBias = ssaoScene ? 0.025f : settings.ssaoBias;
		settings.ssaoStrength = ssaoScene
			? 1.55f
			: (outdoorScene ? 1.05f : (skeletalScene ? 1.15f : 1.0f));
		if (ssaoScene) {
			if (!previousSsaoScene) {
				ssaoRestoreMode = settings.mode;
				ssaoRestoreDebugView = settings.debugView;
				ssaoRestoreBloomEnabled = settings.bloomEnabled;
				ssaoRestoreStateValid = true;
			}
			settings.mode = PipelineMode::Deferred;
			settings.debugView = PipelineDebugView::Ssao;
			settings.bloomEnabled = false;
		}
		else if (previousSsaoScene && ssaoRestoreStateValid) {
			settings.mode = ssaoRestoreMode;
			settings.debugView = ssaoRestoreDebugView;
			settings.bloomEnabled = ssaoRestoreBloomEnabled;
			ssaoRestoreStateValid = false;
		}
		if (auroraScene) {
			settings.mode = PipelineMode::Deferred;
			settings.debugView = PipelineDebugView::Final;
			settings.toneMapper = ToneMapper::Aces;
			settings.bloomEnabled = true;
		}
	}
	resetCamera();
}

void addShowcase(
	const std::string& name,
	const std::string& attribution,
	Object* object,
	int materialCount,
	bool supportsSoftShadow = false,
	const glm::vec3& cameraPosition = glm::vec3(0.0f, 0.0f, 8.0f),
	const glm::vec3& cameraTarget = glm::vec3(0.0f)
) {
	if (object == nullptr) {
		return;
	}
	object->setVisible(false);
	showcases.push_back({
		name,
		attribution,
		object,
		countMeshes(object),
		materialCount,
		supportsSoftShadow,
		cameraPosition,
		cameraTarget
	});
	scene->addChild(object);
}

PbrMaterial* createSsaoMaterial(
	Texture* albedo,
	Texture* normal,
	Texture* scalar,
	const glm::vec4& tint
) {
	PbrMaterial* material = new PbrMaterial();
	material->mAlbedo = albedo;
	material->mNormal = normal;
	material->mMetallic = scalar;
	material->mRoughness = scalar;
	material->mAo = scalar;
	material->mBaseColorFactor = tint;
	material->mMetallicScale = 0.0f;
	material->mRoughnessScale = 0.88f;
	material->mAoScale = 1.0f;
	material->mNormalStrength = 0.0f;
	if (pbrMaterial != nullptr) {
		material->mIrradianceMap = pbrMaterial->mIrradianceMap;
		material->mPrefilterMap = pbrMaterial->mPrefilterMap;
		material->mBrdfLut = pbrMaterial->mBrdfLut;
	}
	materialMatrixMaterials.push_back(material);
	return material;
}

Mesh* addSsaoPrimitive(
	Object* root,
	Geometry* geometry,
	PbrMaterial* material,
	const glm::vec3& position,
	const glm::vec3& scale,
	const glm::vec3& rotation = glm::vec3(0.0f)
) {
	Mesh* mesh = new Mesh(geometry, material);
	mesh->setPosition(position);
	mesh->setScale(scale);
	mesh->setAngleX(rotation.x);
	mesh->setAngleY(rotation.y);
	mesh->setAngleZ(rotation.z);
	root->addChild(mesh);
	return mesh;
}

Object* createSsaoGallery(
	Texture* albedo,
	Texture* normal,
	Texture* scalar
) {
	Object* root = new Object();
	Geometry* box = Geometry::createBox(1.0f);
	Geometry* sphere = Geometry::createSphere(0.5f, 48, 48);
	PbrMaterial* shellMaterial = createSsaoMaterial(
		albedo,
		normal,
		scalar,
		glm::vec4(0.72f, 0.74f, 0.78f, 1.0f)
	);
	PbrMaterial* stepMaterial = createSsaoMaterial(
		albedo,
		normal,
		scalar,
		glm::vec4(0.52f, 0.56f, 0.62f, 1.0f)
	);
	PbrMaterial* objectMaterial = createSsaoMaterial(
		albedo,
		normal,
		scalar,
		glm::vec4(0.82f, 0.58f, 0.36f, 1.0f)
	);

	const float floorTop = -2.225f;
	addSsaoPrimitive(root, box, shellMaterial, { 0.0f, -2.4f, 0.0f }, { 12.0f, 0.35f, 12.0f });
	addSsaoPrimitive(root, box, shellMaterial, { 0.0f, 0.9f, -5.8f }, { 12.0f, 7.0f, 0.35f });
	addSsaoPrimitive(root, box, shellMaterial, { -5.8f, 0.9f, 0.0f }, { 0.35f, 7.0f, 12.0f });

	for (int index = 0; index < 5; ++index) {
		const float height = 0.42f * static_cast<float>(index + 1);
		addSsaoPrimitive(
			root,
			box,
			stepMaterial,
			{ 0.0f, floorTop + height * 0.5f, 2.3f - index * 1.08f },
			{ 5.4f - index * 0.55f, height, 1.25f }
		);
	}

	addSsaoPrimitive(root, box, stepMaterial, { -3.75f, -1.225f, -2.85f }, { 1.45f, 2.0f, 1.45f });
	addSsaoPrimitive(root, sphere, objectMaterial, { -3.75f, 0.67f, -2.85f }, { 1.8f, 1.8f, 1.8f });
	addSsaoPrimitive(root, box, stepMaterial, { 3.65f, -1.525f, -3.15f }, { 1.65f, 1.4f, 1.65f });
	addSsaoPrimitive(root, sphere, objectMaterial, { 3.65f, -0.12f, -3.15f }, { 1.35f, 1.35f, 1.35f });

	addSsaoPrimitive(root, sphere, objectMaterial, { 3.25f, -1.32f, 2.55f }, { 1.8f, 1.8f, 1.8f });
	addSsaoPrimitive(root, sphere, objectMaterial, { 4.55f, -1.58f, 1.95f }, { 1.28f, 1.28f, 1.28f });
	addSsaoPrimitive(root, sphere, objectMaterial, { 2.25f, -1.72f, 3.25f }, { 1.0f, 1.0f, 1.0f });
	addSsaoPrimitive(
		root,
		box,
		objectMaterial,
		{ 3.25f, -1.38f, -0.15f },
		{ 1.55f, 1.55f, 1.55f },
		{ 0.0f, 28.0f, 8.0f }
	);
	addSsaoPrimitive(
		root,
		box,
		objectMaterial,
		{ -4.9f, -0.82f, 1.55f },
		{ 1.25f, 2.8f, 1.25f },
		{ 0.0f, -12.0f, 0.0f }
	);

	addSsaoPrimitive(root, box, stepMaterial, { -2.45f, 0.45f, -5.05f }, { 3.35f, 0.38f, 1.2f });
	addSsaoPrimitive(root, sphere, objectMaterial, { -2.45f, 1.15f, -4.85f }, { 1.05f, 1.05f, 1.05f });
	addSsaoPrimitive(root, box, stepMaterial, { 4.55f, 0.05f, -5.0f }, { 1.15f, 4.55f, 1.2f });
	addSsaoPrimitive(root, sphere, objectMaterial, { 4.55f, 2.75f, -4.9f }, { 1.25f, 1.25f, 1.25f });

	return root;
}

void updateDemoLoop(double currentTime) {
	if (!demoLoopEnabled || showcases.empty()) {
		return;
	}

	demoLoopElapsed = currentTime - demoLoopStartTime;
	if (demoLoopElapsed >= 30.0) {
		demoLoopStartTime = currentTime;
		demoLoopElapsed = 0.0;
		demoLoopStage = -1;
	}

	int stage = 0;
	if (demoLoopElapsed >= 22.0) {
		stage = 3;
	}
	else if (demoLoopElapsed >= 15.0) {
		stage = 2;
	}
	else if (demoLoopElapsed >= 8.0) {
		stage = 1;
	}
	const int sequence[] = { 6, 4, 5, 0 };
	if (stage != demoLoopStage) {
		demoLoopStage = stage;
		selectShowcase(sequence[stage]);
	}
}

void setDemoLoopEnabled(bool enabled) {
	demoLoopEnabled = enabled;
	demoLoopStartTime = glfwGetTime();
	demoLoopElapsed = 0.0;
	demoLoopStage = -1;
	if (enabled) {
		updateDemoLoop(demoLoopStartTime);
	}
}

void syncPointLightColors() {
	const int count = std::min(static_cast<int>(pointLights.size()), kMaxDemoLights);
	for (int i = 0; i < count; ++i) {
		pointLights[i]->mColor = directLightingEnabled && lightEnabled[i]
			? lightUiTint[i] * lightUiPower[i]
			: glm::vec3(0.0f);
	}
}

void syncPbrControls() {
	if (pbrMaterial == nullptr) {
		return;
	}

	for (PbrMaterial* material : pbrMaterials) {
		if (material == nullptr || material == pbrMaterial) {
			continue;
		}
		material->mEnvIntensity = pbrMaterial->mEnvIntensity;
		material->mMaxReflectionLod = pbrMaterial->mMaxReflectionLod;
		material->mMetallicScale = pbrMaterial->mMetallicScale;
		material->mRoughnessScale = pbrMaterial->mRoughnessScale;
		material->mAoScale = pbrMaterial->mAoScale;
		material->mNormalStrength = pbrMaterial->mNormalStrength;
		material->mSurfaceVariation = pbrMaterial->mSurfaceVariation;
		material->mDebugView = pbrMaterial->mDebugView;
		material->mIrradianceMap = pbrMaterial->mIrradianceMap;
		material->mPrefilterMap = pbrMaterial->mPrefilterMap;
		material->mBrdfLut = pbrMaterial->mBrdfLut;
	}

	for (PbrMaterial* material : materialMatrixMaterials) {
		const bool geologicalPlanet = optimizationShowcase != nullptr &&
			material == optimizationShowcase->planetMaterial();
		material->mEnvIntensity = geologicalPlanet
			? pbrMaterial->mEnvIntensity * 1.45f
			: pbrMaterial->mEnvIntensity;
		material->mMaxReflectionLod = pbrMaterial->mMaxReflectionLod;
		material->mAoScale = pbrMaterial->mAoScale;
		if (!geologicalPlanet) {
			material->mNormalStrength = pbrMaterial->mNormalStrength;
			material->mSurfaceVariation = pbrMaterial->mSurfaceVariation;
		}
		material->mDebugView = pbrMaterial->mDebugView;
		material->mIrradianceMap = pbrMaterial->mIrradianceMap;
		material->mPrefilterMap = pbrMaterial->mPrefilterMap;
		material->mBrdfLut = pbrMaterial->mBrdfLut;
	}
}

void resizeRenderTargets(int width, int height) {
	if (width <= 0 || height <= 0) {
		return;
	}
	windowWidth = width;
	windowHeight = height;
	if (PerspectiveCamera* perspective = dynamic_cast<PerspectiveCamera*>(camera)) {
		perspective->mAspect = static_cast<float>(width) / static_cast<float>(height);
	}
	if (renderPipeline != nullptr) {
		renderPipeline->resize(width, height);
	}
}

void onResize(int width, int height) {
	resizeRenderTargets(width, height);
	glViewport(0, 0, width, height);
}

void onKey(int key, int action, int modifiers) {
	if (action == GLFW_PRESS && key == GLFW_KEY_F9 && auroraShowcaseIndex >= 0) {
		selectShowcase(auroraShowcaseIndex);
		return;
	}
	if (action == GLFW_PRESS && key == GLFW_KEY_F10) {
		setDemoLoopEnabled(!demoLoopEnabled);
		return;
	}
	if (action == GLFW_PRESS && key == GLFW_KEY_F11) {
		glApp->toggleFullscreen();
		return;
	}
	if (action == GLFW_PRESS && key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
		const int index = key - GLFW_KEY_1;
		if (index < static_cast<int>(showcases.size())) {
			selectShowcase(index);
			return;
		}
	}
	if (action == GLFW_PRESS && key == GLFW_KEY_0 && skeletalShowcaseIndex >= 0) {
		selectShowcase(skeletalShowcaseIndex);
		return;
	}
	if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard) {
		return;
	}
	cameraControl->onKey(key, action, modifiers);
}

void onMouse(int button, int action, int modifiers) {
	if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) {
		return;
	}
	double x = 0.0;
	double y = 0.0;
	glApp->getCursorPosition(&x, &y);
	cameraControl->onMouse(button, action, x, y);
}

void onCursor(double x, double y) {
	if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) {
		return;
	}
	cameraControl->onCursor(x, y);
}

void onScroll(double offset) {
	if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) {
		return;
	}
	cameraControl->onScroll(static_cast<float>(offset));
}

void prepareCamera() {
	camera = new PerspectiveCamera(
		60.0f,
		static_cast<float>(windowWidth) / static_cast<float>(windowHeight),
		0.1f,
		300.0f
	);
	cameraControl = new GameCameraControl();
	cameraControl->setCamera(camera);
	cameraControl->setSensitivity(cameraMouseSensitivity);
	cameraControl->setSpeed(cameraMoveSpeed);
	resetCamera();
}

void prepare() {
	softShadowFbo = Framebuffer::createShadowFbo(2048, 2048);
	renderer = new Renderer();
	scene = new Scene();

	pbrMaterial = new PbrMaterial();
	pbrMaterials.push_back(pbrMaterial);
	studioEnvironmentTexture = Texture::createExrTexture(
		"assets/textures/brown_photostudio_01_4k.exr"
	);
	Texture* studioEnvironmentCube = Texture::createEnvironmentCubeMap(
		studioEnvironmentTexture,
		512
	);
	studioIrradianceMap = Texture::createIrradianceCubeMap(studioEnvironmentCube, 32);
	studioPrefilterMap = Texture::createPrefilterCubeMap(studioEnvironmentCube, 128, 5);
	pbrMaterial->mIrradianceMap = studioIrradianceMap;
	pbrMaterial->mPrefilterMap = studioPrefilterMap;
	pbrMaterial->mBrdfLut = Texture::createBrdfLut(512);
	outdoorEnvironmentTexture = Texture::createExrTexture(
		"assets/textures/qwantani_night_puresky_2k.exr"
	);
	Texture* outdoorEnvironmentCube = Texture::createEnvironmentCubeMap(
		outdoorEnvironmentTexture,
		512
	);
	outdoorIrradianceMap = Texture::createIrradianceCubeMap(outdoorEnvironmentCube, 32);
	outdoorPrefilterMap = Texture::createPrefilterCubeMap(outdoorEnvironmentCube, 128, 5);
	starfieldBackgroundTexture = Texture::createSolidTexture(
		"deep-space-starfield",
		0,
		0,
		2,
		255,
		GL_SRGB_ALPHA
	);
	skeletalBackgroundTexture = Texture::createSolidTexture(
		"skeletal-neutral-backdrop",
		8,
		10,
		14,
		255,
		GL_SRGB_ALPHA
	);
	delete studioEnvironmentCube;
	delete outdoorEnvironmentCube;

	Geometry* environmentGeometry = Geometry::createBox(1.0f);
	environmentMaterial = new CubeMaterial();
	environmentMaterial->mDiffuse = studioEnvironmentTexture;
	Mesh* environmentMesh = new Mesh(environmentGeometry, environmentMaterial);

	Geometry* sphereGeometry = Geometry::createSphere(0.62f);
	Texture* matrixAlbedo = Texture::createSolidTexture(
		"matrix-albedo",
		184,
		92,
		45,
		255,
		GL_SRGB_ALPHA
	);
	Texture* matrixNormal = Texture::createSolidTexture("matrix-normal", 128, 128, 255);
	Texture* matrixWhite = Texture::createSolidTexture("matrix-white", 255, 255, 255);
	Object* materialSphereGrid = new Object();
	for (int row = 0; row < 5; ++row) {
		for (int column = 0; column < 5; ++column) {
			PbrMaterial* material = new PbrMaterial();
			material->mAlbedo = matrixAlbedo;
			material->mNormal = matrixNormal;
			material->mMetallic = matrixWhite;
			material->mRoughness = matrixWhite;
			material->mAo = matrixWhite;
			material->mIrradianceMap = pbrMaterial->mIrradianceMap;
			material->mPrefilterMap = pbrMaterial->mPrefilterMap;
			material->mBrdfLut = pbrMaterial->mBrdfLut;
			material->mMetallicScale = static_cast<float>(column) / 4.0f;
			material->mRoughnessScale =
				0.08f + 0.92f * static_cast<float>(row) / 4.0f;
			materialMatrixMaterials.push_back(material);

			Mesh* mesh = new Mesh(sphereGeometry, material);
			mesh->setPosition(glm::vec3(
				1.15f + (static_cast<float>(column) - 2.0f) * 1.42f,
				(2.0f - static_cast<float>(row)) * 1.42f,
				0.0f
			));
			materialSphereGrid->addChild(mesh);
		}
	}
	Object* materialSpherePivot = new Object();
	materialSpherePivot->addChild(materialSphereGrid);
	addShowcase(
		"Material Spheres",
		"Procedural BRDF matrix",
		materialSpherePivot,
		25
	);

	const struct ModelAsset {
		const char* name;
		const char* path;
		const char* attribution;
		bool supportsSoftShadow;
		glm::vec3 cameraPosition;
		glm::vec3 cameraTarget;
	} modelAssets[] = {
		{ "Avocado", "assets/models/Avocado/Avocado.glb", "Khronos glTF Sample Assets / CC0", false, { 0.0f, 0.0f, 8.0f }, { 0.0f, 0.0f, 0.0f } },
		{ "Lantern", "assets/models/Lantern/Lantern.glb", "Khronos glTF Sample Assets / CC0", false, { 0.0f, 0.0f, 8.0f }, { 0.0f, 0.0f, 0.0f } },
		{ "BoomBox", "assets/models/BoomBox/BoomBox.glb", "Khronos glTF Sample Assets / CC0", false, { 0.0f, 0.0f, 8.0f }, { 0.0f, 0.0f, 0.0f } },
		{ "Flight Helmet", "assets/models/FlightHelmet/FlightHelmet.gltf", "Khronos glTF Sample Assets / CC0", false, { 0.0f, 0.0f, 8.0f }, { 0.0f, 0.0f, 0.0f } },
		{ "Toy Car", "assets/models/ToyCar/ToyCar.glb", "Khronos glTF Sample Assets / CC0", false, { 0.0f, 0.0f, 8.0f }, { 0.0f, 0.0f, 0.0f } },
		{ "A Beautiful Game", "assets/models/ABeautifulGame/ABeautifulGame.glb", "Khronos glTF Sample Assets / CC BY 4.0", true, { 4.7f, 4.8f, 5.8f }, { 0.45f, -0.25f, 0.0f } }
	};

	for (const ModelAsset& asset : modelAssets) {
		std::vector<PbrMaterial*> loadedMaterials;
		Object* importedModel = AssimpLoader::load(
			asset.path,
			pbrMaterial,
			4.8f,
			&loadedMaterials
		);
		if (importedModel == nullptr) {
			continue;
		}
		pbrMaterials.insert(
			pbrMaterials.end(),
			loadedMaterials.begin(),
			loadedMaterials.end()
		);
		Object* pivot = new Object();
		pivot->addChild(importedModel);
		centerImportedShowcase(pivot);
		addShowcase(
			asset.name,
			asset.attribution,
			pivot,
			static_cast<int>(loadedMaterials.size()),
			asset.supportsSoftShadow,
			asset.cameraPosition,
			asset.cameraTarget
		);
	}

	struct AsteroidSourceDefinition {
		const char* path;
		glm::vec4 color;
		float metallic;
		float roughness;
	};
	const AsteroidSourceDefinition asteroidSourceDefinitions[] = {
		{
			"assets/models/MoonRock01/moon_rock_01_2k.gltf",
			glm::vec4(0.72f, 0.67f, 0.58f, 1.0f),
			0.0f,
			0.88f
		},
		{
			"assets/models/MoonRock02/moon_rock_02_2k.gltf",
			glm::vec4(0.34f, 0.38f, 0.45f, 1.0f),
			0.0f,
			0.98f
		},
		{
			"assets/models/MoonRock06/moon_rock_06_2k.gltf",
			glm::vec4(0.58f, 0.38f, 0.27f, 1.0f),
			0.55f,
			0.62f
		}
	};
	std::vector<Object*> rockSources;
	Texture* asteroidMetallicMask = Texture::createSolidTexture(
		"asteroid-metallic-mask",
		255,
		255,
		255
	);
	for (const AsteroidSourceDefinition& definition : asteroidSourceDefinitions) {
		std::vector<PbrMaterial*> rockMaterials;
		Object* rockSource = AssimpLoader::load(
			definition.path,
			pbrMaterial,
			3.2f,
			&rockMaterials
		);
		if (rockSource == nullptr) {
			continue;
		}
		centerImportedShowcase(rockSource);
		for (PbrMaterial* material : rockMaterials) {
			material->mMetallicScale = definition.metallic;
			material->mRoughnessScale = definition.roughness;
			material->mBaseColorFactor = definition.color;
			material->mMetallic = asteroidMetallicMask;
			material->mMetallicChannel = 0;
			material->mAo = material->mRoughness;
			material->mAoChannel = 0;
			asteroidMaterials.push_back(material);
		}
		materialMatrixMaterials.insert(
			materialMatrixMaterials.end(),
			rockMaterials.begin(),
			rockMaterials.end()
		);
		rockSources.push_back(rockSource);
	}
	optimizationShowcase = new OptimizationShowcase(pbrMaterial, rockSources);
	for (PbrMaterial* material : optimizationShowcase->supplementalMaterials()) {
		materialMatrixMaterials.push_back(material);
	}
	optimizationShowcaseIndex = static_cast<int>(showcases.size());
	addShowcase(
		"GPU Asteroid Belt",
		"Moon rocks + Qwantani IBL / Poly Haven CC0; Mars / USGS-NASA; procedural deep-space starfield",
		optimizationShowcase->root(),
		4,
		false,
		glm::vec3(8.0f, 15.0f, 92.0f),
		optimizationCameraCenter
	);

	Texture* ssaoAlbedo = Texture::createSolidTexture(
		"ssao-gallery-albedo",
		210,
		214,
		220,
		255,
		GL_SRGB_ALPHA
	);
	Object* ssaoGallery = createSsaoGallery(ssaoAlbedo, matrixNormal, matrixWhite);
	ssaoShowcaseIndex = static_cast<int>(showcases.size());
	addShowcase(
		"SSAO Gallery",
		"Procedural contact-occlusion test scene / Deferred SSAO",
		ssaoGallery,
		3,
		false,
		glm::vec3(7.6f, 3.2f, 10.0f),
		glm::vec3(-0.2f, -0.7f, -1.0f)
	);

	std::vector<PbrMaterial*> foxMaterials;
	skeletalShowcase = AssimpLoader::loadSkinned(
		"assets/models/Fox/Fox.glb",
		pbrMaterial,
		4.6f,
		&foxMaterials
	);
	if (skeletalShowcase != nullptr) {
		pbrMaterials.insert(
			pbrMaterials.end(),
			foxMaterials.begin(),
			foxMaterials.end()
		);

		Texture* stageAlbedo = Texture::createSolidTexture(
			"skeletal-stage-albedo",
			54,
			59,
			68,
			255,
			GL_SRGB_ALPHA
		);
		PbrMaterial* stageMaterial = createSsaoMaterial(
			stageAlbedo,
			matrixNormal,
			matrixWhite,
			glm::vec4(1.0f)
		);
		stageMaterial->mRoughnessScale = 0.82f;

		Object* skeletalScene = new Object();
		skeletalScene->addChild(skeletalShowcase->root());
		Mesh* stage = new Mesh(Geometry::createBox(1.0f), stageMaterial);
		stage->setPosition(glm::vec3(0.0f, -1.06f, 0.0f));
		stage->setScale(glm::vec3(11.0f, 0.08f, 8.0f));
		skeletalScene->addChild(stage);

		skeletalShowcaseIndex = static_cast<int>(showcases.size());
		addShowcase(
			"Skeletal Animation",
			"Fox / Khronos glTF Sample Assets; model CC0, rigging and animation CC BY 4.0",
			skeletalScene,
			static_cast<int>(foxMaterials.size()) + 1,
			true,
			glm::vec3(4.7f, 1.9f, 6.4f),
			glm::vec3(0.0f, -0.15f, 0.0f)
		);
	}

	Texture* auroraTerrainAlbedo = Texture::createSolidTexture(
		"aurora-terrain-albedo",
		38,
		46,
		55,
		255,
		GL_SRGB_ALPHA
	);
	PbrMaterial* auroraGroundMaterial = createSsaoMaterial(
		auroraTerrainAlbedo,
		matrixNormal,
		matrixWhite,
		glm::vec4(0.34f, 0.42f, 0.50f, 1.0f)
	);
	auroraGroundMaterial->mRoughnessScale = 0.74f;
	PbrMaterial* auroraMountainMaterial = createSsaoMaterial(
		auroraTerrainAlbedo,
		matrixNormal,
		matrixWhite,
		glm::vec4(0.17f, 0.23f, 0.29f, 1.0f)
	);
	auroraMountainMaterial->mRoughnessScale = 0.96f;
	PbrMaterial* auroraRockMaterial = createSsaoMaterial(
		auroraTerrainAlbedo,
		matrixNormal,
		matrixWhite,
		glm::vec4(0.29f, 0.37f, 0.43f, 1.0f)
	);
	auroraRockMaterial->mRoughnessScale = 0.88f;

	Object* auroraScene = new Object();
	Geometry* auroraGroundGeometry = Geometry::createBox(1.0f);
	addSsaoPrimitive(
		auroraScene,
		auroraGroundGeometry,
		auroraGroundMaterial,
		glm::vec3(0.0f, -1.45f, -31.0f),
		glm::vec3(150.0f, 0.70f, 150.0f)
	);

	Geometry* auroraMountainGeometry = Geometry::createRock(1.0f, 8, 12, 0.40f);
	const struct AuroraMountain {
		glm::vec3 position;
		glm::vec3 scale;
		float rotation;
	} auroraMountains[] = {
		{ { -65.0f, 4.0f, -69.0f }, { 19.0f, 17.0f, 13.0f }, 18.0f },
		{ { -49.0f, 2.8f, -62.0f }, { 17.0f, 13.0f, 10.0f }, -12.0f },
		{ { -32.0f, 4.9f, -70.0f }, { 20.0f, 18.0f, 14.0f }, 9.0f },
		{ { -14.0f, 3.5f, -61.0f }, { 16.0f, 15.0f, 10.0f }, -20.0f },
		{ { 4.0f, 5.4f, -72.0f }, { 22.0f, 20.0f, 15.0f }, 14.0f },
		{ { 23.0f, 3.7f, -63.0f }, { 18.0f, 16.0f, 11.0f }, -8.0f },
		{ { 41.0f, 5.0f, -71.0f }, { 21.0f, 19.0f, 14.0f }, 21.0f },
		{ { 59.0f, 3.1f, -64.0f }, { 17.0f, 14.0f, 11.0f }, -16.0f },
		{ { 73.0f, 4.6f, -72.0f }, { 20.0f, 18.0f, 14.0f }, 7.0f }
	};
	for (const AuroraMountain& mountain : auroraMountains) {
		glm::vec3 mountainPosition = mountain.position;
		glm::vec3 mountainScale = mountain.scale;
		mountainPosition.y -= 3.0f;
		mountainScale.y *= 0.68f;
		addSsaoPrimitive(
			auroraScene,
			auroraMountainGeometry,
			auroraMountainMaterial,
			mountainPosition,
			mountainScale,
			glm::vec3(0.0f, mountain.rotation, 0.0f)
		);
	}

	Geometry* auroraRockGeometry = Geometry::createRock(1.0f, 10, 14, 0.32f);
	addSsaoPrimitive(
		auroraScene,
		auroraRockGeometry,
		auroraRockMaterial,
		glm::vec3(-9.5f, -0.15f, -10.0f),
		glm::vec3(3.4f, 2.2f, 2.8f),
		glm::vec3(0.0f, 24.0f, 0.0f)
	);
	addSsaoPrimitive(
		auroraScene,
		auroraRockGeometry,
		auroraRockMaterial,
		glm::vec3(10.5f, -0.35f, -16.0f),
		glm::vec3(2.8f, 1.8f, 2.3f),
		glm::vec3(0.0f, -31.0f, 0.0f)
	);

	auroraMaterial = new AuroraMaterial();
	Mesh* auroraVolume = new Mesh(Geometry::createBox(1.0f), auroraMaterial);
	auroraScene->addChild(auroraVolume);
	auroraShowcaseIndex = static_cast<int>(showcases.size());
	addShowcase(
		"Volumetric Aurora",
		"Procedural emissive volume inspired by Lawlor & Genetti (WSCG 2011)",
		auroraScene,
		4,
		false,
		glm::vec3(0.0f, 1.20f, 15.0f),
		glm::vec3(0.0f, 18.0f, -44.0f)
	);

	selectShowcase(std::min(6, static_cast<int>(showcases.size()) - 1));
	scene->addChild(environmentMesh);

	const glm::vec3 lightPositions[] = {
		glm::vec3(-4.5f, 6.5f, 3.0f),
		glm::vec3(5.0f, 5.5f, -5.0f)
	};
	for (const glm::vec3& position : lightPositions) {
		PointLight* light = new PointLight();
		light->setPosition(position);
		light->mColor = glm::vec3(0.0f);
		pointLights.push_back(light);
	}
	syncPointLightColors();
	renderPipeline = new RenderPipeline(renderer, windowWidth, windowHeight);
}

void initImGui() {
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(glApp->getWindow(), true);
	ImGui_ImplOpenGL3_Init("#version 460");
}

void drawTexturePreview(const char* label, GLuint texture, float aspect) {
	ImGui::TextUnformatted(label);
	const float width = 174.0f;
	const float height = std::clamp(width / std::max(aspect, 0.1f), 82.0f, 120.0f);
	ImGui::Image(
		reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture)),
		ImVec2(width, height),
		ImVec2(0.0f, 1.0f),
		ImVec2(1.0f, 0.0f)
	);
}

void drawPipelinePanel() {
	if (renderPipeline == nullptr ||
		!ImGui::CollapsingHeader("Render Pipeline", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	PipelineSettings& settings = renderPipeline->settings();
	const char* modes[] = { "Deferred + SSAO", "Forward PBR" };
	int mode = static_cast<int>(settings.mode);
	if (ImGui::Combo("Pipeline", &mode, modes, IM_ARRAYSIZE(modes))) {
		settings.mode = static_cast<PipelineMode>(mode);
	}
	const char* debugViews[] = {
		"Final",
		"World Position",
		"World Normal",
		"Albedo",
		"Material AO",
		"Roughness",
		"Metallic",
		"SSAO Only",
		"Bloom",
		"Depth"
	};
	int debugView = static_cast<int>(settings.debugView);
	if (ImGui::Combo("Output Buffer", &debugView, debugViews, IM_ARRAYSIZE(debugViews))) {
		settings.debugView = static_cast<PipelineDebugView>(debugView);
	}
	const char* toneMappers[] = { "ACES", "Exposure", "Reinhard" };
	int toneMapper = static_cast<int>(settings.toneMapper);
	if (ImGui::Combo("Tone Mapper", &toneMapper, toneMappers, IM_ARRAYSIZE(toneMappers))) {
		settings.toneMapper = static_cast<ToneMapper>(toneMapper);
	}
	ImGui::SliderFloat("Exposure", &settings.exposure, 0.0f, 4.0f, "%.2f");
	ImGui::Checkbox("Frustum Culling", &settings.frustumCulling);
	ImGui::SameLine();
	ImGui::Checkbox("LOD", &settings.lodEnabled);
	ImGui::Checkbox("Instancing", &settings.instancingEnabled);
	ImGui::SameLine();
	ImGui::Checkbox("AABB", &settings.showBounds);

	if (ImGui::TreeNodeEx("SSAO", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (settings.mode == PipelineMode::ForwardReference) {
			ImGui::TextDisabled("Available in the deferred path");
		}
		ImGui::Checkbox("Enabled##SSAO", &settings.ssaoEnabled);
		ImGui::SliderInt("Samples", &settings.ssaoSamples, 8, 64);
		ImGui::SliderFloat("Radius", &settings.ssaoRadius, 0.1f, 2.0f, "%.2f");
		ImGui::SliderFloat("Bias", &settings.ssaoBias, 0.001f, 0.1f, "%.3f");
		ImGui::SliderFloat("Strength", &settings.ssaoStrength, 0.0f, 2.0f, "%.2f");
		if (ImGui::Button("View SSAO Only")) {
			settings.mode = PipelineMode::Deferred;
			settings.ssaoEnabled = true;
			settings.debugView = PipelineDebugView::Ssao;
		}
		ImGui::SameLine();
		if (ImGui::Button("View Lit Result")) {
			settings.debugView = PipelineDebugView::Final;
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enabled##Bloom", &settings.bloomEnabled);
		ImGui::SliderFloat("Threshold", &settings.bloomThreshold, 0.2f, 5.0f, "%.2f");
		ImGui::SliderFloat("Intensity", &settings.bloomIntensity, 0.0f, 1.0f, "%.2f");
		ImGui::SliderInt("Blur Passes", &settings.bloomIterations, 1, 8);
		ImGui::TreePop();
	}

	ImGui::ColorEdit3("Clear Color", &clearColor.x);
	bool fullscreen = glApp->isFullscreen();
	if (ImGui::Checkbox("Fullscreen", &fullscreen)) {
		glApp->setFullscreen(fullscreen);
	}
	ImGui::SameLine();
	bool vsync = glApp->isVsync();
	if (ImGui::Checkbox("VSync", &vsync)) {
		glApp->setVsync(vsync);
	}

	if (settings.mode == PipelineMode::Deferred &&
		ImGui::TreeNode("GBuffer Inspector")) {
		const float aspect = static_cast<float>(windowWidth) /
			static_cast<float>(std::max(windowHeight, 1));
		const struct Preview {
			const char* label;
			PipelineTexture texture;
		} previews[] = {
			{ "Position / AO", PipelineTexture::PositionAo },
			{ "Normal / Roughness", PipelineTexture::NormalRoughness },
			{ "Albedo / Metallic", PipelineTexture::AlbedoMetallic },
			{ "SSAO", PipelineTexture::Ssao },
			{ "Bloom", PipelineTexture::Bloom },
			{ "HDR", PipelineTexture::Hdr }
		};
		ImGui::Columns(2, "GBufferColumns", false);
		for (const Preview& preview : previews) {
			drawTexturePreview(
				preview.label,
				renderPipeline->texture(preview.texture),
				aspect
			);
			ImGui::NextColumn();
		}
		ImGui::Columns(1);
		ImGui::TreePop();
	}
}

void drawProfilerPanel() {
	if (renderPipeline == nullptr ||
		!ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	const PipelineFrameStats& stats = renderPipeline->stats();
	ImGui::Text("CPU frame   %.2f ms", stats.cpuFrameMs);
	ImGui::Text("GPU frame   %.2f ms", stats.gpuFrameMs);
	ImGui::Text(
		"Draw calls  %llu",
		static_cast<unsigned long long>(stats.drawCalls)
	);
	ImGui::Text("Triangles   %.3f M", static_cast<double>(stats.triangles) / 1000000.0);
	ImGui::Text(
		"Meshes      %d / %d  culled %d",
		stats.visibleMeshes,
		stats.totalMeshes,
		stats.culledMeshes
	);
	ImGui::Text(
		"Instances   %llu / %llu  culled %llu",
		static_cast<unsigned long long>(stats.visibleInstances),
		static_cast<unsigned long long>(stats.totalInstances),
		static_cast<unsigned long long>(stats.culledInstances)
	);
	ImGui::Text(
		"LOD H/M/L   %llu / %llu / %llu",
		static_cast<unsigned long long>(stats.lodInstances[0]),
		static_cast<unsigned long long>(stats.lodInstances[1]),
		static_cast<unsigned long long>(stats.lodInstances[2])
	);
	ImGui::Text("VRAM est.   %.1f MB", stats.estimatedVramMb);
	ImGui::TextDisabled(
		"Assets %.1f MB   Targets %.1f MB",
		stats.sceneAssetMb,
		stats.renderTargetMb
	);

	if (ImGui::TreeNode("GPU Pass Timings")) {
		for (const GpuPassTiming& timing : stats.passTimings) {
			if (timing.active) {
				ImGui::Text("%-13s %7.3f ms", timing.name.c_str(), timing.milliseconds);
			}
		}
		ImGui::TreePop();
	}
}

void drawOptimizationPanel() {
	if (renderPipeline == nullptr || selectedShowcase != optimizationShowcaseIndex ||
		!ImGui::CollapsingHeader("Optimization A/B", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	if (!optimizationBenchmark.running()) {
		if (ImGui::Button("Run Reference vs Optimized", ImVec2(-1.0f, 0.0f))) {
			autoRotateShowcase = false;
			optimizationBenchmark.start(renderPipeline->settings());
		}
	}
	else {
		ImGui::ProgressBar(
			optimizationBenchmark.progress(),
			ImVec2(-1.0f, 0.0f),
			optimizationBenchmark.phaseName()
		);
		if (ImGui::Button("Cancel Benchmark")) {
			optimizationBenchmark.cancel(renderPipeline->settings());
		}
	}

	if (optimizationBenchmark.hasResults()) {
		const BenchmarkAverage reference = optimizationBenchmark.reference();
		const BenchmarkAverage optimized = optimizationBenchmark.optimized();
		ImGui::Separator();
		ImGui::Text("                 Reference   Optimized");
		ImGui::Text("CPU ms           %8.2f   %8.2f", reference.cpuMs, optimized.cpuMs);
		ImGui::Text("GPU ms           %8.2f   %8.2f", reference.gpuMs, optimized.gpuMs);
		ImGui::Text("Draw calls       %8.0f   %8.0f", reference.drawCalls, optimized.drawCalls);
		ImGui::Text("Triangles (M)    %8.3f   %8.3f", reference.triangles / 1e6, optimized.triangles / 1e6);
		ImGui::TextColored(
			ImVec4(0.35f, 0.9f, 0.55f, 1.0f),
			"GPU %.1f%%   Draw %.1f%%   Tri %.1f%%",
			reductionPercent(reference.gpuMs, optimized.gpuMs),
			reductionPercent(reference.drawCalls, optimized.drawCalls),
			reductionPercent(reference.triangles, optimized.triangles)
		);
	}
}

void renderImGui() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	const float interfaceScale = std::clamp(
		static_cast<float>(windowHeight) / 1350.0f,
		1.0f,
		1.6f
	);
	ImGui::GetIO().FontGlobalScale = interfaceScale;
	ImGui::NewFrame();

	ImGui::SetNextWindowSize(
		ImVec2(
			450.0f * interfaceScale,
			std::min(830.0f * interfaceScale, static_cast<float>(windowHeight - 40))
		),
		ImGuiCond_Appearing
	);
	ImGui::Begin("OpenGL Renderer");
	const ImGuiIO& io = ImGui::GetIO();
	const float frameMs = io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f;
	ImGui::Text("FPS %.1f   Display frame %.2f ms", io.Framerate, frameMs);
	ImGui::Separator();

	if (!showcases.empty() &&
		ImGui::CollapsingHeader("Showcase", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::BeginCombo("Scene", showcases[selectedShowcase].name.c_str())) {
			for (int i = 0; i < static_cast<int>(showcases.size()); ++i) {
				const bool selected = i == selectedShowcase;
				if (ImGui::Selectable(showcases[i].name.c_str(), selected)) {
					selectShowcase(i);
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		bool requestedDemoLoop = demoLoopEnabled;
		if (ImGui::Checkbox("30 s Demo Loop", &requestedDemoLoop)) {
			setDemoLoopEnabled(requestedDemoLoop);
		}
		if (demoLoopEnabled) {
			ImGui::TextDisabled("Timeline %.1f / 30.0 s", demoLoopElapsed);
		}

		const ShowcaseEntry& showcase = showcases[selectedShowcase];
		ImGui::PushTextWrapPos();
		ImGui::TextDisabled("%s", showcase.attribution.c_str());
		ImGui::PopTextWrapPos();
		ImGui::Text("Meshes %d   Materials %d", showcase.meshCount, showcase.materialCount);
		if (selectedShowcase != auroraShowcaseIndex) {
			ImGui::Checkbox("Auto Rotate", &autoRotateShowcase);
			ImGui::SliderFloat("Rotation Speed", &showcaseRotationSpeed, 0.0f, 60.0f, "%.0f deg/s");
			if (ImGui::SliderFloat("Rotation", &showcaseRotation, -180.0f, 180.0f, "%.0f deg")) {
				showcase.object->setAngleY(showcaseRotation);
			}
			if (ImGui::SliderFloat("Model Scale", &showcaseScale, 0.5f, 1.5f, "%.2f")) {
				showcase.object->setScale(glm::vec3(showcaseScale));
			}
		}
		if (ImGui::Button("Reset View")) {
			showcaseRotation = 0.0f;
			showcaseScale = 1.0f;
			showcase.object->setAngleY(0.0f);
			showcase.object->setScale(glm::vec3(1.0f));
			if (selectedShowcase == optimizationShowcaseIndex &&
				optimizationShowcase != nullptr) {
				optimizationShowcase->resetMotion();
			}
			if (selectedShowcase == skeletalShowcaseIndex &&
				skeletalShowcase != nullptr) {
				skeletalShowcase->reset();
			}
			resetCamera();
		}
		if (selectedShowcase == optimizationShowcaseIndex &&
			optimizationShowcase != nullptr) {
			ImGui::SeparatorText("Orbital Motion");
			ImGui::Checkbox("Enable Motion", &orbitalMotionEnabled);
			ImGui::SliderFloat(
				"Belt Orbit",
				&asteroidOrbitSpeed,
				-12.0f,
				12.0f,
				"%.1f deg/s"
			);
			ImGui::SliderFloat(
				"Planet Spin",
				&planetSpinSpeed,
				-30.0f,
				30.0f,
				"%.1f deg/s"
			);
			ImGui::SliderFloat(
				"Asteroid Spin",
				&asteroidSpinScale,
				0.0f,
				4.0f,
				"%.2fx"
			);
			ImGui::Checkbox("Orbit Camera", &cameraOrbitEnabled);
			ImGui::SliderFloat(
				"Camera Orbit",
				&cameraOrbitSpeed,
				-8.0f,
				8.0f,
				"%.1f deg/s"
			);
			if (ImGui::Button("Reset Motion")) {
				optimizationShowcase->resetMotion();
			}
		}
		if (selectedShowcase == skeletalShowcaseIndex &&
			skeletalShowcase != nullptr) {
			ImGui::SeparatorText("Skeletal Animation");
			const int currentClip = skeletalShowcase->clipIndex();
			const char* clipPreview = currentClip >= 0
				? skeletalShowcase->clipName(currentClip).c_str()
				: "None";
			if (ImGui::BeginCombo("Animation Clip", clipPreview)) {
				for (int clipIndex = 0;
					clipIndex < skeletalShowcase->clipCount();
					++clipIndex) {
					const bool selected = clipIndex == currentClip;
					if (ImGui::Selectable(
						skeletalShowcase->clipName(clipIndex).c_str(),
						selected
					)) {
						skeletalShowcase->setClip(clipIndex);
						skeletalShowcase->setPlaying(true);
					}
					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			bool playing = skeletalShowcase->isPlaying();
			if (ImGui::Checkbox("Playing", &playing)) {
				skeletalShowcase->setPlaying(playing);
			}
			ImGui::SameLine();
			bool looping = skeletalShowcase->isLooping();
			if (ImGui::Checkbox("Loop", &looping)) {
				skeletalShowcase->setLooping(looping);
			}

			float speed = skeletalShowcase->playbackSpeed();
			if (ImGui::SliderFloat("Playback Speed", &speed, 0.1f, 2.5f, "%.2fx")) {
				skeletalShowcase->setPlaybackSpeed(speed);
			}
			float timeline = skeletalShowcase->normalizedTime();
			if (ImGui::SliderFloat("Timeline", &timeline, 0.0f, 1.0f, "%.3f")) {
				skeletalShowcase->setNormalizedTime(timeline);
			}
			ImGui::Text(
				"Time %.2f / %.2f s   Bones %d   Nodes %d",
				skeletalShowcase->timeSeconds(),
				skeletalShowcase->durationSeconds(),
				skeletalShowcase->boneCount(),
				skeletalShowcase->nodeCount()
			);
			if (ImGui::Button("Reset Animation")) {
				skeletalShowcase->reset();
				skeletalShowcase->setPlaying(true);
			}
		}
		if (selectedShowcase == auroraShowcaseIndex && auroraMaterial != nullptr) {
			ImGui::SeparatorText("Aurora Volume");
			ImGui::Checkbox("Animate", &auroraMaterial->mAnimate);
			ImGui::SliderFloat(
				"Emission Intensity",
				&auroraMaterial->mIntensity,
				0.0f,
				3.0f,
				"%.2f"
			);
			ImGui::SliderInt(
				"Raymarch Steps",
				&auroraMaterial->mRaymarchSteps,
				16,
				96
			);
			ImGui::SliderFloat(
				"Motion Speed",
				&auroraMaterial->mMotionSpeed,
				0.0f,
				0.8f,
				"%.2f"
			);

			ImGui::SeparatorText("Curtain Shape");
			ImGui::SliderFloat("Lower Height", &auroraMaterial->mLowerHeight, 2.0f, 20.0f, "%.1f");
			ImGui::SliderFloat("Upper Height", &auroraMaterial->mUpperHeight, 20.0f, 70.0f, "%.1f");
			auroraMaterial->mUpperHeight = std::max(
				auroraMaterial->mUpperHeight,
				auroraMaterial->mLowerHeight + 1.0f
			);
			ImGui::SliderFloat("Distance", &auroraMaterial->mCurtainDistance, 20.0f, 90.0f, "%.1f");
			ImGui::SliderFloat("Layer Spacing", &auroraMaterial->mCurtainSpread, 2.0f, 18.0f, "%.1f");
			ImGui::SliderFloat("Horizontal Span", &auroraMaterial->mCurtainSpan, 20.0f, 130.0f, "%.1f");
			ImGui::SliderFloat("Sheet Thickness", &auroraMaterial->mCurtainThickness, 0.15f, 2.0f, "%.2f");
			ImGui::SliderFloat("Fold Scale", &auroraMaterial->mFoldScale, 0.02f, 0.20f, "%.3f");
			ImGui::SliderFloat("Fold Strength", &auroraMaterial->mFoldStrength, 1.0f, 20.0f, "%.1f");
			ImGui::SliderFloat("Turbulence", &auroraMaterial->mTurbulence, 0.0f, 1.5f, "%.2f");
			ImGui::SliderFloat("Band Variation", &auroraMaterial->mBandVariation, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Ray Detail", &auroraMaterial->mRayDetail, 0.0f, 1.0f, "%.2f");

			ImGui::SeparatorText("Emission Layers");
			ImGui::SliderFloat("Red Oxygen", &auroraMaterial->mRedEmission, 0.0f, 1.5f, "%.2f");
			ImGui::SliderFloat("Blue Nitrogen", &auroraMaterial->mBlueEmission, 0.0f, 1.5f, "%.2f");
			ImGui::ColorEdit3("Green Emission", &auroraMaterial->mGreenColor.x);
			ImGui::ColorEdit3("Red Emission", &auroraMaterial->mRedColor.x);
			ImGui::ColorEdit3("Blue Emission", &auroraMaterial->mBlueColor.x);
			if (ImGui::Button("Reset Aurora")) {
				auroraMaterial->resetAppearance();
			}
		}
		if (showcase.supportsSoftShadow) {
			ImGui::Separator();
			ImGui::Checkbox("Soft Shadow", &softShadowEnabled);
			ImGui::SliderFloat("Shadow Strength", &softShadowStrength, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Shadow Radius", &softShadowRadius, 1.0f, 24.0f, "%.1f px");
			ImGui::SliderFloat("Shadow Bias", &softShadowBias, 0.0002f, 0.005f, "%.4f");
		}
	}

	drawPipelinePanel();
	drawProfilerPanel();
	drawOptimizationPanel();

	if (pbrMaterial != nullptr && ImGui::CollapsingHeader("IBL")) {
		const char* debugViews[] = {
			"Final",
			"Direct Light",
			"IBL Ambient",
			"Diffuse IBL",
			"Specular IBL",
			"Irradiance Map",
			"Prefilter Map",
			"BRDF LUT",
			"Normal",
			"AO",
			"Roughness",
			"Metallic"
		};
		ImGui::Combo("Lighting View", &pbrMaterial->mDebugView, debugViews, IM_ARRAYSIZE(debugViews));
		ImGui::SliderFloat("Environment", &pbrMaterial->mEnvIntensity, 0.0f, 5.0f, "%.2f");
		ImGui::SliderFloat("Reflection LOD", &pbrMaterial->mMaxReflectionLod, 0.0f, 4.0f, "%.2f");
		if (environmentMaterial != nullptr &&
			(selectedShowcase == optimizationShowcaseIndex ||
				selectedShowcase == auroraShowcaseIndex)) {
			ImGui::Separator();
			ImGui::SliderFloat(
				"Background Intensity",
				&environmentMaterial->mIntensity,
				0.0f,
				1.0f,
				"%.2f"
			);
			ImGui::SliderFloat(
				"Background Black Level",
				&environmentMaterial->mBlackLevel,
				0.0f,
				0.5f,
				"%.2f"
			);
			ImGui::SliderFloat(
				"Star Intensity",
				&environmentMaterial->mStarIntensity,
				0.0f,
				5.0f,
				"%.2f"
			);
			ImGui::Checkbox("Lock Starfield", &environmentMaterial->mFixedBackground);
			ImGui::Checkbox("Star Twinkle", &environmentMaterial->mStarTwinkleEnabled);
			if (environmentMaterial->mStarTwinkleEnabled) {
				ImGui::SliderFloat(
					"Twinkling Stars",
					&environmentMaterial->mStarTwinkleFraction,
					0.0f,
					0.20f,
					"%.2f"
				);
				ImGui::SliderFloat(
					"Twinkle Strength",
					&environmentMaterial->mStarTwinkleStrength,
					0.0f,
					0.50f,
					"%.2f"
				);
				ImGui::SliderFloat(
					"Twinkle Speed",
					&environmentMaterial->mStarTwinkleSpeed,
					0.0f,
					4.0f,
					"%.2f"
				);
			}
		}
	}

	if (pbrMaterial != nullptr && ImGui::CollapsingHeader("Material")) {
		ImGui::SliderFloat("Metallic", &pbrMaterial->mMetallicScale, 0.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Roughness", &pbrMaterial->mRoughnessScale, 0.1f, 2.0f, "%.2f");
		ImGui::SliderFloat("Material AO", &pbrMaterial->mAoScale, 0.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Normal Strength", &pbrMaterial->mNormalStrength, 0.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Surface Variation", &pbrMaterial->mSurfaceVariation, 0.0f, 1.0f, "%.2f");
	}
	if (selectedShowcase == optimizationShowcaseIndex &&
		optimizationShowcase != nullptr &&
		optimizationShowcase->planetMaterial() != nullptr &&
		ImGui::CollapsingHeader("Geological Planet", ImGuiTreeNodeFlags_DefaultOpen)) {
		PbrMaterial* planetMaterial = optimizationShowcase->planetMaterial();
		ImGui::ColorEdit3("Planet Tint", &planetMaterial->mBaseColorFactor.x);
		ImGui::SliderFloat(
			"Planet Roughness",
			&planetMaterial->mRoughnessScale,
			0.7f,
			1.3f,
			"%.2f"
		);
		ImGui::SliderFloat(
			"Terrain Relief",
			&planetMaterial->mNormalStrength,
			0.0f,
			1.0f,
			"%.2f"
		);
	}
	if (selectedShowcase == optimizationShowcaseIndex &&
		!asteroidMaterials.empty() &&
		ImGui::CollapsingHeader("Asteroid Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* materialNames[] = {
			"Silicate Rock",
			"Carbonaceous Rock",
			"Iron Meteorite"
		};
		for (std::size_t index = 0; index < asteroidMaterials.size(); ++index) {
			PbrMaterial* material = asteroidMaterials[index];
			ImGui::PushID(static_cast<int>(index));
			ImGui::SeparatorText(
				index < IM_ARRAYSIZE(materialNames) ? materialNames[index] : "Rock Variant"
			);
			ImGui::ColorEdit3("Base Color", &material->mBaseColorFactor.x);
			ImGui::SliderFloat("Metallic", &material->mMetallicScale, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Roughness", &material->mRoughnessScale, 0.1f, 1.5f, "%.2f");
			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("Direct Lights")) {
		ImGui::Checkbox("Enable Direct Lights", &directLightingEnabled);
		const int count = std::min(static_cast<int>(pointLights.size()), kMaxDemoLights);
		const float maximumPower =
			selectedShowcase == optimizationShowcaseIndex ? 500000.0f : 200.0f;
		for (int i = 0; i < count; ++i) {
			ImGui::PushID(i);
			ImGui::Separator();
			ImGui::Checkbox("Enabled", &lightEnabled[i]);
			glm::vec3 position = pointLights[i]->getPosition();
			if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
				pointLights[i]->setPosition(position);
			}
			ImGui::ColorEdit3("Tint", &lightUiTint[i].x);
			ImGui::SliderFloat("Power", &lightUiPower[i], 0.0f, maximumPower, "%.0f");
			ImGui::PopID();
		}
	}

	if (camera != nullptr && cameraControl != nullptr &&
		ImGui::CollapsingHeader("Camera")) {
		ImGui::DragFloat3("Camera Position", &camera->mPosition.x, 0.1f);
		if (PerspectiveCamera* perspective = dynamic_cast<PerspectiveCamera*>(camera)) {
			ImGui::SliderFloat("Field of View", &perspective->mFovy, 30.0f, 90.0f, "%.0f deg");
		}
		if (ImGui::SliderFloat("Move Speed", &cameraMoveSpeed, 0.01f, 1.0f, "%.2f")) {
			cameraControl->setSpeed(cameraMoveSpeed);
		}
		if (ImGui::SliderFloat(
			"Mouse Sensitivity",
			&cameraMouseSensitivity,
			0.01f,
			1.0f,
			"%.2f"
		)) {
			cameraControl->setSensitivity(cameraMouseSensitivity);
		}
		if (ImGui::Button("Reset Camera")) {
			resetCamera();
		}
	}

	ImGui::End();
	ImGui::Render();
	int framebufferWidth = 0;
	int framebufferHeight = 0;
	glApp->getFramebufferSize(&framebufferWidth, &framebufferHeight);
	glViewport(0, 0, framebufferWidth, framebufferHeight);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
}

int main(int argc, char* argv[]) {
	bool launchFullscreen = false;
	bool launchDemoLoop = false;
	bool launchBenchmark = false;
	bool exitAfterBenchmark = false;
	bool hideUi = false;
	int launchShowcase = -1;
	std::string launchAnimationClip;
	std::string screenshotPath;
	for (int i = 1; i < argc; ++i) {
		const std::string argument(argv[i]);
		launchFullscreen = launchFullscreen || argument == "--fullscreen";
		launchDemoLoop = launchDemoLoop || argument == "--demo";
		launchBenchmark = launchBenchmark || argument == "--benchmark";
		exitAfterBenchmark = exitAfterBenchmark || argument == "--benchmark-exit";
		hideUi = hideUi || argument == "--no-ui";
		launchBenchmark = launchBenchmark || exitAfterBenchmark;
		const std::string showcasePrefix = "--showcase=";
		const std::string animationPrefix = "--animation=";
		const std::string screenshotPrefix = "--screenshot=";
		if (argument.rfind(showcasePrefix, 0) == 0) {
			launchShowcase = std::atoi(argument.substr(showcasePrefix.size()).c_str()) - 1;
		}
		else if (argument.rfind(screenshotPrefix, 0) == 0) {
			screenshotPath = argument.substr(screenshotPrefix.size());
		}
		else if (argument.rfind(animationPrefix, 0) == 0) {
			launchAnimationClip = argument.substr(animationPrefix.size());
		}
	}

	if (!glApp->init(windowWidth, windowHeight)) {
		return -1;
	}
	glApp->getFramebufferSize(&windowWidth, &windowHeight);
	glApp->setResizeCallback(onResize);
	glApp->setKeyBoardCallback(onKey);
	glApp->setMouseCallback(onMouse);
	glApp->setCursorCallback(onCursor);
	glApp->setScrollCallback(onScroll);
	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);

	prepareCamera();
	prepare();
	initImGui();
	if (launchShowcase >= 0) {
		selectShowcase(launchShowcase);
	}
	if (!launchAnimationClip.empty() && skeletalShowcase != nullptr) {
		bool foundClip = false;
		for (int clipIndex = 0; clipIndex < skeletalShowcase->clipCount(); ++clipIndex) {
			if (skeletalShowcase->clipName(clipIndex) == launchAnimationClip) {
				selectShowcase(skeletalShowcaseIndex);
				skeletalShowcase->setClip(clipIndex);
				skeletalShowcase->setPlaying(true);
				foundClip = true;
				break;
			}
		}
		if (!foundClip) {
			std::cerr << "Unknown animation clip: " << launchAnimationClip << std::endl;
		}
	}
	if (launchFullscreen) {
		glApp->setFullscreen(true);
	}
	if (launchDemoLoop) {
		setDemoLoopEnabled(true);
	}
	if (launchBenchmark && optimizationShowcaseIndex >= 0) {
		selectShowcase(optimizationShowcaseIndex);
		autoRotateShowcase = false;
		optimizationBenchmark.start(renderPipeline->settings());
	}

	double previousFrameTime = glfwGetTime();
	int renderedFrames = 0;
	while (glApp->update()) {
		renderPipeline->beginFrame();
		const double currentFrameTime = glfwGetTime();
		const float deltaTime = static_cast<float>(
			std::min(currentFrameTime - previousFrameTime, 0.1)
		);
		previousFrameTime = currentFrameTime;
		if (environmentMaterial != nullptr) {
			environmentMaterial->mTimeSeconds = static_cast<float>(currentFrameTime);
		}
		if (auroraMaterial != nullptr && auroraMaterial->mAnimate) {
			auroraMaterial->mTimeSeconds += deltaTime;
		}

		updateDemoLoop(currentFrameTime);
		cameraControl->update();
		syncPointLightColors();
		syncPbrControls();
		optimizationBenchmark.apply(renderPipeline->settings());
		if (autoRotateShowcase && !showcases.empty()) {
			showcaseRotation += showcaseRotationSpeed * deltaTime;
			if (showcaseRotation > 180.0f) {
				showcaseRotation -= 360.0f;
			}
			showcases[selectedShowcase].object->setAngleY(showcaseRotation);
		}

		if (optimizationShowcase != nullptr &&
			optimizationShowcase->root()->isVisible()) {
			if (cameraOrbitEnabled && !optimizationBenchmark.running()) {
				updateCameraOrbit(deltaTime);
			}
			optimizationShowcase->animate(
				deltaTime,
				orbitalMotionEnabled && !optimizationBenchmark.running(),
				asteroidOrbitSpeed,
				planetSpinSpeed,
				asteroidSpinScale
			);
			const PipelineSettings& settings = renderPipeline->settings();
			optimizationShowcase->update(
				camera,
				settings.instancingEnabled,
				settings.frustumCulling,
				settings.lodEnabled
			);
		}

		if (skeletalShowcase != nullptr &&
			selectedShowcase == skeletalShowcaseIndex) {
			skeletalShowcase->update(deltaTime);
		}

		ShadowPassInput shadowInput;
		const bool renderSoftShadow =
			!showcases.empty() &&
			showcases[selectedShowcase].supportsSoftShadow &&
			softShadowEnabled &&
			softShadowFbo != nullptr &&
			!pointLights.empty();
		renderer->mSoftShadowEnabled = renderSoftShadow;
		if (renderSoftShadow) {
			const glm::vec3 lightPosition = pointLights[0]->getPosition();
			const bool outdoorShadow = selectedShowcase == optimizationShowcaseIndex;
			const glm::mat4 lightView = glm::lookAt(
				lightPosition,
				outdoorShadow ? glm::vec3(0.0f, 0.0f, -12.0f) : glm::vec3(0.0f, -0.4f, 0.0f),
				glm::vec3(0.0f, 1.0f, 0.0f)
			);
			const glm::mat4 lightProjection = outdoorShadow
				? glm::ortho(-76.0f, 76.0f, -76.0f, 76.0f, 1.0f, 190.0f)
				: glm::perspective(glm::radians(50.0f), 1.0f, 1.0f, 30.0f);
			renderer->mSoftShadowMatrix = lightProjection * lightView;
			renderer->mSoftShadowLightPosition = lightPosition;
			renderer->mSoftShadowMap = softShadowFbo->mDepthAttachment;
			renderer->mSoftShadowStrength = softShadowStrength;
			renderer->mSoftShadowBias = softShadowBias;
			renderer->mSoftShadowRadius = softShadowRadius;
			shadowInput.enabled = true;
			shadowInput.root = showcases[selectedShowcase].object;
			shadowInput.target = softShadowFbo;
			shadowInput.lightMatrix = renderer->mSoftShadowMatrix;
		}

		renderer->setClearColor(clearColor);
		renderPipeline->render(scene, camera, pointLights, pbrMaterial, shadowInput);
		if (!hideUi) {
			renderPipeline->beginUiPass();
			renderImGui();
			renderPipeline->endUiPass();
		}
		renderPipeline->endFrame();
		optimizationBenchmark.collect(
			renderPipeline->stats(),
			renderPipeline->settings()
		);
		if (exitAfterBenchmark && optimizationBenchmark.hasResults()) {
			glfwSetWindowShouldClose(glApp->getWindow(), GLFW_TRUE);
			exitAfterBenchmark = false;
		}
		++renderedFrames;
		if (!screenshotPath.empty() && renderedFrames >= 90) {
			int framebufferWidth = 0;
			int framebufferHeight = 0;
			glApp->getFramebufferSize(&framebufferWidth, &framebufferHeight);
			const bool saved = FrameCapture::saveBackBufferPng(
				screenshotPath,
				framebufferWidth,
				framebufferHeight
			);
			std::ostream& stream = saved ? std::cout : std::cerr;
			stream << (saved ? "SCREENSHOT_SAVED " : "SCREENSHOT_FAILED ")
				<< screenshotPath << " " << framebufferWidth << "x"
				<< framebufferHeight << std::endl;
			screenshotPath.clear();
			glfwSetWindowShouldClose(glApp->getWindow(), GLFW_TRUE);
		}
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	delete renderPipeline;
	delete renderer;
	delete softShadowFbo;
	delete cameraControl;
	delete camera;
	glApp->destroy();
	return 0;
}
