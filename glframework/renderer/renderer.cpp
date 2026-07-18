#include "renderer.h"
#include <iostream>
#include "../material/phongMaterial.h"
#include "../material/whiteMaterial.h"
#include "../material/opacityMaskMaterial.h"
#include "../material/screenMaterial.h"
#include "../material/cubeMaterial.h"
#include "../material/phongEnvMaterial.h"
#include "../material/phongInstanceMaterial.h"
#include "../material/grassInstanceMaterial.h"
#include "../material/advanced/phongNormalMaterial.h"
#include "../material/advanced/phongParallaxMaterial.h"
#include "../material/advanced/phongShadowMaterial.h"
#include "../material/advanced/phongCSMShadowMaterial.h"
#include "../material/advanced/pbrMaterial.h"


#include "../mesh/instancedMesh.h"
#include "../../application/camera/orthographicCamera.h"

#include "../light/shadow/directionalLightShadow.h"
#include "../light/shadow/directionalLightCSMShadow.h"
#include <string>//stl string
#include <algorithm>


Renderer::Renderer() {
	//mPhongShader = new Shader("assets/shaders/phong.vert", "assets/shaders/phong.frag");
	mWhiteShader = new Shader("assets/shaders/white.vert", "assets/shaders/white.frag");
	mDepthShader = new Shader("assets/shaders/depth.vert", "assets/shaders/depth.frag");
	mOpacityMaskShader = new Shader("assets/shaders/phongOpacityMask.vert", "assets/shaders/phongOpacityMask.frag");
	mScreenShader = new Shader("assets/shaders/screen.vert", "assets/shaders/screen.frag");
	mCubeShader = new Shader("assets/shaders/cube.vert", "assets/shaders/cube.frag");
	mPhongEnvShader = new Shader("assets/shaders/phongEnv.vert", "assets/shaders/phongEnv.frag");
	mPhongInstanceShader = new Shader("assets/shaders/phongInstance.vert", "assets/shaders/phongInstance.frag");
	mGrassInstanceShader = new Shader("assets/shaders/grassInstance.vert", "assets/shaders/grassInstance.frag");

	mPhongNormalShader = new Shader("assets/shaders/advanced/phongNormal.vert", "assets/shaders/advanced/phongNormal.frag");
	mPhongParallaxShader = new Shader("assets/shaders/advanced/phongParallax.vert", "assets/shaders/advanced/phongParallax.frag");
	mPhongShader = new Shader("assets/shaders/advanced/phong.vert", "assets/shaders/advanced/phong.frag");
	mShadowShader = new Shader("assets/shaders/advanced/shadow.vert", "assets/shaders/advanced/shadow.frag");
	mPhongShadowShader = new Shader("assets/shaders/advanced/phongShadow.vert", "assets/shaders/advanced/phongShadow.frag");
	mPhongCSMShadowShader = new Shader("assets/shaders/advanced/phongCSMShadow.vert", "assets/shaders/advanced/phongCSMShadow.frag");
	mPbrShader = new Shader("assets/shaders/advanced/pbr/pbr.vert", "assets/shaders/advanced/pbr/pbr.frag");

}

Renderer::~Renderer() {
	delete mPhongShader;
	delete mWhiteShader;
	delete mDepthShader;
	delete mOpacityMaskShader;
	delete mScreenShader;
	delete mCubeShader;
	delete mPhongEnvShader;
	delete mPhongInstanceShader;
	delete mGrassInstanceShader;
	delete mPhongNormalShader;
	delete mPhongParallaxShader;
	delete mShadowShader;
	delete mPhongShadowShader;
	delete mPhongCSMShadowShader;
	delete mPbrShader;
}

void Renderer::recordDraw(const Geometry* geometry, std::uint64_t instanceCount) {
	if (geometry == nullptr || instanceCount == 0) {
		return;
	}
	mDrawStats.drawCalls += 1;
	mDrawStats.triangles +=
		static_cast<std::uint64_t>(geometry->getIndicesCount() / 3u) * instanceCount;
}

void Renderer::setClearColor(glm::vec3 color) {
	glClearColor(color.r, color.g, color.b, 1.0);
}


