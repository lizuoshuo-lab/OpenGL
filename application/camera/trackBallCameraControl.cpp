#include "trackBallCameraControl.h"


TrackBallCameraControl::TrackBallCameraControl() {

}

TrackBallCameraControl::~TrackBallCameraControl() {

}

void TrackBallCameraControl::onCursor(double xpos, double ypos) {
	if (mLeftMouseDown) {
		const float deltaX = static_cast<float>(xpos - mCurrentX) * mSensitivity;
		const float deltaY = static_cast<float>(ypos - mCurrentY) * mSensitivity;

		pitch(-deltaY);
		yaw(-deltaX);
	}
	else if (mMiddleMouseDown) {
		const float deltaX = static_cast<float>(xpos - mCurrentX) * mMoveSpeed;
		const float deltaY = static_cast<float>(ypos - mCurrentY) * mMoveSpeed;

		mCamera->mPosition += mCamera->mUp * deltaY;
		mCamera->mPosition -= mCamera->mRight * deltaX;
	}


	mCurrentX = static_cast<float>(xpos);
	mCurrentY = static_cast<float>(ypos);
}

void TrackBallCameraControl::pitch(float angle) {
	// 绕相机右轴旋转。
	auto mat = glm::rotate(glm::mat4(1.0f), glm::radians(angle), mCamera->mRight);

	mCamera->mUp = mat * glm::vec4(mCamera->mUp, 0.0f);
	mCamera->mPosition = mat * glm::vec4(mCamera->mPosition, 1.0f);
}

void TrackBallCameraControl::yaw(float angle) {
	// 绕世界坐标系 Y 轴旋转。
	auto mat = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f,1.0f,0.0f));
	mCamera->mUp = mat * glm::vec4(mCamera->mUp, 0.0f);
	mCamera->mRight = mat * glm::vec4(mCamera->mRight, 0.0f);
	mCamera->mPosition = mat * glm::vec4(mCamera->mPosition, 1.0f);
}

void TrackBallCameraControl::onScroll(float offset) {
	mCamera->scale(mScaleSpeed * offset);
}
