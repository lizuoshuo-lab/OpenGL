#include "gameCameraControl.h"

GameCameraControl::GameCameraControl() {

}

GameCameraControl::~GameCameraControl() {

}

void GameCameraControl::onCursor(double xpos, double ypos) {
	const float deltaX = static_cast<float>(xpos - mCurrentX) * mSensitivity;
	const float deltaY = static_cast<float>(ypos - mCurrentY) * mSensitivity;
	
	if (mRightMouseDown) {
		pitch(-deltaY);
		yaw(-deltaX);
	}

	mCurrentX = static_cast<float>(xpos);
	mCurrentY = static_cast<float>(ypos);
}

void GameCameraControl::pitch(float angle) {
	mPitch += angle;
	if (mPitch > 89.0f || mPitch < -89.0f) {
		mPitch -= angle;
		return;
	}

	// 第一人称俯仰只调整朝向，不移动相机位置。
	auto mat = glm::rotate(glm::mat4(1.0f), glm::radians(angle), mCamera->mRight);
	mCamera->mUp = mat * glm::vec4(mCamera->mUp, 0.0f);
}

void GameCameraControl::yaw(float angle) {
	auto mat = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f,1.0f,0.0f));
	mCamera->mUp = mat * glm::vec4(mCamera->mUp, 0.0f);
	mCamera->mRight = mat * glm::vec4(mCamera->mRight, 0.0f);
}


void GameCameraControl::update() {
	// 合并当前按键对应的移动方向。
	glm::vec3 direction(0.0f);

	auto front = glm::cross(mCamera->mUp, mCamera->mRight);
	auto right = mCamera->mRight;
	auto up = mCamera->mUp;

	if (mKeyMap[GLFW_KEY_W]) {
		direction += front;
	}

	if (mKeyMap[GLFW_KEY_S]) {
		direction -= front;
	}

	if (mKeyMap[GLFW_KEY_A]) {
		direction -= right;
	}

	if (mKeyMap[GLFW_KEY_D]) {
		direction += right;
	}
	if (mKeyMap[GLFW_KEY_Q]) {
		direction += up;
	}
	if (mKeyMap[GLFW_KEY_E]) {
		direction -= up;
	}


	if (glm::length(direction) > 0.0f) {
		direction = glm::normalize(direction);
		mCamera->mPosition += direction * mSpeed;
	}
}
