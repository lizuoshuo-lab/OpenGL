#include "renderPipeline.h"

#include "../material/cubeMaterial.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <unordered_set>

namespace {
constexpr double kBytesPerMegabyte = 1024.0 * 1024.0;

void createTexture2D(
	GLuint& texture,
	GLenum internalFormat,
	GLenum format,
	GLenum type,
	int width,
	int height,
	GLenum filter
) {
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		internalFormat,
		width,
		height,
		0,
		format,
		type,
		nullptr
	);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void bindTexture2D(GLuint texture, int unit) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, texture);
}

void labelObject(GLenum type, GLuint object, const char* label) {
	if (glObjectLabel != nullptr && object != 0) {
		glObjectLabel(type, object, -1, label);
	}
}

void beginDebugGroup(const std::string& name) {
	if (glPushDebugGroup != nullptr) {
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name.c_str());
	}
}

void endDebugGroup() {
	if (glPopDebugGroup != nullptr) {
		glPopDebugGroup();
	}
}

std::uint64_t meshInstanceCount(const Mesh* mesh) {
	if (mesh != nullptr && mesh->getType() == ObjectType::InstancedMesh) {
		return static_cast<const InstancedMesh*>(mesh)->mInstanceCount;
	}
	return 1;
}

void applyFaceCulling(const Material* material) {
	if (material != nullptr && material->mFaceCulling) {
		glEnable(GL_CULL_FACE);
		glFrontFace(material->mFrontFace);
		glCullFace(material->mCullFace);
	}
	else {
		glDisable(GL_CULL_FACE);
	}
}
}

RenderPipeline::RenderPipeline(Renderer* renderer, int width, int height)
	: mRenderer(renderer), mWidth(std::max(width, 1)), mHeight(std::max(height, 1)) {
	mHalfWidth = std::max(1, mWidth / 2);
	mHalfHeight = std::max(1, mHeight / 2);
	createShaders();
	mFullscreenQuad = Geometry::createScreenPlane();
	mDebugBox = Geometry::createBox(1.0f);
	createSsaoResources();
	createRenderTargets();
}

RenderPipeline::~RenderPipeline() {
	destroyRenderTargets();
	destroySsaoResources();
	delete mFullscreenQuad;
	delete mDebugBox;
	delete mGBufferShader;
	delete mSsaoShader;
	delete mSsaoBlurShader;
	delete mSsaoUpsampleShader;
	delete mLightingShader;
	delete mBloomExtractShader;
	delete mGaussianBlurShader;
	delete mPostProcessShader;
	delete mDebugBoundsShader;
}

void RenderPipeline::createShaders() {
	mGBufferShader = new Shader(
		"assets/shaders/deferred/gbuffer.vert",
		"assets/shaders/deferred/gbuffer.frag"
	);
	mSsaoShader = new Shader(
		"assets/shaders/screen.vert",
		"assets/shaders/deferred/ssao.frag"
	);
	mSsaoBlurShader = new Shader(
		"assets/shaders/screen.vert",
		"assets/shaders/deferred/ssaoBlur.frag"
	);
	mSsaoUpsampleShader = new Shader(
		"assets/shaders/screen.vert",
		"assets/shaders/deferred/ssaoUpsample.frag"
	);
	mLightingShader = new Shader(
		"assets/shaders/screen.vert",
		"assets/shaders/deferred/lighting.frag"
	);
	mBloomExtractShader = new Shader(
		"assets/shaders/screen.vert",
		"assets/shaders/deferred/bloomExtract.frag"
	);
	mGaussianBlurShader = new Shader(
		"assets/shaders/screen.vert",
		"assets/shaders/deferred/gaussianBlur.frag"
	);
	mPostProcessShader = new Shader(
		"assets/shaders/screen.vert",
		"assets/shaders/deferred/postProcess.frag"
	);
	mDebugBoundsShader = new Shader(
		"assets/shaders/deferred/debugBounds.vert",
		"assets/shaders/deferred/debugBounds.frag"
	);
}

void RenderPipeline::createSsaoResources() {
	std::mt19937 random(1337u);
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);
	std::uniform_real_distribution<float> signedUnit(-1.0f, 1.0f);
	for (std::size_t i = 0; i < mSsaoKernel.size(); ++i) {
		glm::vec3 sample(signedUnit(random), signedUnit(random), unit(random));
		sample = glm::normalize(sample) * unit(random);
		const float normalizedIndex = static_cast<float>(i) /
			static_cast<float>(mSsaoKernel.size() - 1);
		const float scale = glm::mix(0.1f, 1.0f, normalizedIndex * normalizedIndex);
		mSsaoKernel[i] = sample * scale;
	}

	std::array<glm::vec3, 16> noise{};
	for (glm::vec3& value : noise) {
		value = glm::normalize(glm::vec3(signedUnit(random), signedUnit(random), 0.0f));
	}
	glGenTextures(1, &mSsaoNoiseTexture);
	glBindTexture(GL_TEXTURE_2D, mSsaoNoiseTexture);
	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGB16F,
		4,
		4,
		0,
		GL_RGB,
		GL_FLOAT,
		noise.data()
	);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);
	labelObject(GL_TEXTURE, mSsaoNoiseTexture, "SSAO Noise");
}

void RenderPipeline::destroySsaoResources() {
	if (mSsaoNoiseTexture != 0) {
		glDeleteTextures(1, &mSsaoNoiseTexture);
		mSsaoNoiseTexture = 0;
	}
}