void Renderer::msaaResolve(Framebuffer* src, Framebuffer* dst) {
	glBindFramebuffer(GL_READ_FRAMEBUFFER, src->mFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst->mFBO);
	glBlitFramebuffer(0, 0, src->mWidth, src->mHeight, 0, 0, dst->mWidth, dst->mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}


void Renderer::render(
	Scene* scene,
	Camera* camera,
	const std::vector<PointLight*>& pointLights,
	unsigned int fbo
) {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	////1 设置当前帧绘制的时候，opengl的必要状态机参数
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);

	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_POLYGON_OFFSET_LINE);


	//开启测试、设置基本写入状态，打开模板测试写入
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilMask(0xFF);//保证了模板缓冲可以被清理

	//默认颜色混合
	glDisable(GL_BLEND);

	// 清理当前帧缓冲。
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);


	//清空两个队列
	mOpacityObjects.clear();
	mTransparentObjects.clear();

	projectObject(scene);

	std::sort(
		mTransparentObjects.begin(), 
		mTransparentObjects.end(),
		[camera](const Mesh* a, const Mesh* b) {
			auto viewMatrix = camera->getViewMatrix();

			//1 计算a的相机系的Z
			auto modelMatrixA = a->getModelMatrix();
			auto worldPositionA = modelMatrixA * glm::vec4(0.0, 0.0, 0.0, 1.0);
			auto cameraPositionA = viewMatrix * worldPositionA;

			//2 计算b的相机系的Z
			auto modelMatrixB = b->getModelMatrix();
			auto worldPositionB = modelMatrixB * glm::vec4(0.0, 0.0, 0.0, 1.0);
			auto cameraPositionB = viewMatrix * worldPositionB;

			return cameraPositionA.z < cameraPositionB.z;
		}
	);

	//渲染Shadowmap
	//renderShadowMap(camera, mOpacityObjects, dirLight);

	//3 渲染两个队列
	for (int i = 0; i < mOpacityObjects.size(); i++) {
		renderObject(mOpacityObjects[i], camera, pointLights);
	}

	for (int i = 0; i < mTransparentObjects.size(); i++) {
		renderObject(mTransparentObjects[i], camera, pointLights);
	}
}

void Renderer::projectObject(Object* obj) {
	if (!obj->isVisible()) {
		return;
	}

	if (obj->getType() == ObjectType::Mesh || obj->getType() == ObjectType::InstancedMesh) {
		Mesh* mesh = (Mesh*)obj;
		auto material = mesh->mMaterial;
		if (material->mBlend) {
			mTransparentObjects.push_back(mesh);
		}
		else {
			mOpacityObjects.push_back(mesh);
		}
	}

	auto children = obj->getChildren();
	for (int i = 0; i < children.size(); i++) {
		projectObject(children[i]);
	}
}

Shader* Renderer::pickShader(MaterialType type) {
	Shader* result = nullptr;

	switch (type) {
	case MaterialType::PhongMaterial:
		result = mPhongShader;
		break;
	case MaterialType::WhiteMaterial:
		result = mWhiteShader;
		break;
	case MaterialType::DepthMaterial:
		result = mDepthShader;
		break;
	case MaterialType::OpacityMaskMaterial:
		result = mOpacityMaskShader;
		break;
	case MaterialType::ScreenMaterial:
		result = mScreenShader;
		break;
	case MaterialType::CubeMaterial:
		result = mCubeShader;
		break;
	case MaterialType::PhongEnvMaterial:
		result = mPhongEnvShader;
		break;
	case MaterialType::PhongInstanceMaterial:
		result = mPhongInstanceShader;
		break;
	case MaterialType::GrassInstanceMaterial:
		result = mGrassInstanceShader;
		break;
	case MaterialType::PhongNormalMaterial:
		result = mPhongNormalShader;
		break;
	case MaterialType::PhongParallaxMaterial:
		result = mPhongParallaxShader;
		break;
	case MaterialType::PhongShadowMaterial:
		result = mPhongShadowShader;
		break;
	case MaterialType::PhongCSMShadowMaterial:
		result = mPhongCSMShadowShader;
		break;
	case MaterialType::PbrMaterial:
		result = mPbrShader;
		break;

	default:
		std::cout << "Unknown material type to pick shader" << std::endl;
		break;
	}

	return result;
}

