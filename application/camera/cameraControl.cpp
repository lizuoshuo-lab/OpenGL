#include "cameraControl.h"
#include <iostream>

CameraControl::CameraControl() {

}

CameraControl::~CameraControl() {

}

void CameraControl::onMouse(int button, int action, double xpos, double ypos) {

	const bool pressed = action == GLFW_PRESS;

	// 记录拖拽起点。
	if (pressed) {
		mCurrentX = static_cast<float>(xpos);
		mCurrentY = static_cast<float>(ypos);
	}

	switch (button) {
	case GLFW_MOUSE_BUTTON_LEFT:
		mLeftMouseDown = pressed;
		break;
	case GLFW_MOUSE_BUTTON_RIGHT:
		mRightMouseDown = pressed;
		break;
	case GLFW_MOUSE_BUTTON_MIDDLE:
		mMiddleMouseDown = pressed;
		break;
	}
}

void CameraControl::onCursor(double xpos, double ypos) {
}

void CameraControl::onKey(int key, int action, int mods) {

	// 按键状态只在按下和抬起时更新。
	if (action == GLFW_REPEAT) {
		return;
	}

	const bool pressed = action == GLFW_PRESS;
	mKeyMap[key] = pressed;
}

void CameraControl::update() {
}

void CameraControl::onScroll(float offset) {

}