void RenderPipeline::checkFramebuffer(const char* name) const {
	const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "Framebuffer '" << name << "' is incomplete: 0x"
			<< std::hex << status << std::dec << std::endl;
	}
}

void RenderPipeline::createRenderTargets() {
	mHalfWidth = std::max(1, mWidth / 2);
	mHalfHeight = std::max(1, mHeight / 2);

	glGenFramebuffers(1, &mGBufferFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mGBufferFbo);
	createTexture2D(mGPositionAo, GL_RGBA16F, GL_RGBA, GL_FLOAT, mWidth, mHeight, GL_NEAREST);
	createTexture2D(mGNormalRoughness, GL_RGBA16F, GL_RGBA, GL_FLOAT, mWidth, mHeight, GL_NEAREST);
	createTexture2D(mGAlbedoMetallic, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, mWidth, mHeight, GL_NEAREST);
	createTexture2D(mGDepth, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, mWidth, mHeight, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mGPositionAo, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mGNormalRoughness, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, mGAlbedoMetallic, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mGDepth, 0);
	const GLenum gBufferAttachments[] = {
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2
	};
	glDrawBuffers(3, gBufferAttachments);
	checkFramebuffer("GBuffer");

	glGenFramebuffers(1, &mHdrFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mHdrFbo);
	createTexture2D(mHdrColor, GL_RGBA16F, GL_RGBA, GL_FLOAT, mWidth, mHeight, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mHdrColor, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mGDepth, 0);
	checkFramebuffer("HDR Lighting");

	glGenFramebuffers(1, &mSsaoFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mSsaoFbo);
	createTexture2D(mSsaoTexture, GL_R16F, GL_RED, GL_FLOAT, mHalfWidth, mHalfHeight, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mSsaoTexture, 0);
	checkFramebuffer("SSAO");
	const GLfloat white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glClearBufferfv(GL_COLOR, 0, white);

	glGenFramebuffers(1, &mSsaoBlurFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mSsaoBlurFbo);
	createTexture2D(mSsaoBlurTexture, GL_R16F, GL_RED, GL_FLOAT, mHalfWidth, mHalfHeight, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mSsaoBlurTexture, 0);
	checkFramebuffer("SSAO Bilateral Blur");
	glClearBufferfv(GL_COLOR, 0, white);

	glGenFramebuffers(1, &mSsaoUpsampleFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mSsaoUpsampleFbo);
	createTexture2D(mSsaoUpsampleTexture, GL_R16F, GL_RED, GL_FLOAT, mWidth, mHeight, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mSsaoUpsampleTexture, 0);
	checkFramebuffer("SSAO Joint Bilateral Upsample");
	glClearBufferfv(GL_COLOR, 0, white);

	glGenFramebuffers(1, &mBloomExtractFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mBloomExtractFbo);
	createTexture2D(
		mBloomExtractTexture,
		GL_RGBA16F,
		GL_RGBA,
		GL_FLOAT,
		mHalfWidth,
		mHalfHeight,
		GL_LINEAR
	);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mBloomExtractTexture, 0);
	checkFramebuffer("Bloom Extract");

	glGenFramebuffers(2, mBloomFbos.data());
	for (std::size_t i = 0; i < mBloomFbos.size(); ++i) {
		glBindFramebuffer(GL_FRAMEBUFFER, mBloomFbos[i]);
		createTexture2D(
			mBloomTextures[i],
			GL_RGBA16F,
			GL_RGBA,
			GL_FLOAT,
			mHalfWidth,
			mHalfHeight,
			GL_LINEAR
		);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mBloomTextures[i], 0);
		checkFramebuffer(i == 0 ? "Bloom Ping" : "Bloom Pong");
	}
	mBloomResultTexture = mBloomExtractTexture;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	labelObject(GL_FRAMEBUFFER, mGBufferFbo, "GBuffer Pass FBO");
	labelObject(GL_FRAMEBUFFER, mHdrFbo, "Lighting Pass FBO");
	labelObject(GL_TEXTURE, mGPositionAo, "GBuffer Position AO");
	labelObject(GL_TEXTURE, mGNormalRoughness, "GBuffer Normal Roughness");
	labelObject(GL_TEXTURE, mGAlbedoMetallic, "GBuffer Albedo Metallic");
	labelObject(GL_TEXTURE, mGDepth, "GBuffer Depth");
	labelObject(GL_TEXTURE, mHdrColor, "HDR Lighting");
	labelObject(GL_TEXTURE, mSsaoBlurTexture, "SSAO Filtered Half Resolution");
	labelObject(GL_TEXTURE, mSsaoUpsampleTexture, "SSAO Full Resolution");
	labelObject(GL_TEXTURE, mBloomExtractTexture, "Bloom Bright Pass");
}

