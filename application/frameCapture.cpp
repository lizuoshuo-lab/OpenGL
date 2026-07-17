#include "frameCapture.h"

#include "../glframework/core.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool FrameCapture::saveBackBufferPng(
	const std::string& path,
	int width,
	int height
) {
	if (path.empty() || width <= 0 || height <= 0) {
		return false;
	}

	const std::filesystem::path outputPath(path);
	if (outputPath.has_parent_path()) {
		std::error_code error;
		std::filesystem::create_directories(outputPath.parent_path(), error);
		if (error) {
			return false;
		}
	}

	std::vector<unsigned char> pixels(
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4
	);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glFinish();
	glReadPixels(
		0,
		0,
		width,
		height,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		pixels.data()
	);

	const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
	std::vector<unsigned char> row(rowBytes);
	for (int top = 0, bottom = height - 1; top < bottom; ++top, --bottom) {
		unsigned char* topRow = pixels.data() + static_cast<std::size_t>(top) * rowBytes;
		unsigned char* bottomRow = pixels.data() + static_cast<std::size_t>(bottom) * rowBytes;
		std::copy(topRow, topRow + rowBytes, row.data());
		std::copy(bottomRow, bottomRow + rowBytes, topRow);
		std::copy(row.data(), row.data() + rowBytes, bottomRow);
	}

	return stbi_write_png(
		outputPath.string().c_str(),
		width,
		height,
		4,
		pixels.data(),
		static_cast<int>(rowBytes)
	) != 0;
}
