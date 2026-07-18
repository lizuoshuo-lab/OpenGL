#pragma once

#include "gpuProfiler.h"
#include "renderer.h"
#include "frustum.h"

#include "../framebuffer/framebuffer.h"
#include "../material/advanced/pbrMaterial.h"
#include "../mesh/instancedMesh.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

enum class PipelineMode : int {
	Deferred = 0,
	ForwardReference = 1
};

enum class PipelineDebugView : int {
	Final = 0,
	Position = 1,
	Normal = 2,
	Albedo = 3,
	MaterialAo = 4,
	Roughness = 5,
	Metallic = 6,
	Ssao = 7,
	Bloom = 8,
	Depth = 9
};

enum class ToneMapper : int {
	Aces = 0,
	Exposure = 1,
	Reinhard = 2
};

enum class PipelineTexture : int {
	PositionAo,
	NormalRoughness,
	AlbedoMetallic,
	Ssao,
	Bloom,
	Hdr,
	Depth
};

struct PipelineSettings {
	PipelineMode mode{ PipelineMode::Deferred };
	PipelineDebugView debugView{ PipelineDebugView::Final };
	ToneMapper toneMapper{ ToneMapper::Aces };
	bool frustumCulling{ true };
	bool lodEnabled{ true };
	bool instancingEnabled{ true };
	bool ssaoEnabled{ true };
	bool bloomEnabled{ true };
	bool showBounds{ false };
	float exposure{ 1.0f };
	float ssaoRadius{ 0.55f };
	float ssaoBias{ 0.025f };
	float ssaoStrength{ 1.0f };
	int ssaoSamples{ 32 };
	float bloomThreshold{ 1.0f };
	float bloomIntensity{ 0.12f };
	int bloomIterations{ 4 };
};

struct ShadowPassInput {
	bool enabled{ false };
	Object* root{ nullptr };
	Framebuffer* target{ nullptr };
	glm::mat4 lightMatrix{ 1.0f };
};

struct PipelineFrameStats {
	double cpuFrameMs{ 0.0 };
	double gpuFrameMs{ 0.0 };
	std::uint64_t drawCalls{ 0 };
	std::uint64_t triangles{ 0 };
	std::uint64_t totalInstances{ 0 };
	std::uint64_t visibleInstances{ 0 };
	std::uint64_t culledInstances{ 0 };
	std::array<std::uint64_t, 3> lodInstances{};
	int totalMeshes{ 0 };
	int visibleMeshes{ 0 };
	int culledMeshes{ 0 };
	double estimatedVramMb{ 0.0 };
	double sceneAssetMb{ 0.0 };
	double renderTargetMb{ 0.0 };
	std::vector<GpuPassTiming> passTimings;
};

class RenderPipeline {
public:
	RenderPipeline(Renderer* renderer, int width, int height);
	~RenderPipeline();

	void resize(int width, int height);
	void beginFrame();
	void render(
		Scene* scene,
		Camera* camera,
		const std::vector<PointLight*>& pointLights,
		PbrMaterial* globalPbr,
		const ShadowPassInput& shadowInput
	);
	void beginUiPass();
	void endUiPass();
	void endFrame();

	PipelineSettings& settings() { return mSettings; }
	const PipelineSettings& settings() const { return mSettings; }
	const PipelineFrameStats& stats() const { return mPublishedStats; }
	GLuint texture(PipelineTexture texture) const;

private:
	void createShaders();
	void createRenderTargets();
	void destroyRenderTargets();
	void ensureForwardTargets();
	void createSsaoResources();
	void destroySsaoResources();
	void checkFramebuffer(const char* name) const;

	void executePass(const std::string& name, const std::function<void()>& callback);
	void collectScene(Scene* scene, Camera* camera);
	void collectObject(Object* object, Camera* camera, const Frustum& frustum);
	void estimateSceneAssets(
		Object* object,
		std::unordered_set<const Geometry*>& geometries,
		std::unordered_set<const Texture*>& textures,
		std::size_t& instanceBytes
	);

	void executeShadowPass(const ShadowPassInput& input);
	void executeGBufferPass(Camera* camera);
	void executeSsaoPass(Camera* camera);
	void executeLightingPass(
		Camera* camera,
		const std::vector<PointLight*>& pointLights,
		PbrMaterial* globalPbr
	);
	void executeForwardOpaquePass(
		Camera* camera,
		const std::vector<PointLight*>& pointLights
	);
	void executeTransparentPass(
		Camera* camera,
		const std::vector<PointLight*>& pointLights
	);
	void executeBloomPass();
	void executePostProcessPass(Camera* camera);
	void drawGBufferMesh(Mesh* mesh, Camera* camera);
	void drawDebugBounds(Camera* camera);
	void drawFullscreen();

	GLuint activeHdrTexture() const;
	std::size_t calculateRenderTargetBytes() const;

	Renderer* mRenderer{ nullptr };
	PipelineSettings mSettings{};
	GpuProfiler mGpuProfiler;
	PipelineFrameStats mWorkingStats{};
	PipelineFrameStats mPublishedStats{};
	std::chrono::steady_clock::time_point mCpuFrameStart{};

	int mWidth{ 0 };
	int mHeight{ 0 };
	int mHalfWidth{ 0 };
	int mHalfHeight{ 0 };
	bool mUiPassActive{ false };

	Shader* mGBufferShader{ nullptr };
	Shader* mSsaoShader{ nullptr };
	Shader* mSsaoBlurShader{ nullptr };
	Shader* mSsaoUpsampleShader{ nullptr };
	Shader* mLightingShader{ nullptr };
	Shader* mBloomExtractShader{ nullptr };
	Shader* mGaussianBlurShader{ nullptr };
	Shader* mPostProcessShader{ nullptr };
	Shader* mDebugBoundsShader{ nullptr };
	Geometry* mFullscreenQuad{ nullptr };
	Geometry* mDebugBox{ nullptr };

	GLuint mGBufferFbo{ 0 };
	GLuint mGPositionAo{ 0 };
	GLuint mGNormalRoughness{ 0 };
	GLuint mGAlbedoMetallic{ 0 };
	GLuint mGDepth{ 0 };
	GLuint mHdrFbo{ 0 };
	GLuint mHdrColor{ 0 };
	GLuint mSsaoFbo{ 0 };
	GLuint mSsaoTexture{ 0 };
	GLuint mSsaoBlurFbo{ 0 };
	GLuint mSsaoBlurTexture{ 0 };
	GLuint mSsaoUpsampleFbo{ 0 };
	GLuint mSsaoUpsampleTexture{ 0 };
	GLuint mBloomExtractFbo{ 0 };
	GLuint mBloomExtractTexture{ 0 };
	std::array<GLuint, 2> mBloomFbos{};
	std::array<GLuint, 2> mBloomTextures{};
	GLuint mBloomResultTexture{ 0 };
	GLuint mSsaoNoiseTexture{ 0 };
	std::array<glm::vec3, 64> mSsaoKernel{};
	Framebuffer* mForwardMsaa{ nullptr };
	Framebuffer* mForwardResolve{ nullptr };

	std::vector<Mesh*> mDeferredOpaque;
	std::vector<Mesh*> mForwardOpaque;
	std::vector<Mesh*> mTransparent;
	std::vector<Mesh*> mVisibleMeshes;
	std::size_t mSceneAssetBytes{ 0 };
};