void RenderPipeline::destroyRenderTargets() {
	delete mForwardMsaa;
	delete mForwardResolve;
	mForwardMsaa = nullptr;
	mForwardResolve = nullptr;

	const GLuint framebuffers[] = {
		mGBufferFbo,
		mHdrFbo,
		mSsaoFbo,
		mSsaoBlurFbo,
		mSsaoUpsampleFbo,
		mBloomExtractFbo
	};
	glDeleteFramebuffers(6, framebuffers);
	glDeleteFramebuffers(static_cast<GLsizei>(mBloomFbos.size()), mBloomFbos.data());

	const GLuint textures[] = {
		mGPositionAo,
		mGNormalRoughness,
		mGAlbedoMetallic,
		mGDepth,
		mHdrColor,
		mSsaoTexture,
		mSsaoBlurTexture,
		mSsaoUpsampleTexture,
		mBloomExtractTexture
	};
	glDeleteTextures(9, textures);
	glDeleteTextures(static_cast<GLsizei>(mBloomTextures.size()), mBloomTextures.data());

	mGBufferFbo = 0;
	mHdrFbo = 0;
	mSsaoFbo = 0;
	mSsaoBlurFbo = 0;
	mSsaoUpsampleFbo = 0;
	mBloomExtractFbo = 0;
	mBloomFbos.fill(0);
	mGPositionAo = 0;
	mGNormalRoughness = 0;
	mGAlbedoMetallic = 0;
	mGDepth = 0;
	mHdrColor = 0;
	mSsaoTexture = 0;
	mSsaoBlurTexture = 0;
	mSsaoUpsampleTexture = 0;
	mBloomExtractTexture = 0;
	mBloomTextures.fill(0);
	mBloomResultTexture = 0;
}

void RenderPipeline::ensureForwardTargets() {
	if (mForwardMsaa != nullptr && mForwardResolve != nullptr &&
		mForwardMsaa->mWidth == static_cast<unsigned int>(mWidth) &&
		mForwardMsaa->mHeight == static_cast<unsigned int>(mHeight)) {
		return;
	}

	delete mForwardMsaa;
	delete mForwardResolve;
	mForwardMsaa = Framebuffer::createMultiSampleHDRFbo(mWidth, mHeight, 4);
	mForwardResolve = Framebuffer::createHDRFbo(mWidth, mHeight);
	labelObject(GL_FRAMEBUFFER, mForwardMsaa->mFBO, "Forward Reference MSAA FBO");
	labelObject(GL_FRAMEBUFFER, mForwardResolve->mFBO, "Forward Reference Resolve FBO");
}

void RenderPipeline::resize(int width, int height) {
	width = std::max(width, 1);
	height = std::max(height, 1);
	if (width == mWidth && height == mHeight) {
		return;
	}
	mWidth = width;
	mHeight = height;
	destroyRenderTargets();
	createRenderTargets();
}

void RenderPipeline::beginFrame() {
	mCpuFrameStart = std::chrono::steady_clock::now();
	mWorkingStats = {};
	mRenderer->resetDrawStats();
	mGpuProfiler.beginFrame();
}

void RenderPipeline::executePass(
	const std::string& name,
	const std::function<void()>& callback
) {
	beginDebugGroup(name);
	mGpuProfiler.beginPass(name);
	callback();
	mGpuProfiler.endPass();
	endDebugGroup();
}

void RenderPipeline::beginUiPass() {
	if (mUiPassActive) {
		return;
	}
	mUiPassActive = true;
	beginDebugGroup("UI");
	mGpuProfiler.beginPass("UI");
}

void RenderPipeline::endUiPass() {
	if (!mUiPassActive) {
		return;
	}
	mGpuProfiler.endPass();
	endDebugGroup();
	mUiPassActive = false;
}

void RenderPipeline::endFrame() {
	endUiPass();
	mGpuProfiler.endFrame();

	const RendererDrawStats& rendererStats = mRenderer->getDrawStats();
	mWorkingStats.drawCalls += rendererStats.drawCalls;
	mWorkingStats.triangles += rendererStats.triangles;
	mWorkingStats.passTimings = mGpuProfiler.getTimings();
	for (const GpuPassTiming& timing : mWorkingStats.passTimings) {
		if (timing.active) {
			mWorkingStats.gpuFrameMs += timing.milliseconds;
		}
	}

	const auto now = std::chrono::steady_clock::now();
	mWorkingStats.cpuFrameMs = std::chrono::duration<double, std::milli>(
		now - mCpuFrameStart
	).count();
	mWorkingStats.renderTargetMb =
		static_cast<double>(calculateRenderTargetBytes()) / kBytesPerMegabyte;
	mWorkingStats.sceneAssetMb =
		static_cast<double>(mSceneAssetBytes) / kBytesPerMegabyte;
	mWorkingStats.estimatedVramMb =
		mWorkingStats.renderTargetMb + mWorkingStats.sceneAssetMb;
	mPublishedStats = mWorkingStats;
}

void RenderPipeline::render(
	Scene* scene,
	Camera* camera,
	const std::vector<PointLight*>& pointLights,
	PbrMaterial* globalPbr,
	const ShadowPassInput& shadowInput
) {
	if (scene == nullptr || camera == nullptr || globalPbr == nullptr) {
		return;
	}

	collectScene(scene, camera);
	executePass("Shadow", [&]() { executeShadowPass(shadowInput); });

	if (mSettings.mode == PipelineMode::Deferred) {
		executePass("GBuffer", [&]() { executeGBufferPass(camera); });
		executePass("SSAO", [&]() { executeSsaoPass(camera); });
		executePass("Lighting", [&]() {
			executeLightingPass(camera, pointLights, globalPbr);
		});
	}
	else {
		executePass("Forward", [&]() {
			executeForwardOpaquePass(camera, pointLights);
		});
	}

	executePass("Transparent", [&]() {
		executeTransparentPass(camera, pointLights);
	});
	executePass("Bloom", [&]() { executeBloomPass(); });
	executePass("PostProcess", [&]() { executePostProcessPass(camera); });
}

