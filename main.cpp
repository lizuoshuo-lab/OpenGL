#include <iostream>

#include "glframework/core.h"
#include "glframework/shader.h"
#include <string>
#include <assert.h>//断言
#include "wrapper/checkError.h"
#include "application/Application.h"
#include "glframework/texture.h"

//引入相机+控制器
#include "application/camera/perspectiveCamera.h"
#include "application/camera/orthographicCamera.h"
#include "application/camera/trackBallCameraControl.h"
#include "application/camera/GameCameraControl.h"

#include "glframework/geometry.h"
#include "glframework/material/phongMaterial.h"
#include "glframework/material/whiteMaterial.h"
#include "glframework/material/depthMaterial.h"
#include "glframework/material/opacityMaskMaterial.h"
#include "glframework/material/screenMaterial.h"
#include "glframework/material/cubeMaterial.h"
#include "glframework/material/phongEnvMaterial.h"
#include "glframework/material/phongInstanceMaterial.h"
#include "glframework/material/grassInstanceMaterial.h"
#include "glframework/material/advanced/phongNormalMaterial.h"
#include "glframework/material/advanced/phongParallaxMaterial.h"
#include "glframework/material/advanced/phongShadowMaterial.h"
#include "glframework/material/advanced/phongCSMShadowMaterial.h"

#include "glframework/material/advanced/pbrMaterial.h"
#include "glframework/mesh/mesh.h"
#include "glframework/mesh/instancedMesh.h"
#include "glframework/renderer/renderer.h"
#include "glframework/light/pointLight.h"
#include "glframework/light/spotLight.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "glframework/scene.h"
#include "application/assimpLoader.h"

#include "glframework/framebuffer/framebuffer.h"

#include "application/assimpInstanceLoader.h"


Renderer* renderer = nullptr;
Scene* sceneOff = nullptr;
Scene* scene = nullptr;

Framebuffer* fboMulti = nullptr;
Framebuffer* fboResolve = nullptr;

ScreenMaterial* smat = nullptr;
PbrMaterial* pbrMaterial = nullptr;


Mesh* upPlane = nullptr;

int WIDTH = 2560;
int HEIGHT = 1440;

//灯光们

std::vector<PointLight*> pointLights{};

const int MAX_DEMO_LIGHTS = 4;
bool directLightingEnabled = false;
bool lightEnabled[MAX_DEMO_LIGHTS]{ true, true, false, false };
glm::vec3 lightUiTint[MAX_DEMO_LIGHTS]{
	glm::vec3(1.0f, 1.0f, 1.0f),
	glm::vec3(1.0f, 1.0f, 1.0f),
	glm::vec3(1.0f, 1.0f, 1.0f),
	glm::vec3(1.0f, 1.0f, 1.0f),
};
float lightUiPower[MAX_DEMO_LIGHTS]{ 80.0f, 80.0f, 80.0f, 80.0f };

float cameraMoveSpeed = 0.1f;
float cameraMouseSensitivity = 0.1f;


Camera* camera = nullptr;
GameCameraControl* cameraControl = nullptr;

glm::vec3 clearColor{ 0.1f, 0.13f, 0.25f };

void syncPointLightColors() {
	int lightCount = static_cast<int>(pointLights.size());
	if (lightCount > MAX_DEMO_LIGHTS) {
		lightCount = MAX_DEMO_LIGHTS;
	}

	for (int i = 0; i < lightCount; i++) {
		if (directLightingEnabled && lightEnabled[i]) {
			pointLights[i]->mColor = lightUiTint[i] * lightUiPower[i];
		}
		else {
			pointLights[i]->mColor = glm::vec3(0.0f);
		}
	}
}

void OnResize(int width, int height) {
	GL_CALL(glViewport(0, 0, width, height));
}

void OnKey(int key, int action, int mods) {
	if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard) {
		return;
	}
	cameraControl->onKey(key, action, mods);
}

//鼠标按下/抬起
void OnMouse(int button, int action, int mods) {
	if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) {
		return;
	}
	double x, y;
	glApp->getCursorPosition(&x, &y);
	cameraControl->onMouse(button, action, x, y);
}

//鼠标移动
void OnCursor(double xpos, double ypos) {
	if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) {
		return;
	}
	cameraControl->onCursor(xpos, ypos);
}

//鼠标滚轮
void OnScroll(double offset) {
	if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) {
		return;
	}
	cameraControl->onScroll(static_cast<float>(offset));
}