void Renderer::renderShadowMap(
	Camera* camera,
	const std::vector<Mesh*>& meshes,
	DirectionalLight* dirLight) {
	//1 确保现在的绘制不是postProcessPass的绘制，如果是，则不执行渲染
	bool isPostProcessPass = true;
	for (int i = 0; i < meshes.size(); i++) {
		auto mesh = meshes[i];
		if (mesh->mMaterial->mType != MaterialType::ScreenMaterial) {
			isPostProcessPass = false;
			break;
		}
	}
	if (isPostProcessPass) {
		return;
	}

	//2 保存原始状态，绘制shadowMap完毕后，要恢复原始状态
	GLint preFbo;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &preFbo);

	GLint preViewport[4];
	glGetIntegerv(GL_VIEWPORT, preViewport);


	//3 设置ShadowPass绘制的时候所需的状态
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);

	DirectionalLightCSMShadow* csmShadow = (DirectionalLightCSMShadow*)dirLight->mShadow;
	std::vector<float>layers;
	csmShadow->generateCascadeLayers(layers, camera->mNear, camera->mFar);
	auto lightMatrices = csmShadow->getLightMatrices(camera, dirLight->getDirection(), layers);

	glBindFramebuffer(GL_FRAMEBUFFER, csmShadow->mRenderTarget->mFBO);
	glViewport(0, 0, csmShadow->mRenderTarget->mWidth, csmShadow->mRenderTarget->mHeight);

	//4 循环为每个子视锥体渲染shadowmap
	for (int i = 0; i < csmShadow->mLayerCount; i++) {
		glFramebufferTextureLayer(
			GL_FRAMEBUFFER, 
			GL_DEPTH_ATTACHMENT, 
			csmShadow->mRenderTarget->mDepthAttachment->getTexture(),
			0, i);

		glClear(GL_DEPTH_BUFFER_BIT);//!!!别忘了
		mShadowShader->begin();
		mShadowShader->setMatrix4x4("lightMatrix", lightMatrices[i]);
		for (int i = 0; i < meshes.size(); i++) {
			auto mesh = meshes[i];
			auto geometry = mesh->mGeometry;

			glBindVertexArray(geometry->getVao());
			mShadowShader->setMatrix4x4("modelMatrix", mesh->getModelMatrix());

			if (mesh->getType() == ObjectType::InstancedMesh) {
				InstancedMesh* im = (InstancedMesh*)mesh;
				glDrawElementsInstanced(GL_TRIANGLES, geometry->getIndicesCount(), GL_UNSIGNED_INT, 0, im->mInstanceCount);
				recordDraw(geometry, im->mInstanceCount);
			}
			else {
				glDrawElements(GL_TRIANGLES, geometry->getIndicesCount(), GL_UNSIGNED_INT, 0);
				recordDraw(geometry);
			}
		}
		mShadowShader->end();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, preFbo);
	glViewport(preViewport[0], preViewport[1], preViewport[2], preViewport[3]);
}

void Renderer::renderShadowMap(
	Object* root,
	Framebuffer* target,
	const glm::mat4& lightMatrix
) {
	if (root == nullptr || target == nullptr || mShadowShader == nullptr) {
		return;
	}

	GLint previousFbo = 0;
	GLint previousViewport[4]{};
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
	glGetIntegerv(GL_VIEWPORT, previousViewport);

	glBindFramebuffer(GL_FRAMEBUFFER, target->mFBO);
	glViewport(0, 0, target->mWidth, target->mHeight);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.0f, 4.0f);
	glClear(GL_DEPTH_BUFFER_BIT);

	mShadowShader->begin();
	mShadowShader->setMatrix4x4("lightMatrix", lightMatrix);
	renderDepthObject(root);
	mShadowShader->end();

	glDisable(GL_POLYGON_OFFSET_FILL);
	glBindFramebuffer(GL_FRAMEBUFFER, previousFbo);
	glViewport(
		previousViewport[0],
		previousViewport[1],
		previousViewport[2],
		previousViewport[3]
	);
}