void RenderPipeline::collectScene(Scene* scene, Camera* camera) {
	mDeferredOpaque.clear();
	mForwardOpaque.clear();
	mTransparent.clear();
	mVisibleMeshes.clear();

	const Frustum frustum(camera->getProjectionMatrix() * camera->getViewMatrix());
	collectObject(scene, camera, frustum);

	const glm::mat4 viewMatrix = camera->getViewMatrix();
	std::sort(
		mTransparent.begin(),
		mTransparent.end(),
		[&viewMatrix](const Mesh* left, const Mesh* right) {
			const float leftZ = (viewMatrix * left->getModelMatrix() * glm::vec4(0, 0, 0, 1)).z;
			const float rightZ = (viewMatrix * right->getModelMatrix() * glm::vec4(0, 0, 0, 1)).z;
			return leftZ < rightZ;
		}
	);

	if (mSceneAssetBytes == 0) {
		std::unordered_set<const Geometry*> geometries;
		std::unordered_set<const Texture*> textures;
		std::size_t instanceBytes = 0;
		estimateSceneAssets(scene, geometries, textures, instanceBytes);
		mSceneAssetBytes = instanceBytes;
		for (const Geometry* geometry : geometries) {
			mSceneAssetBytes += geometry->getEstimatedGpuBytes();
		}
		for (const Texture* texture : textures) {
			mSceneAssetBytes += texture->getEstimatedGpuBytes();
		}
	}
}

void RenderPipeline::collectObject(
	Object* object,
	Camera* camera,
	const Frustum& frustum
) {
	if (object == nullptr || !object->isVisible()) {
		return;
	}

	if (object->getType() == ObjectType::Mesh ||
		object->getType() == ObjectType::InstancedMesh) {
		Mesh* mesh = static_cast<Mesh*>(object);
		Material* material = mesh->mMaterial;
		Geometry* geometry = mesh->mGeometry;
		if (material != nullptr && geometry != nullptr) {
			bool submitMesh = true;
			++mWorkingStats.totalMeshes;

			if (object->getType() == ObjectType::InstancedMesh) {
				InstancedMesh* instanced = static_cast<InstancedMesh*>(mesh);
				const std::uint64_t logicalTotal = instanced->getLogicalInstanceTotal();
				const std::uint64_t visible = instanced->getLogicalVisibleCount();
				const std::uint64_t drawInstances = instanced->mInstanceCount;
				mWorkingStats.totalInstances += logicalTotal;
				mWorkingStats.visibleInstances += visible;
				if (drawInstances > 0) {
					++mWorkingStats.visibleMeshes;
					mWorkingStats.lodInstances[std::clamp(mesh->getActiveLod(), 0, 2)] += visible;
				}
				else {
					++mWorkingStats.culledMeshes;
					submitMesh = false;
				}
			}
			else {
				mWorkingStats.totalInstances += 1;
				const glm::vec3 localCenter =
					(geometry->getMinBounds() + geometry->getMaxBounds()) * 0.5f;
				const glm::vec3 worldCenter = glm::vec3(
					mesh->getModelMatrix() * glm::vec4(localCenter, 1.0f)
				);
				mesh->updateLod(
					glm::distance(camera->mPosition, worldCenter),
					mSettings.lodEnabled
				);
				geometry = mesh->mGeometry;

				const bool canCull = material->mType != MaterialType::CubeMaterial;
				const bool visible = !mSettings.frustumCulling || !canCull ||
					frustum.intersectsAabb(
						geometry->getMinBounds(),
						geometry->getMaxBounds(),
						mesh->getModelMatrix()
					);
				if (!visible) {
					++mWorkingStats.culledMeshes;
					submitMesh = false;
				}
				else {
					++mWorkingStats.visibleMeshes;
					++mWorkingStats.visibleInstances;
					++mWorkingStats.lodInstances[std::clamp(mesh->getActiveLod(), 0, 2)];
				}
			}

			if (submitMesh) {
				mVisibleMeshes.push_back(mesh);
				if (material->mBlend) {
					mTransparent.push_back(mesh);
				}
				else if (material->mType == MaterialType::PbrMaterial) {
					mDeferredOpaque.push_back(mesh);
				}
				else {
					mForwardOpaque.push_back(mesh);
				}
			}
		}
	}

	for (Object* child : object->getChildren()) {
		collectObject(child, camera, frustum);
	}

	mWorkingStats.culledInstances =
		mWorkingStats.totalInstances > mWorkingStats.visibleInstances
		? mWorkingStats.totalInstances - mWorkingStats.visibleInstances
		: 0;
}

void RenderPipeline::estimateSceneAssets(
	Object* object,
	std::unordered_set<const Geometry*>& geometries,
	std::unordered_set<const Texture*>& textures,
	std::size_t& instanceBytes
) {
	if (object == nullptr) {
		return;
	}

	if (object->getType() == ObjectType::Mesh ||
		object->getType() == ObjectType::InstancedMesh) {
		Mesh* mesh = static_cast<Mesh*>(object);
		if (mesh->mGeometry != nullptr) {
			geometries.insert(mesh->mGeometry);
		}
		if (object->getType() == ObjectType::InstancedMesh) {
			const InstancedMesh* instanced = static_cast<InstancedMesh*>(object);
			instanceBytes += sizeof(glm::mat4) *
				static_cast<std::size_t>(std::max(
					instanced->mInstanceCount,
					instanced->getLogicalInstanceTotal()
				));
		}

		if (mesh->mMaterial != nullptr &&
			mesh->mMaterial->mType == MaterialType::PbrMaterial) {
			PbrMaterial* material = static_cast<PbrMaterial*>(mesh->mMaterial);
			const Texture* materialTextures[] = {
				material->mAlbedo,
				material->mNormal,
				material->mRoughness,
				material->mMetallic,
				material->mAo,
				material->mIrradianceMap,
				material->mPrefilterMap,
				material->mBrdfLut
			};
			for (const Texture* texture : materialTextures) {
				if (texture != nullptr) {
					textures.insert(texture);
				}
			}
		}
		else if (mesh->mMaterial != nullptr &&
			mesh->mMaterial->mType == MaterialType::CubeMaterial) {
			CubeMaterial* material = static_cast<CubeMaterial*>(mesh->mMaterial);
			if (material->mDiffuse != nullptr) {
				textures.insert(material->mDiffuse);
			}
		}
	}

	for (Object* child : object->getChildren()) {
		estimateSceneAssets(child, geometries, textures, instanceBytes);
	}
}

