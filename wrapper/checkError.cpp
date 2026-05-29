#include "checkError.h"
#include <glad/glad.h>
#include <cassert>
#include <iostream>
#include <string>

void checkError(const char* file, int line) {
	GLenum errorCode = glGetError();
	if (errorCode == GL_NO_ERROR) {
		return;
	}

	std::string error;
	switch (errorCode) {
	case GL_INVALID_ENUM: error = "INVALID_ENUM"; break;
	case GL_INVALID_VALUE: error = "INVALID_VALUE"; break;
	case GL_INVALID_OPERATION: error = "INVALID_OPERATION"; break;
	case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
	case GL_OUT_OF_MEMORY: error = "OUT_OF_MEMORY"; break;
	default: error = "UNKNOWN"; break;
	}

	std::cout << error << " (0x" << std::hex << errorCode << std::dec << ")";
	if (file != nullptr) {
		std::cout << " at " << file << ":" << line;
	}
	std::cout << std::endl;

	assert(false);
}