void Renderer::renderDepthObject(Object* object) {
	if (object == nullptr || !object->isVisible()) {
		return;
	}

	if (object->getType() == ObjectType::Mesh || object->getType() == ObjectType::InstancedMesh) {
		Mesh* mesh = static_cast<Mesh*>(object);
		Geometry* geometry = mesh->mGeometry;
		mShadowShader->setMatrix4x4("modelMatrix", mesh->getModelMatrix());
		mShadowShader->setInt(
			"instanced",
			object->getType() == ObjectType::InstancedMesh ? 1 : 0
		);
		mShadowShader->setInt("skinned", mesh->isSkinned() ? 1 : 0);
		if (mesh->isSkinned()) {
			std::vector<glm::mat4>& boneMatrices = mesh->getBoneMatrices();
			mShadowShader->setMatrix4x4Array(
				"boneMatrices[0]",
				boneMatrices.data(),
				static_cast<int>(boneMatrices.size())
			);
		}
		glBindVertexArray(geometry->getVao());
		if (object->getType() == ObjectType::InstancedMesh) {
			InstancedMesh* instancedMesh = static_cast<InstancedMesh*>(mesh);
			glDrawElementsInstanced(
				GL_TRIANGLES,
				geometry->getIndicesCount(),
				GL_UNSIGNED_INT,
				nullptr,
				instancedMesh->mInstanceCount
			);
			recordDraw(geometry, instancedMesh->mInstanceCount);
		}
		else {
			glDrawElements(
				GL_TRIANGLES,
				geometry->getIndicesCount(),
				GL_UNSIGNED_INT,
				nullptr
			);
			recordDraw(geometry);
		}
	}

	for (Object* child : object->getChildren()) {
		renderDepthObject(child);
	}
}