void RenderPipeline::executeShadowPass(const ShadowPassInput& input) {
	if (!input.enabled || input.root == nullptr || input.target == nullptr) {
		return;
	}
	mRenderer->renderShadowMap(input.root, input.target, input.lightMatrix);
}

void RenderPipeline::executeGBufferPass(Camera* camera) {
	glBindFramebuffer(GL_FRAMEBUFFER, mGBufferFbo);
	glViewport(0, 0, mWidth, mHeight);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glDisable(GL_STENCIL_TEST);

	const GLfloat zero[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glClearBufferfv(GL_COLOR, 0, zero);
	glClearBufferfv(GL_COLOR, 1, zero);
	glClearBufferfv(GL_COLOR, 2, zero);
	glClear(GL_DEPTH_BUFFER_BIT);

	mGBufferShader->begin();
	for (Mesh* mesh : mDeferredOpaque) {
		drawGBufferMesh(mesh, camera);
	}
	mGBufferShader->end();
	glBindVertexArray(0);
}

void RenderPipeline::drawGBufferMesh(Mesh* mesh, Camera* camera) {
	PbrMaterial* material = static_cast<PbrMaterial*>(mesh->mMaterial);
	Geometry* geometry = mesh->mGeometry;
	if (material == nullptr || geometry == nullptr) {
		return;
	}

	applyFaceCulling(material);
	mGBufferShader->setMatrix4x4("modelMatrix", mesh->getModelMatrix());
	mGBufferShader->setMatrix4x4("viewMatrix", camera->getViewMatrix());
	mGBufferShader->setMatrix4x4("projectionMatrix", camera->getProjectionMatrix());
	mGBufferShader->setInt(
		"instanced",
		mesh->getType() == ObjectType::InstancedMesh ? 1 : 0
	);
	mGBufferShader->setInt("skinned", mesh->isSkinned() ? 1 : 0);
	if (mesh->isSkinned()) {
		std::vector<glm::mat4>& boneMatrices = mesh->getBoneMatrices();
		mGBufferShader->setMatrix4x4Array(
			"boneMatrices[0]",
			boneMatrices.data(),
			static_cast<int>(boneMatrices.size())
		);
	}

	const Texture* textures[] = {
		material->mAlbedo,
		material->mNormal,
		material->mMetallic,
		material->mRoughness,
		material->mAo
	};
	const char* samplers[] = {
		"albedoTex",
		"normalTex",
		"metallicTex",
		"roughnessTex",
		"aoTex"
	};
	for (int unit = 0; unit < 5; ++unit) {
		mGBufferShader->setInt(samplers[unit], unit);
		if (textures[unit] != nullptr) {
			const_cast<Texture*>(textures[unit])->setUnit(unit);
			const_cast<Texture*>(textures[unit])->bind();
		}
	}

	mGBufferShader->setVector3("baseColorFactor", glm::vec3(material->mBaseColorFactor));
	mGBufferShader->setFloat("metallicScale", material->mMetallicScale);
	mGBufferShader->setFloat("roughnessScale", material->mRoughnessScale);
	mGBufferShader->setFloat("aoScale", material->mAoScale);
	mGBufferShader->setFloat("normalStrength", material->mNormalStrength);
	mGBufferShader->setFloat("surfaceVariation", material->mSurfaceVariation);
	mGBufferShader->setFloat("opacity", material->mOpacity * material->mBaseColorFactor.a);
	mGBufferShader->setInt("metallicChannel", material->mMetallicChannel);
	mGBufferShader->setInt("roughnessChannel", material->mRoughnessChannel);
	mGBufferShader->setInt("aoChannel", material->mAoChannel);
	mGBufferShader->setInt("alphaMask", material->mAlphaMask ? 1 : 0);
	mGBufferShader->setFloat("alphaCutoff", material->mAlphaCutoff);

	const std::uint64_t instances = meshInstanceCount(mesh);
	glBindVertexArray(geometry->getVao());
	if (mesh->getType() == ObjectType::InstancedMesh) {
		static_cast<InstancedMesh*>(mesh)->bindInstanceAttributes();
		glDrawElementsInstanced(
			GL_TRIANGLES,
			geometry->getIndicesCount(),
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(instances)
		);
	}
	else {
		glDrawElements(
			GL_TRIANGLES,
			geometry->getIndicesCount(),
			GL_UNSIGNED_INT,
			nullptr
		);
	}
	++mWorkingStats.drawCalls;
	mWorkingStats.triangles +=
		static_cast<std::uint64_t>(geometry->getIndicesCount() / 3) * instances;
}

void RenderPipeline::executeSsaoPass(Camera* camera) {
	if (!mSettings.ssaoEnabled) {
		const GLfloat white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glBindFramebuffer(GL_FRAMEBUFFER, mSsaoFbo);
		glClearBufferfv(GL_COLOR, 0, white);
		glBindFramebuffer(GL_FRAMEBUFFER, mSsaoBlurFbo);
		glClearBufferfv(GL_COLOR, 0, white);
		glBindFramebuffer(GL_FRAMEBUFFER, mSsaoUpsampleFbo);
		glClearBufferfv(GL_COLOR, 0, white);
		return;
	}

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);
	glViewport(0, 0, mHalfWidth, mHalfHeight);
	glBindFramebuffer(GL_FRAMEBUFFER, mSsaoFbo);
	glClear(GL_COLOR_BUFFER_BIT);
	mSsaoShader->begin();
	mSsaoShader->setInt("gPositionAo", 0);
	mSsaoShader->setInt("gNormalRoughness", 1);
	mSsaoShader->setInt("noiseTexture", 2);
	mSsaoShader->setVector3("samples[0]", mSsaoKernel[0]);
	for (std::size_t i = 0; i < mSsaoKernel.size(); ++i) {
		mSsaoShader->setVector3(
			"samples[" + std::to_string(i) + "]",
			mSsaoKernel[i]
		);
	}
	mSsaoShader->setInt("sampleCount", std::clamp(mSettings.ssaoSamples, 1, 64));
	mSsaoShader->setFloat("radius", mSettings.ssaoRadius);
	mSsaoShader->setFloat("bias", mSettings.ssaoBias);
	mSsaoShader->setMatrix4x4("viewMatrix", camera->getViewMatrix());
	mSsaoShader->setMatrix4x4("projectionMatrix", camera->getProjectionMatrix());
	bindTexture2D(mGPositionAo, 0);
	bindTexture2D(mGNormalRoughness, 1);
	bindTexture2D(mSsaoNoiseTexture, 2);
	drawFullscreen();
	mSsaoShader->end();

	glViewport(0, 0, mHalfWidth, mHalfHeight);
	glBindFramebuffer(GL_FRAMEBUFFER, mSsaoBlurFbo);
	glClear(GL_COLOR_BUFFER_BIT);
	mSsaoBlurShader->begin();
	mSsaoBlurShader->setInt("ssaoInput", 0);
	mSsaoBlurShader->setInt("gPositionAo", 1);
	mSsaoBlurShader->setInt("gNormalRoughness", 2);
	bindTexture2D(mSsaoTexture, 0);
	bindTexture2D(mGPositionAo, 1);
	bindTexture2D(mGNormalRoughness, 2);
	drawFullscreen();
	mSsaoBlurShader->end();

	glViewport(0, 0, mWidth, mHeight);
	glBindFramebuffer(GL_FRAMEBUFFER, mSsaoUpsampleFbo);
	glClear(GL_COLOR_BUFFER_BIT);
	mSsaoUpsampleShader->begin();
	mSsaoUpsampleShader->setInt("ssaoInput", 0);
	mSsaoUpsampleShader->setInt("gPositionAo", 1);
	mSsaoUpsampleShader->setInt("gNormalRoughness", 2);
	bindTexture2D(mSsaoBlurTexture, 0);
	bindTexture2D(mGPositionAo, 1);
	bindTexture2D(mGNormalRoughness, 2);
	drawFullscreen();
	mSsaoUpsampleShader->end();
}

