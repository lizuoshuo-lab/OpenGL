#pragma once

#include <string>

class FrameCapture {
public:
	static bool saveBackBufferPng(
		const std::string& path,
		int width,
		int height
	);
};
