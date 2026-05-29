#pragma once

#include "../core.h"
#include "../texture.h"

class Framebuffer {
public:
	static Framebuffer* createShadowFbo(unsigned int width, unsigned int height);
	static Framebuffer* createCSMShadowFbo(unsigned int width, unsigned int height, unsigned int layerNumber);
	static Framebuffer* createMultiSampleFbo(unsigned int width, unsigned int height, unsigned int samples);
	static Framebuffer* createHDRFbo(unsigned int width, unsigned int height);
	static Framebuffer* createMultiSampleHDRFbo(unsigned int width, unsigned int height, unsigned int samples = 4);


	Framebuffer();
	Framebuffer(unsigned int width, unsigned int height);
	~Framebuffer();

public:
	unsigned int mWidth{ 0 };
	unsigned int mHeight{ 0 };

	unsigned int mFBO{ 0 };
	Texture* mColorAttachment{ nullptr };
	Texture* mDepthStencilAttachment{ nullptr };
	Texture* mDepthAttachment{ nullptr };
};