void RenderPipeline::executeLightingPass(
	Camera* camera,
	const std::vector<PointLight*>& pointLights,
	PbrMaterial* globalPbr
) {
	glBindFramebuffer(GL_FRAMEBUFFER, mHdrFbo);
	glViewport(0, 0, mWidth, mHeight);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);
	glClear(GL_COLOR_BUFFER_BIT);

	GLfloat clearColor[4]{};
	glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
	mLightingShader->begin();
	mLightingShader->setInt("gPositionAo", 0);
	mLightingShader->setInt("gNormalRoughness", 1);
	mLightingShader->setInt("gAlbedoMetallic", 2);
	mLightingShader->setInt("ssaoTexture", 3);
	mLightingShader->setInt("irradianceMap", 4);
	mLightingShader->setInt("prefilterMap", 5);
	mLightingShader->setInt("brdfLUT", 6);
	mLightingShader->setInt("softShadowMap", 7);
	bindTexture2D(mGPositionAo, 0);
	bindTexture2D(mGNormalRoughness, 1);
	bindTexture2D(mGAlbedoMetallic, 2);
	bindTexture2D(mSsaoUpsampleTexture, 3);
	if (globalPbr->mIrradianceMap != nullptr) {
		globalPbr->mIrradianceMap->setUnit(4);
		globalPbr->mIrradianceMap->bind();
	}
	if (globalPbr->mPrefilterMap != nullptr) {
		globalPbr->mPrefilterMap->setUnit(5);
		globalPbr->mPrefilterMap->bind();
	}
	if (globalPbr->mBrdfLut != nullptr) {
		globalPbr->mBrdfLut->setUnit(6);
		globalPbr->mBrdfLut->bind();
	}
	if (mRenderer->mSoftShadowMap != nullptr) {
		mRenderer->mSoftShadowMap->setUnit(7);
		mRenderer->mSoftShadowMap->bind();
	}

	mLightingShader->setVector3("cameraPosition", camera->mPosition);
	mLightingShader->setVector3("clearColor", clearColor[0], clearColor[1], clearColor[2]);
	mLightingShader->setFloat("envIntensity", globalPbr->mEnvIntensity);
	mLightingShader->setFloat("maxReflectionLod", globalPbr->mMaxReflectionLod);
	mLightingShader->setFloat("ssaoStrength", mSettings.ssaoStrength);
	mLightingShader->setInt("ssaoEnabled", mSettings.ssaoEnabled ? 1 : 0);
	mLightingShader->setInt("lightingDebugView", globalPbr->mDebugView);
	mLightingShader->setInt(
		"softShadowEnabled",
		mRenderer->mSoftShadowEnabled && mRenderer->mSoftShadowMap != nullptr ? 1 : 0
	);
	mLightingShader->setMatrix4x4("softShadowMatrix", mRenderer->mSoftShadowMatrix);
	mLightingShader->setVector3(
		"softShadowLightPosition",
		mRenderer->mSoftShadowLightPosition
	);
	mLightingShader->setFloat("softShadowStrength", mRenderer->mSoftShadowStrength);
	mLightingShader->setFloat("softShadowBias", mRenderer->mSoftShadowBias);
	mLightingShader->setFloat("softShadowRadius", mRenderer->mSoftShadowRadius);
	for (int i = 0; i < 4; ++i) {
		const bool available = static_cast<std::size_t>(i) < pointLights.size();
		mLightingShader->setVector3(
			"pointLights[" + std::to_string(i) + "].color",
			available ? pointLights[i]->mColor : glm::vec3(0.0f)
		);
		mLightingShader->setVector3(
			"pointLights[" + std::to_string(i) + "].position",
			available ? pointLights[i]->getPosition() : glm::vec3(0.0f)
		);
	}
	drawFullscreen();
	mLightingShader->end();
}