//针对单个object进行渲染
void Renderer::renderObject(
	
	Object* object,
	Camera* camera,
	const std::vector<PointLight*>& pointLights
) {
	//判断是Mesh还是Object，如果是Mesh需要渲染
	if (object->getType() == ObjectType::Mesh || object->getType() == ObjectType::InstancedMesh) {
		auto mesh = (Mesh*)object;
		auto geometry = mesh->mGeometry;

		//考察是否拥有全局材质
		Material* material = nullptr;
		if (mGlobalMaterial != nullptr) {
			material = mGlobalMaterial;
		}
		else {
			material = mesh->mMaterial;
		}
		
		//设置渲染状态
		setDepthState(material);
		setPolygonOffsetState(material);
		setStencilState(material);
		setBlendState(material);
		setFaceCullingState(material);

		// 按材质类型选择着色器。
		Shader* shader = pickShader(material->mType);
		if (shader == nullptr) {
			return;
		}

		//2 更新shader的uniform
		shader->begin();

		switch (material->mType) {
		
									
		case MaterialType::WhiteMaterial: {
			//mvp
			shader->setMatrix4x4("modelMatrix", mesh->getModelMatrix());
			shader->setMatrix4x4("viewMatrix", camera->getViewMatrix());
			shader->setMatrix4x4("projectionMatrix", camera->getProjectionMatrix());
		}
										break;
		
									
		case MaterialType::ScreenMaterial: {
			ScreenMaterial* screenMat = (ScreenMaterial*)material;
			shader->setInt("screenTexSampler", 0);

			//凑合了一下
			shader->setFloat("texWidth", 1600);
			shader->setFloat("texHeight", 1200);

			screenMat->mScreenTexture->bind();
			shader->setFloat("exposure", screenMat->mExposure);

		}
										 break;
		case MaterialType::CubeMaterial: {
			CubeMaterial* cubeMat = (CubeMaterial*)material;
			mesh->setPosition(camera->mPosition);
			//mvp
			shader->setMatrix4x4("modelMatrix", mesh->getModelMatrix());
			shader->setMatrix4x4("viewMatrix", camera->getViewMatrix());
			shader->setMatrix4x4("projectionMatrix", camera->getProjectionMatrix());

			shader->setInt("sphericalSampler", 0);
			shader->setFloat("environmentIntensity", cubeMat->mIntensity);
			shader->setFloat("environmentBlackLevel", cubeMat->mBlackLevel);
			shader->setFloat("starIntensity", cubeMat->mStarIntensity);
			cubeMat->mDiffuse->bind();
		}
										break;
		
		case MaterialType::PbrMaterial: {
			PbrMaterial* pbrMat = (PbrMaterial*)material;

			//mvp以及基础参数
			shader->setMatrix4x4("modelMatrix", mesh->getModelMatrix());
			shader->setMatrix4x4("viewMatrix", camera->getViewMatrix());
			shader->setMatrix4x4("projectionMatrix", camera->getProjectionMatrix());
			auto normalMatrix = glm::mat3(glm::transpose(glm::inverse(mesh->getModelMatrix())));
			shader->setMatrix3x3("normalMatrix", normalMatrix);
			shader->setVector3("cameraPosition", camera->mPosition);
			shader->setInt("instanced", object->getType() == ObjectType::InstancedMesh ? 1 : 0);
			shader->setInt("skinned", mesh->isSkinned() ? 1 : 0);
			if (mesh->isSkinned()) {
				std::vector<glm::mat4>& boneMatrices = mesh->getBoneMatrices();
				shader->setMatrix4x4Array(
					"boneMatrices[0]",
					boneMatrices.data(),
					static_cast<int>(boneMatrices.size())
				);
			}

			//pbr相关参数更新
			
			shader->setInt("albedoTex", 0);
			pbrMat->mAlbedo->setUnit(0);
			pbrMat->mAlbedo->bind();

			shader->setInt("normalTex", 1);
			pbrMat->mNormal->setUnit(1);
			pbrMat->mNormal->bind();

			shader->setInt("roughnessTex", 2);
			pbrMat->mRoughness->setUnit(2);
			pbrMat->mRoughness->bind();

			shader->setInt("metallicTex", 3);
			pbrMat->mMetallic->setUnit(3);
			pbrMat->mMetallic->bind();

			shader->setInt("aoTex", 4);
			pbrMat->mAo->setUnit(4);
			pbrMat->mAo->bind();

			shader->setInt("irradianceMap", 5);
			pbrMat->mIrradianceMap->setUnit(5);
			pbrMat->mIrradianceMap->bind();

			shader->setInt("prefilterMap", 6);
			pbrMat->mPrefilterMap->setUnit(6);
			pbrMat->mPrefilterMap->bind();

			shader->setInt("brdfLUT", 7);
			pbrMat->mBrdfLut->setUnit(7);
			pbrMat->mBrdfLut->bind();
			shader->setFloat("envIntensity", pbrMat->mEnvIntensity);
			shader->setFloat("maxReflectionLod", pbrMat->mMaxReflectionLod);
			shader->setFloat("metallicScale", pbrMat->mMetallicScale);
			shader->setFloat("roughnessScale", pbrMat->mRoughnessScale);
			shader->setFloat("aoScale", pbrMat->mAoScale);
			shader->setFloat("normalStrength", pbrMat->mNormalStrength);
			shader->setFloat("surfaceVariation", pbrMat->mSurfaceVariation);
			shader->setVector3("baseColorFactor", glm::vec3(pbrMat->mBaseColorFactor));
			shader->setFloat("opacity", pbrMat->mOpacity);
			shader->setInt("alphaMask", pbrMat->mAlphaMask ? 1 : 0);
			shader->setFloat("alphaCutoff", pbrMat->mAlphaCutoff);
			shader->setInt("metallicChannel", pbrMat->mMetallicChannel);
			shader->setInt("roughnessChannel", pbrMat->mRoughnessChannel);
			shader->setInt("aoChannel", pbrMat->mAoChannel);
			shader->setInt("debugView", pbrMat->mDebugView);
			shader->setInt("softShadowEnabled", mSoftShadowEnabled ? 1 : 0);
			shader->setMatrix4x4("softShadowMatrix", mSoftShadowMatrix);
			shader->setVector3("softShadowLightPosition", mSoftShadowLightPosition);
			shader->setFloat("softShadowStrength", mSoftShadowStrength);
			shader->setFloat("softShadowBias", mSoftShadowBias);
			shader->setFloat("softShadowRadius", mSoftShadowRadius);
			if (mSoftShadowEnabled && mSoftShadowMap != nullptr) {
				shader->setInt("softShadowMap", 8);
				mSoftShadowMap->setUnit(8);
				mSoftShadowMap->bind();
			}

			for (int i = 0; i < 4; i++) {
				if (static_cast<size_t>(i) < pointLights.size()) {
					shader->setVector3("pointLights[" + std::to_string(i) + "].color", pointLights[i]->mColor);
					shader->setVector3("pointLights[" + std::to_string(i) + "].position", pointLights[i]->getPosition());
				}
				else {
					shader->setVector3("pointLights[" + std::to_string(i) + "].color", glm::vec3(0.0f));
					shader->setVector3("pointLights[" + std::to_string(i) + "].position", glm::vec3(0.0f));
				}
			}

		}
									  break;

		default:
			break;
		}

		//3 绑定vao
		glBindVertexArray(geometry->getVao());

		//4 执行绘制命令
		if (object->getType() == ObjectType::InstancedMesh) {
			InstancedMesh* im = (InstancedMesh*)mesh;
			glDrawElementsInstanced(GL_TRIANGLES, geometry->getIndicesCount(), GL_UNSIGNED_INT, 0, im->mInstanceCount);
			recordDraw(geometry, im->mInstanceCount);
		}
		else {
			glDrawElements(GL_TRIANGLES, geometry->getIndicesCount(), GL_UNSIGNED_INT, 0);
			recordDraw(geometry);
		}
	}
}