void prepare() {
	fboMulti = Framebuffer::createMultiSampleHDRFbo(WIDTH, HEIGHT);
	fboResolve = Framebuffer::createHDRFbo(WIDTH, HEIGHT);

	renderer = new Renderer();
	sceneOff = new Scene();
	scene = new Scene();

	//pass 01

	auto geometry = Geometry::createSphere(2.0f);
	pbrMaterial = new PbrMaterial();
	pbrMaterial->mAlbedo = new Texture("assets/textures/metal/metal1.jpg", 0, GL_SRGB_ALPHA);

	pbrMaterial->mNormal = Texture::createNearestTexture("assets/textures/metal/metal2.jpg");
	pbrMaterial->mRoughness = Texture::createNearestTexture("assets/textures/metal/metal3.jpg");
	pbrMaterial->mMetallic = Texture::createNearestTexture("assets/textures/metal/metal4.jpg");
	pbrMaterial->mAo = Texture::createNearestTexture("assets/textures/metal/metal5.jpg");
	auto hdrEnvironment = Texture::createExrTexture("assets/textures/brown_photostudio_01_4k.exr");
	auto environmentCube = Texture::createEnvironmentCubeMap(hdrEnvironment, 512);
	pbrMaterial->mIrradianceMap = Texture::createIrradianceCubeMap(environmentCube, 32);
	pbrMaterial->mPrefilterMap = Texture::createPrefilterCubeMap(environmentCube, 128, 5);
	pbrMaterial->mBrdfLut = Texture::createBrdfLut(512);

	auto boxGeo = Geometry::createBox(1.0f);
	auto boxMat = new CubeMaterial();
	boxMat->mDiffuse = hdrEnvironment;
	auto boxMesh = new Mesh(boxGeo, boxMat);
	sceneOff->addChild(boxMesh);


	for (int i = 1; i < 4; i++) {
		for (int j = 1; j < 4; j++) {
			auto mesh = new Mesh(geometry, pbrMaterial);
			mesh->setPosition(glm::vec3(i * 5.0f, j * 5.0f, 0.0f));

			sceneOff->addChild(mesh);
		}
	}

	


	//pass 02 postProcessPass:后处理pass
	auto sgeo = Geometry::createScreenPlane();
	smat = new ScreenMaterial();
	smat->mScreenTexture = fboResolve->mColorAttachment;
	auto smesh = new Mesh(sgeo, smat);
	scene->addChild(smesh);

	
	glm::vec3 lightPositions[] = {
			glm::vec3(7.5f, 7.5f, -10.0f),
			glm::vec3(7.5f,  7.5f, 10.0f),
	
	};
	glm::vec3 lightColors[] = {
		glm::vec3(0.0f, 0.0f, 0.0f),
	    glm::vec3(0.0f, 0.0f, 0.0f),		
	
	};
	for (int i = 0; i < 2; i++) {
		auto pointLight = new PointLight();
		pointLight->setPosition(lightPositions[i]);
		pointLight->mColor = lightColors[i];
		pointLights.push_back(pointLight);
	}

	syncPointLightColors();
}


void prepareCamera() {
	float size = 10.0f;
	//camera = new OrthographicCamera(-size, size, size, -size, size, -size);
	camera = new PerspectiveCamera(
		60.0f, 
		(float)glApp->getWidth() / (float)glApp->getHeight(),
		0.1f,
		150.0f
	);

	cameraControl = new GameCameraControl();
	cameraControl->setCamera(camera);
	cameraControl->setSensitivity(cameraMouseSensitivity);
	cameraControl->setSpeed(cameraMoveSpeed);
}



void initIMGUI() {
	ImGui::CreateContext();//创建imgui上下文
	ImGui::StyleColorsDark(); // 选择一个主题

	// 设置ImGui与GLFW和OpenGL的绑定
	ImGui_ImplGlfw_InitForOpenGL(glApp->getWindow(), true);
	ImGui_ImplOpenGL3_Init("#version 460");
}