void RenderPipeline::executeForwardOpaquePass(
	Camera* camera,
	const std::vector<PointLight*>& pointLights
) {
	ensureForwardTargets();
	glBindFramebuffer(GL_FRAMEBUFFER, mForwardMsaa->mFBO);
	glViewport(0, 0, mWidth, mHeight);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	for (Mesh* mesh : mDeferredOpaque) {
		mRenderer->renderObject(mesh, camera, pointLights);
	}
	for (Mesh* mesh : mForwardOpaque) {
		mRenderer->renderObject(mesh, camera, pointLights);
	}
}

void RenderPipeline::executeTransparentPass(
	Camera* camera,
	const std::vector<PointLight*>& pointLights
) {
	if (mSettings.mode == PipelineMode::Deferred) {
		glBindFramebuffer(GL_FRAMEBUFFER, mHdrFbo);
		glViewport(0, 0, mWidth, mHeight);
		for (Mesh* mesh : mForwardOpaque) {
			mRenderer->renderObject(mesh, camera, pointLights);
		}
		for (Mesh* mesh : mTransparent) {
			mRenderer->renderObject(mesh, camera, pointLights);
		}
		if (mSettings.showBounds) {
			drawDebugBounds(camera);
		}
	}
	else {
		ensureForwardTargets();
		glBindFramebuffer(GL_FRAMEBUFFER, mForwardMsaa->mFBO);
		glViewport(0, 0, mWidth, mHeight);
		for (Mesh* mesh : mTransparent) {
			mRenderer->renderObject(mesh, camera, pointLights);
		}
		if (mSettings.showBounds) {
			drawDebugBounds(camera);
		}
		mRenderer->msaaResolve(mForwardMsaa, mForwardResolve);
	}
}

void RenderPipeline::drawDebugBounds(Camera* camera) {
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glLineWidth(1.0f);
	mDebugBoundsShader->begin();
	mDebugBoundsShader->setMatrix4x4("viewMatrix", camera->getViewMatrix());
	mDebugBoundsShader->setMatrix4x4("projectionMatrix", camera->getProjectionMatrix());
	mDebugBoundsShader->setVector3("boundsColor", glm::vec3(0.15f, 0.95f, 0.45f));
	glBindVertexArray(mDebugBox->getVao());
	for (Mesh* mesh : mVisibleMeshes) {
		if (mesh->getType() == ObjectType::InstancedMesh || mesh->mGeometry == nullptr ||
			mesh->mMaterial->mType == MaterialType::CubeMaterial) {
			continue;
		}
		const glm::vec3 minimum = mesh->mGeometry->getMinBounds();
		const glm::vec3 maximum = mesh->mGeometry->getMaxBounds();
		const glm::vec3 center = (minimum + maximum) * 0.5f;
		const glm::vec3 size = glm::max(maximum - minimum, glm::vec3(0.001f));
		glm::mat4 bounds = mesh->getModelMatrix();
		bounds = glm::translate(bounds, center);
		bounds = glm::scale(bounds, size);
		mDebugBoundsShader->setMatrix4x4("boundsMatrix", bounds);
		glDrawElements(
			GL_TRIANGLES,
			mDebugBox->getIndicesCount(),
			GL_UNSIGNED_INT,
			nullptr
		);
		++mWorkingStats.drawCalls;
		mWorkingStats.triangles += mDebugBox->getIndicesCount() / 3;
	}
	mDebugBoundsShader->end();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDepthMask(GL_TRUE);
}