void Renderer::setDepthState(Material* material) {
	if (material->mDepthTest) {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(material->mDepthFunc);
	}
	else {
		glDisable(GL_DEPTH_TEST);
	}

	if (material->mDepthWrite) {
		glDepthMask(GL_TRUE);
	}
	else {
		glDepthMask(GL_FALSE);
	}
}
void Renderer::setPolygonOffsetState(Material* material) {
	if (material->mPolygonOffset) {
		glEnable(material->mPolygonOffsetType);
		glPolygonOffset(material->mFactor, material->mUnit);
	}
	else {
		glDisable(GL_POLYGON_OFFSET_FILL);
		glDisable(GL_POLYGON_OFFSET_LINE);
	}
}

void Renderer::setStencilState(Material* material) {
	if (material->mStencilTest) {
		glEnable(GL_STENCIL_TEST);

		glStencilOp(material->mSFail, material->mZFail, material->mZPass);
		glStencilMask(material->mStencilMask);
		glStencilFunc(material->mStencilFunc, material->mStencilRef, material->mStencilFuncMask);

	}
	else {
		glDisable(GL_STENCIL_TEST);
	}
}

void Renderer::setBlendState(Material* material) {
	if (material->mBlend) {
		glEnable(GL_BLEND);
		glBlendFunc(material->mSFactor, material->mDFactor);
	}
	else {
		glDisable(GL_BLEND);
	}
}

void Renderer::setFaceCullingState(Material* material) {
	if (material->mFaceCulling) {
		glEnable(GL_CULL_FACE);
		glFrontFace(material->mFrontFace);
		glCullFace(material->mCullFace);
	}
	else {
		glDisable(GL_CULL_FACE);
	}
}



//
//void Renderer::render(
//	const std::vector<Mesh*>& meshes,
//	Camera* camera,
//	PointLight* pointLight,
//	AmbientLight* ambLight
//) {
//	//1 设置当前帧绘制的时候，opengl的必要状态机参数
//	glEnable(GL_DEPTH_TEST);
//	glDepthFunc(GL_LESS);
//
//	//2 清理画布
//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//
//	//3 遍历mesh进行绘制
//	for (int i = 0; i < meshes.size(); i++) {
//		auto mesh = meshes[i];
//		auto geometry = mesh->mGeometry;
//		auto material = mesh->mMaterial;
//
//		//1 决定使用哪个Shader
//		Shader* shader = pickShader(material->mType);
//
//		//2 更新shader的uniform
//		shader->begin();
//
//		switch (material->mType) {
//		case MaterialType::PhongMaterial: {
//			PhongMaterial* phongMat = (PhongMaterial*)material;
//
//			//diffuse贴图帧更新
//			//将纹理采样器与纹理单元进行挂钩
//			shader->setInt("sampler", 0);
//			//将纹理与纹理单元进行挂钩
//			phongMat->mDiffuse->bind();
//
//			//高光蒙版的帧更新
//			shader->setInt("specularMaskSampler", 1);
//			phongMat->mSpecularMask->bind();
//
//			//mvp
//			shader->setMatrix4x4("modelMatrix", mesh->getModelMatrix());
//			shader->setMatrix4x4("viewMatrix", camera->getViewMatrix());
//			shader->setMatrix4x4("projectionMatrix", camera->getProjectionMatrix());
//
//			auto normalMatrix = glm::mat3(glm::transpose(glm::inverse(mesh->getModelMatrix())));
//			shader->setMatrix3x3("normalMatrix", normalMatrix);
//
//			//光源参数的uniform更新
//			shader->setVector3("lightPosition", pointLight->getPosition());
//			shader->setVector3("lightColor", pointLight->mColor);
//			shader->setFloat("specularIntensity", pointLight->mSpecularIntensity);
//			shader->setFloat("k2", pointLight->mK2);
//			shader->setFloat("k1", pointLight->mK1);
//			shader->setFloat("kc", pointLight->mKc);
//
//			shader->setFloat("shiness", phongMat->mShiness);
//
//			shader->setVector3("ambientColor", ambLight->mColor);
//
//			//相机信息更新
//			shader->setVector3("cameraPosition", camera->mPosition);
//
//		}
//										break;
//		case MaterialType::WhiteMaterial: {
//			//mvp
//			shader->setMatrix4x4("modelMatrix", mesh->getModelMatrix());
//			shader->setMatrix4x4("viewMatrix", camera->getViewMatrix());
//			shader->setMatrix4x4("projectionMatrix", camera->getProjectionMatrix());
//		}
//										break;
//		default:
//			continue;
//		}
//
//		//3 绑定vao
//		glBindVertexArray(geometry->getVao());
//
//		//4 执行绘制命令
//		glDrawElements(GL_TRIANGLES, geometry->getIndicesCount(), GL_UNSIGNED_INT, 0);
//	}
//}



