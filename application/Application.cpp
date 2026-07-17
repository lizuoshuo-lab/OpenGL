#include "Application.h"
#include<glad/glad.h>
#include<GLFW/glfw3.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif


Application* Application::mInstance = nullptr;
Application* Application::getInstance() {
	if (mInstance == nullptr) {
		mInstance = new Application();
	}

	return mInstance;
}

Application::Application() {

}

Application::~Application() {

}


bool Application::init(const int& width, const int& height) {
	mWidth = width;
	mHeight = height;

#ifdef _WIN32
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

	if (glfwInit() != GLFW_TRUE) {
		const char* description = nullptr;
		const int errorCode = glfwGetError(&description);
		std::cerr << "GLFW initialization failed (" << errorCode << "): "
			<< (description != nullptr ? description : "unknown error") << std::endl;
		return false;
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);

	mWindow = glfwCreateWindow(mWidth, mHeight, "OpenGL Renderer", NULL, NULL);
	if (mWindow == NULL) {
		const char* description = nullptr;
		const int errorCode = glfwGetError(&description);
		std::cerr << "GLFW window creation failed (" << errorCode << "): "
			<< (description != nullptr ? description : "unknown error") << std::endl;
		glfwTerminate();
		return false;
	}
	glfwGetWindowPos(mWindow, &mWindowedX, &mWindowedY);
	glfwGetWindowSize(mWindow, &mWindowedWidth, &mWindowedHeight);

	glfwMakeContextCurrent(mWindow);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Failed to initialize GLAD" << std::endl;
		return false;
	}
	setVsync(false);
	
	glfwSetFramebufferSizeCallback(mWindow, frameBufferSizeCallback);

	glfwSetWindowUserPointer(mWindow, this);

	glfwSetKeyCallback(mWindow, keyCallback);

	glfwSetMouseButtonCallback(mWindow, mouseCallback);

	glfwSetCursorPosCallback(mWindow, cursorCallback);

	glfwSetScrollCallback(mWindow, scrollCallback);

	return true;
}

bool Application::update() {
	if (glfwWindowShouldClose(mWindow)) {
		return false;
	}

	glfwPollEvents();

	int width = 0;
	int height = 0;
	getFramebufferSize(&width, &height);
	if (width > 0 && height > 0 &&
		(width != static_cast<int>(mWidth) || height != static_cast<int>(mHeight))) {
		mWidth = width;
		mHeight = height;
		if (mResizeCallback != nullptr) {
			mResizeCallback(width, height);
		}
	}

	glfwSwapBuffers(mWindow);

	return true;
}

void Application::destroy() {
	glfwTerminate();
}

void Application::getCursorPosition(double* x, double* y) {
	glfwGetCursorPos(mWindow, x, y);
}

void Application::getFramebufferSize(int* width, int* height) const {
	if (width == nullptr || height == nullptr) {
		return;
	}

	*width = 0;
	*height = 0;
	if (mWindow == nullptr) {
		return;
	}

	if (mFullscreen) {
		GLFWmonitor* monitor = glfwGetWindowMonitor(mWindow);
		if (monitor == nullptr) {
			monitor = glfwGetPrimaryMonitor();
		}
		const GLFWvidmode* mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
		if (mode != nullptr) {
			*width = mode->width;
			*height = mode->height;
			return;
		}
	}

#ifdef _WIN32
	RECT clientRect{};
	HWND nativeWindow = glfwGetWin32Window(mWindow);
	DPI_AWARENESS_CONTEXT previousContext =
		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	const bool hasClientRect =
		nativeWindow != nullptr && GetClientRect(nativeWindow, &clientRect) != FALSE;
	if (previousContext != nullptr) {
		SetThreadDpiAwarenessContext(previousContext);
	}
	if (hasClientRect) {
		*width = clientRect.right - clientRect.left;
		*height = clientRect.bottom - clientRect.top;
		return;
	}
#endif

	glfwGetFramebufferSize(mWindow, width, height);
}

void Application::setFullscreen(bool fullscreen) {
	if (mWindow == nullptr || fullscreen == mFullscreen) {
		return;
	}

	if (fullscreen) {
		glfwGetWindowPos(mWindow, &mWindowedX, &mWindowedY);
		glfwGetWindowSize(mWindow, &mWindowedWidth, &mWindowedHeight);

		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
		if (monitor == nullptr || mode == nullptr) {
			return;
		}
		int monitorX = 0;
		int monitorY = 0;
		glfwGetMonitorPos(monitor, &monitorX, &monitorY);

		mFullscreen = true;
		glfwSetWindowAttrib(mWindow, GLFW_DECORATED, GLFW_FALSE);
		glfwSetWindowMonitor(
			mWindow,
			nullptr,
			monitorX,
			monitorY,
			mode->width,
			mode->height,
			GLFW_DONT_CARE);
		glfwFocusWindow(mWindow);
	} else {
		mFullscreen = false;
		glfwSetWindowAttrib(mWindow, GLFW_DECORATED, GLFW_TRUE);
		glfwSetWindowMonitor(
			mWindow,
			nullptr,
			mWindowedX,
			mWindowedY,
			mWindowedWidth,
			mWindowedHeight,
			GLFW_DONT_CARE);
	}

	int width = 0;
	int height = 0;
	getFramebufferSize(&width, &height);
	if (width > 0 && height > 0 &&
		(width != static_cast<int>(mWidth) || height != static_cast<int>(mHeight))) {
		mWidth = width;
		mHeight = height;
		if (mResizeCallback != nullptr) {
			mResizeCallback(width, height);
		}
	}
}

void Application::toggleFullscreen() {
	setFullscreen(!mFullscreen);
}

void Application::setVsync(bool enabled) {
	mVsync = enabled;
	if (mWindow != nullptr) {
		glfwSwapInterval(enabled ? 1 : 0);
	}
}


void Application::frameBufferSizeCallback(GLFWwindow* window, int width, int height) {
	Application* self = (Application*)glfwGetWindowUserPointer(window);
	self->getFramebufferSize(&width, &height);
	self->mWidth = width;
	self->mHeight = height;
	if (self->mResizeCallback != nullptr) {
		self->mResizeCallback(width, height);
	}
}

void Application::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	Application* self = (Application*)glfwGetWindowUserPointer(window);
	if (self->mKeyBoardCallback != nullptr) {
		self->mKeyBoardCallback(key, action, mods);
	}
}

void Application::mouseCallback(GLFWwindow* window, int button, int action, int mods) {
	Application* self = (Application*)glfwGetWindowUserPointer(window);
	if (self->mMouseCallback != nullptr) {
		self->mMouseCallback(button, action, mods);
	}
}

void Application::cursorCallback(GLFWwindow* window, double xpos, double ypos) {
	Application* self = (Application*)glfwGetWindowUserPointer(window);
	if (self->mCursorCallback != nullptr) {
		self->mCursorCallback(xpos, ypos);
	}
}

void Application::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	Application* self = (Application*)glfwGetWindowUserPointer(window);
	if (self->mScrollCallback != nullptr) {
		self->mScrollCallback(yoffset);
	}
}