void RenderPipeline::executeBloomPass() {
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);
	glViewport(0, 0, mHalfWidth, mHalfHeight);

	if (!mSettings.bloomEnabled) {
		const GLfloat black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		glBindFramebuffer(GL_FRAMEBUFFER, mBloomExtractFbo);
		glClearBufferfv(GL_COLOR, 0, black);
		mBloomResultTexture = mBloomExtractTexture;
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, mBloomExtractFbo);
	glClear(GL_COLOR_BUFFER_BIT);
	mBloomExtractShader->begin();
	mBloomExtractShader->setInt("hdrScene", 0);
	mBloomExtractShader->setFloat("threshold", mSettings.bloomThreshold);
	bindTexture2D(activeHdrTexture(), 0);
	drawFullscreen();
	mBloomExtractShader->end();

	GLuint inputTexture = mBloomExtractTexture;
	bool horizontal = true;
	const int passCount = std::max(1, mSettings.bloomIterations) * 2;
	mGaussianBlurShader->begin();
	for (int pass = 0; pass < passCount; ++pass) {
		const std::size_t target = horizontal ? 0 : 1;
		glBindFramebuffer(GL_FRAMEBUFFER, mBloomFbos[target]);
		mGaussianBlurShader->setInt("image", 0);
		mGaussianBlurShader->setInt("horizontal", horizontal ? 1 : 0);
		bindTexture2D(inputTexture, 0);
		drawFullscreen();
		inputTexture = mBloomTextures[target];
		horizontal = !horizontal;
	}
	mGaussianBlurShader->end();
	mBloomResultTexture = inputTexture;
}

void RenderPipeline::executePostProcessPass(Camera* camera) {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, mWidth, mHeight);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);
	glClear(GL_COLOR_BUFFER_BIT);

	int debugView = static_cast<int>(mSettings.debugView);
	if (mSettings.mode == PipelineMode::ForwardReference &&
		mSettings.debugView != PipelineDebugView::Final &&
		mSettings.debugView != PipelineDebugView::Bloom) {
		debugView = static_cast<int>(PipelineDebugView::Final);
	}
	mPostProcessShader->begin();
	mPostProcessShader->setInt("hdrScene", 0);
	mPostProcessShader->setInt("bloomTexture", 1);
	mPostProcessShader->setInt("gPositionAo", 2);
	mPostProcessShader->setInt("gNormalRoughness", 3);
	mPostProcessShader->setInt("gAlbedoMetallic", 4);
	mPostProcessShader->setInt("ssaoTexture", 5);
	mPostProcessShader->setInt("depthTexture", 6);
	mPostProcessShader->setFloat("exposure", mSettings.exposure);
	mPostProcessShader->setFloat("bloomIntensity", mSettings.bloomIntensity);
	mPostProcessShader->setFloat("ssaoStrength", mSettings.ssaoStrength);
	mPostProcessShader->setFloat("cameraNear", std::max(camera->mNear, 0.001f));
	mPostProcessShader->setFloat("cameraFar", std::max(camera->mFar, camera->mNear + 0.001f));
	mPostProcessShader->setInt("bloomEnabled", mSettings.bloomEnabled ? 1 : 0);
	mPostProcessShader->setInt("toneMapper", static_cast<int>(mSettings.toneMapper));
	mPostProcessShader->setInt("debugView", debugView);
	bindTexture2D(activeHdrTexture(), 0);
	bindTexture2D(mBloomResultTexture, 1);
	bindTexture2D(mGPositionAo, 2);
	bindTexture2D(mGNormalRoughness, 3);
	bindTexture2D(mGAlbedoMetallic, 4);
	bindTexture2D(mSsaoUpsampleTexture, 5);
	bindTexture2D(mGDepth, 6);
	drawFullscreen();
	mPostProcessShader->end();
	glDepthMask(GL_TRUE);
}

void RenderPipeline::drawFullscreen() {
	glBindVertexArray(mFullscreenQuad->getVao());
	glDrawElements(
		GL_TRIANGLES,
		mFullscreenQuad->getIndicesCount(),
		GL_UNSIGNED_INT,
		nullptr
	);
	++mWorkingStats.drawCalls;
	mWorkingStats.triangles += mFullscreenQuad->getIndicesCount() / 3;
}

GLuint RenderPipeline::activeHdrTexture() const {
	if (mSettings.mode == PipelineMode::ForwardReference &&
		mForwardResolve != nullptr && mForwardResolve->mColorAttachment != nullptr) {
		return mForwardResolve->mColorAttachment->getTexture();
	}
	return mHdrColor;
}

GLuint RenderPipeline::texture(PipelineTexture textureType) const {
	switch (textureType) {
	case PipelineTexture::PositionAo:
		return mGPositionAo;
	case PipelineTexture::NormalRoughness:
		return mGNormalRoughness;
	case PipelineTexture::AlbedoMetallic:
		return mGAlbedoMetallic;
	case PipelineTexture::Ssao:
		return mSsaoUpsampleTexture;
	case PipelineTexture::Bloom:
		return mBloomResultTexture;
	case PipelineTexture::Hdr:
		return activeHdrTexture();
	case PipelineTexture::Depth:
		return mGDepth;
	default:
		return 0;
	}
}

std::size_t RenderPipeline::calculateRenderTargetBytes() const {
	const std::size_t fullPixels =
		static_cast<std::size_t>(mWidth) * static_cast<std::size_t>(mHeight);
	const std::size_t halfPixels =
		static_cast<std::size_t>(mHalfWidth) * static_cast<std::size_t>(mHalfHeight);
	std::size_t bytes = fullPixels * 34 + halfPixels * 28 + 16 * 3 * sizeof(float);

	const Framebuffer* forwardTargets[] = { mForwardMsaa, mForwardResolve };
	for (const Framebuffer* target : forwardTargets) {
		if (target == nullptr) {
			continue;
		}
		const Texture* attachments[] = {
			target->mColorAttachment,
			target->mDepthStencilAttachment,
			target->mDepthAttachment
		};
		for (const Texture* attachment : attachments) {
			if (attachment != nullptr) {
				bytes += attachment->getEstimatedGpuBytes();
			}
		}
	}
	return bytes;
}
