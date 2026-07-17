#pragma once

#include <cstdint>
#include <iostream>

#define glApp Application::getInstance()

struct GLFWwindow;

using ResizeCallback = void(*)(int width, int height);
using KeyBoardCallback = void(*)(int key, int action, int mods);
using MouseCallback = void(*)(int button, int action, int mods);
using CursorCallback = void(*)(double xpos, double ypos);
using ScrollCallback = void(*)(double offset);

class Application {
public:
	~Application();

	static Application* getInstance();

	bool init(const int& width = 800, const int& height = 600);
	bool update();
	void destroy();

	GLFWwindow* getWindow() const { return mWindow; }
	uint32_t getWidth() const { return mWidth; }
	uint32_t getHeight() const { return mHeight; }
	void getFramebufferSize(int* width, int* height) const;
	void getCursorPosition(double* x, double* y);
	void setFullscreen(bool fullscreen);
	void toggleFullscreen();
	bool isFullscreen() const { return mFullscreen; }
	void setVsync(bool enabled);
	bool isVsync() const { return mVsync; }

	void setResizeCallback(ResizeCallback callback) { mResizeCallback = callback; }
	void setKeyBoardCallback(KeyBoardCallback callback) { mKeyBoardCallback = callback; }
	void setMouseCallback(MouseCallback callback) { mMouseCallback = callback; }
	void setCursorCallback(CursorCallback callback) { mCursorCallback = callback; }
	void setScrollCallback(ScrollCallback callback) { mScrollCallback = callback; }

private:
	static void frameBufferSizeCallback(GLFWwindow* window, int width, int height);
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void mouseCallback(GLFWwindow* window, int button, int action, int mods);
	static void cursorCallback(GLFWwindow* window, double xpos, double ypos);
	static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

	static Application* mInstance;

	uint32_t mWidth{ 0 };
	uint32_t mHeight{ 0 };
	GLFWwindow* mWindow{ nullptr };
	bool mFullscreen{ false };
	bool mVsync{ false };
	int mWindowedX{ 100 };
	int mWindowedY{ 100 };
	int mWindowedWidth{ 1600 };
	int mWindowedHeight{ 900 };

	ResizeCallback mResizeCallback{ nullptr };
	KeyBoardCallback mKeyBoardCallback{ nullptr };
	MouseCallback mMouseCallback{ nullptr };
	CursorCallback mCursorCallback{ nullptr };
	ScrollCallback mScrollCallback{ nullptr };

	Application();
};
