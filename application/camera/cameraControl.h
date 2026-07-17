#pragma once

#include "../../glframework/core.h"
#include "camera.h"
#include <map>

class CameraControl {
public:
	CameraControl();
	~CameraControl();

	// 子类按相机类型实现各自的输入行为。
	virtual void onMouse(int button, int action, double xpos, double ypos);
	virtual void onCursor(double xpos, double ypos);
	virtual void onKey(int key, int action, int mods);
	virtual void onScroll(float offset);

	// 在每帧渲染前更新相机。
	virtual void update();

	void setCamera(Camera* camera) { mCamera = camera; }
	void setSensitivity(float s) { mSensitivity = s; }
	void setScaleSpeed(float s) { mScaleSpeed = s; }

protected:
	// 鼠标按键状态
	bool mLeftMouseDown = false;
	bool mRightMouseDown = false;
	bool mMiddleMouseDown = false;

	// 当前鼠标位置
	float mCurrentX = 0.0f, mCurrentY = 0.0f;

	// 鼠标旋转灵敏度
	float mSensitivity = 0.2f;

	// 键盘按键状态
	std::map<int, bool> mKeyMap;

	// 当前受控相机
	Camera* mCamera = nullptr;

	// 相机缩放速度
	float mScaleSpeed = 0.2f;
};