void renderIMGUI() {
	//1 开启当前的IMGUI渲染
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowSize(ImVec2(390.0f, 560.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("IBL Showcase");

	ImGuiIO& io = ImGui::GetIO();
	float frameMs = io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f;
	ImGui::Text("FPS %.1f  Frame %.2f ms", io.Framerate, frameMs);
	ImGui::Separator();

	if (smat != nullptr && ImGui::CollapsingHeader("Output", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Exposure", &smat->mExposure, 0.0f, 3.0f, "%.2f");
		ImGui::ColorEdit3("Clear Color", &clearColor.x);
	}

	if (pbrMaterial != nullptr && ImGui::CollapsingHeader("IBL", ImGuiTreeNodeFlags_DefaultOpen)) {
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
		ImGui::Combo("View", &pbrMaterial->mDebugView, debugViews, IM_ARRAYSIZE(debugViews));
		ImGui::SliderFloat("Environment", &pbrMaterial->mEnvIntensity, 0.0f, 5.0f, "%.2f");
		ImGui::SliderFloat("Reflection LOD", &pbrMaterial->mMaxReflectionLod, 0.0f, 4.0f, "%.2f");
		if (ImGui::Button("Reset IBL")) {
			pbrMaterial->mDebugView = 0;
			pbrMaterial->mEnvIntensity = 1.0f;
			pbrMaterial->mMaxReflectionLod = 4.0f;
		}
	}

	if (pbrMaterial != nullptr && ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Metallic", &pbrMaterial->mMetallicScale, 0.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Roughness", &pbrMaterial->mRoughnessScale, 0.1f, 2.0f, "%.2f");
		ImGui::SliderFloat("AO", &pbrMaterial->mAoScale, 0.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Normal", &pbrMaterial->mNormalStrength, 0.0f, 2.0f, "%.2f");
		if (ImGui::Button("Reset Material")) {
			pbrMaterial->mMetallicScale = 1.0f;
			pbrMaterial->mRoughnessScale = 1.0f;
			pbrMaterial->mAoScale = 1.0f;
			pbrMaterial->mNormalStrength = 1.0f;
		}
	}

	if (ImGui::CollapsingHeader("Direct Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Direct Lights", &directLightingEnabled);
		int lightCount = static_cast<int>(pointLights.size());
		if (lightCount > MAX_DEMO_LIGHTS) {
			lightCount = MAX_DEMO_LIGHTS;
		}

		for (int i = 0; i < lightCount; i++) {
			ImGui::PushID(i);
			ImGui::Separator();
			ImGui::Checkbox("Enabled", &lightEnabled[i]);
			glm::vec3 lightPosition = pointLights[i]->getPosition();
			if (ImGui::DragFloat3("Position", &lightPosition.x, 0.1f)) {
				pointLights[i]->setPosition(lightPosition);
			}
			ImGui::ColorEdit3("Tint", &lightUiTint[i].x);
			ImGui::SliderFloat("Power", &lightUiPower[i], 0.0f, 200.0f, "%.1f");
			ImGui::PopID();
		}
	}

	syncPointLightColors();

	if (camera != nullptr && cameraControl != nullptr && ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Position", &camera->mPosition.x, 0.1f);
		if (ImGui::SliderFloat("Move Speed", &cameraMoveSpeed, 0.01f, 1.0f, "%.2f")) {
			cameraControl->setSpeed(cameraMoveSpeed);
		}
		if (ImGui::SliderFloat("Mouse Sensitivity", &cameraMouseSensitivity, 0.01f, 1.0f, "%.2f")) {
			cameraControl->setSensitivity(cameraMouseSensitivity);
		}
		if (ImGui::Button("Reset Camera")) {
			camera->mPosition = glm::vec3(0.0f, 0.0f, 5.0f);
		}
	}

	ImGui::End();

	//3 执行UI渲染
	ImGui::Render();
	//获取当前窗体的宽高
	int display_w, display_h;
	glfwGetFramebufferSize(glApp->getWindow(), &display_w, &display_h);
	//重置视口大小
	glViewport(0, 0, display_w, display_h);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

int main() {
	if (!glApp->init(WIDTH, HEIGHT)) {
		return -1;
	}

	glApp->setResizeCallback(OnResize);
	glApp->setKeyBoardCallback(OnKey);
	glApp->setMouseCallback(OnMouse);
	glApp->setCursorCallback(OnCursor);
	glApp->setScrollCallback(OnScroll);

	//设置opengl视口以及清理颜色
	GL_CALL(glViewport(0, 0, WIDTH, HEIGHT));
	GL_CALL(glClearColor(0.1f, 0.13f, 0.25f, 1.0f));

	prepareCamera();

	prepare();
	
	initIMGUI();

	while (glApp->update()) {
		cameraControl->update();
		syncPointLightColors();

		renderer->setClearColor(clearColor);
		
		renderer->render(sceneOff, camera, pointLights, fboMulti->mFBO);
		renderer->msaaResolve(fboMulti, fboResolve);
		renderer->render(scene, camera, pointLights);
		
		renderIMGUI();
	}

	glApp->destroy();

	return 0;
}