//void Renderer::render(
//	const std::vector<Mesh*>& meshes,
//	Camera* camera,
//	DirectionalLight* dirLight,
//	AmbientLight* ambLight
//) {
//	//1 设置当前帧绘制的时候，opengl的必要状态机参数
//	glEnable(GL_DEPTH_TEST);
//	glDepthFunc(GL_LESS);
//
//	//2 清理画布
//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//
//	//3 遍历mesh进行绘制
//	for (int i = 0; i < meshes.size(); i++) {
//		auto mesh = meshes[i];
//		auto geometry = mesh->mGeometry;
//		auto material = mesh->mMaterial;
//
//		//1 决定使用哪个Shader
//		Shader* shader = pickShader(material->mType);
//
//		//2 更新shader的uniform
//		shader->begin();
//
//		switch (material->mType) {
//		case MaterialType::PhongMaterial: {
//			PhongMaterial* phongMat = (PhongMaterial*)material;
//
//			//diffuse贴图帧更新
//			//将纹理采样器与纹理单元进行挂钩
//			shader->setInt("sampler", 0);
//			//将纹理与纹理单元进行挂钩
//			phongMat->mDiffuse->bind();
//
//			//高光蒙版的帧更新
//			shader->setInt("specularMaskSampler", 1);
//			phongMat->mSpecularMask->bind();
//
//			//mvp
//			shader->setMatrix4x4("modelMatrix", mesh->getModelMatrix());
//			shader->setMatrix4x4("viewMatrix", camera->getViewMatrix());
//			shader->setMatrix4x4("projectionMatrix", camera->getProjectionMatrix());
//
//			auto normalMatrix = glm::mat3(glm::transpose(glm::inverse(mesh->getModelMatrix())));
//			shader->setMatrix3x3("normalMatrix", normalMatrix);
//
//			//光源参数的uniform更新
//			shader->setVector3("lightDirection", dirLight->mDirection);
//			shader->setVector3("lightColor", dirLight->mColor);
//			shader->setFloat("specularIntensity", dirLight->mSpecularIntensity);
//
//			shader->setFloat("shiness", phongMat->mShiness);
//
//			shader->setVector3("ambientColor", ambLight->mColor);
//
//			//相机信息更新
//			shader->setVector3("cameraPosition", camera->mPosition);
//
//		}
//										break;
//		case MaterialType::WhiteMaterial: {
//			//mvp
//			shader->setMatrix4x4("modelMatrix", mesh->getModelMatrix());
//			shader->setMatrix4x4("viewMatrix", camera->getViewMatrix());
//			shader->setMatrix4x4("projectionMatrix", camera->getProjectionMatrix());
//		}
//										break;
//		default:
//			continue;
//		}
//
//		//3 绑定vao
//		glBindVertexArray(geometry->getVao());
//
//		//4 执行绘制命令
//		glDrawElements(GL_TRIANGLES, geometry->getIndicesCount(), GL_UNSIGNED_INT, 0);
//	}
//}
