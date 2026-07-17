#include "texture.h"
#include "../wrapper/checkError.h"
#include "shader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#define STB_IMAGE_IMPLEMENTATION
#include "../application/stb_image.h"

#define TINYEXR_IMPLEMENTATION
#include "../application/tinyexr.h"

std::map<std::string, Texture*> Texture::mTextureCache{};

namespace {
	constexpr float kPi = 3.14159265358979323846f;

	glm::vec2 hammersley(uint32_t i, uint32_t n) {
		uint32_t bits = i;
		bits = (bits << 16u) | (bits >> 16u);
		bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
		bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
		bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
		bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
		float radicalInverse = float(bits) * 2.3283064365386963e-10f;
		return glm::vec2(float(i) / float(n), radicalInverse);
	}

	glm::vec3 importanceSampleGGX(const glm::vec2& xi, const glm::vec3& n, float roughness) {
		float a = roughness * roughness;
		float phi = 2.0f * kPi * xi.x;
		float cosTheta = std::sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
		float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

		glm::vec3 h;
		h.x = std::cos(phi) * sinTheta;
		h.y = std::sin(phi) * sinTheta;
		h.z = cosTheta;

		glm::vec3 up = std::abs(n.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 tangent = glm::normalize(glm::cross(up, n));
		glm::vec3 bitangent = glm::cross(n, tangent);
		return glm::normalize(tangent * h.x + bitangent * h.y + n * h.z);
	}

	float geometrySchlickGGX(float nDotV, float roughness) {
		float a = roughness;
		float k = (a * a) / 2.0f;
		float denom = nDotV * (1.0f - k) + k;
		return nDotV / std::max(denom, 0.000001f);
	}

	float geometrySmith(float nDotV, float nDotL, float roughness) {
		return geometrySchlickGGX(nDotV, roughness) * geometrySchlickGGX(nDotL, roughness);
	}

	glm::vec2 integrateBRDF(float nDotV, float roughness) {
		glm::vec3 v;
		v.x = std::sqrt(std::max(0.0f, 1.0f - nDotV * nDotV));
		v.y = 0.0f;
		v.z = nDotV;

		float a = 0.0f;
		float b = 0.0f;
		glm::vec3 n = glm::vec3(0.0f, 0.0f, 1.0f);
		const uint32_t sampleCount = 1024u;

		for (uint32_t i = 0; i < sampleCount; i++) {
			glm::vec2 xi = hammersley(i, sampleCount);
			glm::vec3 h = importanceSampleGGX(xi, n, roughness);
			glm::vec3 l = glm::normalize(2.0f * glm::dot(v, h) * h - v);

			float nDotL = std::max(l.z, 0.0f);
			float nDotH = std::max(h.z, 0.0f);
			float vDotH = std::max(glm::dot(v, h), 0.0f);

			if (nDotL > 0.0f) {
				float g = geometrySmith(nDotV, nDotL, roughness);
				float gVis = (g * vDotH) / std::max(nDotH * nDotV, 0.000001f);
				float fc = std::pow(1.0f - vDotH, 5.0f);
				a += (1.0f - fc) * gVis;
				b += fc * gVis;
			}
		}

		return glm::vec2(a, b) / float(sampleCount);
	}

	std::vector<glm::mat4> captureViews() {
		return {
			glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
		};
	}

	void renderCaptureCube() {
		static GLuint cubeVao = 0;
		static GLuint cubeVbo = 0;

		if (cubeVao == 0) {
			float vertices[] = {
				-1.0f,-1.0f,-1.0f,  1.0f, 1.0f,-1.0f,  1.0f,-1.0f,-1.0f,
				1.0f, 1.0f,-1.0f, -1.0f,-1.0f,-1.0f, -1.0f, 1.0f,-1.0f,
				-1.0f,-1.0f, 1.0f,  1.0f,-1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
				1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f,-1.0f, 1.0f,
				-1.0f, 1.0f, 1.0f, -1.0f, 1.0f,-1.0f, -1.0f,-1.0f,-1.0f,
				-1.0f,-1.0f,-1.0f, -1.0f,-1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
				1.0f, 1.0f, 1.0f,  1.0f,-1.0f,-1.0f,  1.0f, 1.0f,-1.0f,
				1.0f,-1.0f,-1.0f,  1.0f, 1.0f, 1.0f,  1.0f,-1.0f, 1.0f,
				-1.0f,-1.0f,-1.0f,  1.0f,-1.0f,-1.0f,  1.0f,-1.0f, 1.0f,
				1.0f,-1.0f, 1.0f, -1.0f,-1.0f, 1.0f, -1.0f,-1.0f,-1.0f,
				-1.0f, 1.0f,-1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,-1.0f,
				1.0f, 1.0f, 1.0f, -1.0f, 1.0f,-1.0f, -1.0f, 1.0f, 1.0f
			};

			glGenVertexArrays(1, &cubeVao);
			glGenBuffers(1, &cubeVbo);
			glBindVertexArray(cubeVao);
			glBindBuffer(GL_ARRAY_BUFFER, cubeVbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		}

		glBindVertexArray(cubeVao);
		GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 36));
		glBindVertexArray(0);
	}

	void renderCaptureQuad() {
		static GLuint quadVao = 0;
		static GLuint quadVbo = 0;

		if (quadVao == 0) {
			float vertices[] = {
				-1.0f, -1.0f, 0.0f, 0.0f,
				 1.0f, -1.0f, 1.0f, 0.0f,
				-1.0f,  1.0f, 0.0f, 1.0f,
				-1.0f,  1.0f, 0.0f, 1.0f,
				 1.0f, -1.0f, 1.0f, 0.0f,
				 1.0f,  1.0f, 1.0f, 1.0f
			};

			glGenVertexArrays(1, &quadVao);
			glGenBuffers(1, &quadVbo);
			glBindVertexArray(quadVao);
			glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		}

		glBindVertexArray(quadVao);
		GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
		glBindVertexArray(0);
	}

	void saveFramebufferState(GLint& fbo, GLint viewport[4]) {
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
		glGetIntegerv(GL_VIEWPORT, viewport);
	}

	void restoreFramebufferState(GLint fbo, const GLint viewport[4]) {
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	}
}

Texture* Texture::createTexture(const std::string& path, unsigned int unit) {
	//1 检查是否缓存过本路径对应的纹理对象
	auto iter = mTextureCache.find(path);
	if (iter != mTextureCache.end()) {
		//对于iterater（迭代器），first-》key  second-》value
		return iter->second;
	}

	//2 如果本路径对应的texture没有生成过，则重新生成
	auto texture = new Texture(path, unit, GL_SRGB_ALPHA);
	mTextureCache[path] = texture;

	return texture;
}

Texture* Texture::createTextureFromMemory(
	const std::string& path,
	unsigned int unit,
	unsigned char* dataIn,
	uint32_t widthIn,
	uint32_t heightIn,
	unsigned int internalFormat
) {
	const std::string cacheKey = path + "#" + std::to_string(internalFormat);
	auto iter = mTextureCache.find(cacheKey);
	if (iter != mTextureCache.end()) {
		return iter->second;
	}

	auto texture = new Texture(unit, dataIn, widthIn, heightIn, internalFormat);
	mTextureCache[cacheKey] = texture;

	return texture;
}

Texture* Texture::createSolidTexture(
	const std::string& name,
	unsigned char red,
	unsigned char green,
	unsigned char blue,
	unsigned char alpha,
	unsigned int internalFormat
) {
	const std::string cacheKey = "solid:" + name + "#" + std::to_string(internalFormat);
	auto iter = mTextureCache.find(cacheKey);
	if (iter != mTextureCache.end()) {
		return iter->second;
	}

	Texture* texture = new Texture(1, 1, 0, internalFormat);
	const unsigned char pixel[] = { red, green, blue, alpha };
	texture->bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	mTextureCache[cacheKey] = texture;
	return texture;
}

Texture* Texture::createColorAttachment(
	unsigned int width,
	unsigned int height,
	unsigned int unit
) {
	return new Texture(width, height, unit);
}

Texture* Texture::createDepthStencilAttachment(
	unsigned int width,
	unsigned int height,
	unsigned int unit
) {
	Texture* dsTex = new Texture();

	unsigned int depthStencil;
	glGenTextures(1, &depthStencil);
	glBindTexture(GL_TEXTURE_2D, depthStencil);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	dsTex->mTexture = depthStencil;
	dsTex->mWidth = width;
	dsTex->mHeight = height;
	dsTex->mUnit = unit;
	dsTex->mEstimatedGpuBytes = static_cast<std::size_t>(width) * height * 4u;

	return dsTex;
}

Texture* Texture::createDepthAttachment(
	unsigned int width,
	unsigned int height,
	unsigned int unit
) {
	Texture* depthTex = new Texture();

	unsigned int depth;
	glGenTextures(1, &depth);
	glBindTexture(GL_TEXTURE_2D, depth);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GLfloat borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);//u
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);//v

	depthTex->mTexture = depth;
	depthTex->mWidth = width;
	depthTex->mHeight = height;
	depthTex->mUnit = unit;
	depthTex->mEstimatedGpuBytes = static_cast<std::size_t>(width) * height * 4u;

	return depthTex;
}

Texture* Texture::createDepthAttachmentCSMArray(
	unsigned int width,
	unsigned int height,
	unsigned int layerNum,
	unsigned int unit
){
	Texture* dTex = new Texture();

	unsigned int depth;
	glGenTextures(1, &depth);
	glBindTexture(GL_TEXTURE_2D_ARRAY, depth);

	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, width, height, layerNum, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GLfloat borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);//u
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);//v
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	dTex->mTexture = depth;
	dTex->mWidth = width;
	dTex->mHeight = height;
	dTex->mUnit = unit;
	dTex->mTextureTarget = GL_TEXTURE_2D_ARRAY;
	dTex->mEstimatedGpuBytes = static_cast<std::size_t>(width) * height * layerNum * 4u;

	return dTex;
}

Texture* Texture::createMultiSampleTexture(
	unsigned int width,
	unsigned int height,
	unsigned int samples,
	unsigned int format,
	unsigned int unit
) {
	Texture* tex = new Texture();

	GLuint glTex;
	glGenTextures(1, &glTex);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, glTex);

	glTexImage2DMultisample(
		GL_TEXTURE_2D_MULTISAMPLE,
		samples,
		format,
		width, height,
		GL_TRUE);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

	tex->mTexture = glTex;
	tex->mWidth = width;
	tex->mHeight = height;
	tex->mUnit = unit;
	tex->mTextureTarget = GL_TEXTURE_2D_MULTISAMPLE;
	const std::size_t bytesPerPixel = format == GL_RGB16F ? 6u : 4u;
	tex->mEstimatedGpuBytes =
		static_cast<std::size_t>(width) * height * samples * bytesPerPixel;

	return tex;
}

Texture* Texture::createHDRTexture(
	unsigned int width,
	unsigned int height,
	unsigned int unit
) {
	Texture* tex = new Texture();

	GLuint glTex;
	glGenTextures(1, &glTex);
	glBindTexture(GL_TEXTURE_2D, glTex);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGB16F,
		width, height, 0,
		GL_RGB,
		GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);//u
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);//v

	tex->mTexture = glTex;
	tex->mWidth = width;
	tex->mHeight = height;
	tex->mUnit = unit;
	tex->mTextureTarget = GL_TEXTURE_2D;
	tex->mEstimatedGpuBytes = static_cast<std::size_t>(width) * height * 6u;

	return tex;
}

Texture* Texture::createNearestTexture(std::string path) {
	auto texture = new Texture(path, 0);
	glBindTexture(GL_TEXTURE_2D, texture->mTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	return texture;
}


Texture* Texture::createExrCubeMap(std::vector<std::string> paths) {
	Texture* tex = new Texture();

	GLuint glTex;
	glGenTextures(1, &glTex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, glTex);

	int width = 0, height = 0;
	for (int i = 0; i < paths.size(); i++) {
		float* data = nullptr;
		const char* err = nullptr;
		int ret = LoadEXR(&data, &width, &height, paths[i].c_str(), &err);
		if (ret == TINYEXR_SUCCESS && data != nullptr) {
			GL_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, width, height, 0, GL_RGBA, GL_FLOAT, data));
			std::free(data);
		}
		else {
			std::cout << "Error: EXR cubemap face failed to load: " << paths[i] << std::endl;
			if (err != nullptr) {
				std::cout << err << std::endl;
				FreeEXRErrorMessage(err);
			}
		}
	}

	GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	tex->mTexture = glTex;
	tex->mWidth = width;
	tex->mHeight = height;
	tex->mUnit = 0;
	tex->mTextureTarget = GL_TEXTURE_CUBE_MAP;
	tex->mEstimatedGpuBytes = static_cast<std::size_t>(width) * height * 6u * 6u;

	return tex;
}

Texture* Texture::createExrTexture(const std::string& path) {
	Texture* tex = new Texture();

	GLuint glTex;
	glGenTextures(1, &glTex);
	glBindTexture(GL_TEXTURE_2D, glTex);

	int width = 0, height = 0;
	float* data = nullptr;
	const char* err = nullptr;
	int ret = LoadEXR(&data, &width, &height, path.c_str(), &err);
	if (ret == TINYEXR_SUCCESS && data != nullptr) {
		int numChannels = 4;
		for (int y = 0; y < height / 2; ++y) {
			int oppositeY = height - y - 1;
			for (int x = 0; x < width * numChannels; ++x) {
				std::swap(data[y * width * numChannels + x], data[oppositeY * width * numChannels + x]);
			}
		}
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGBA, GL_FLOAT, data);
		std::free(data);
	}
	else {
		std::cout << "Error: EXR texture failed to load: " << path << std::endl;
		if (err != nullptr) {
			std::cout << err << std::endl;
			FreeEXRErrorMessage(err);
		}
	}

	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	glBindTexture(GL_TEXTURE_2D, 0);

	tex->mTexture = glTex;
	tex->mWidth = width;
	tex->mHeight = height;
	tex->mUnit = 0;
	tex->mTextureTarget = GL_TEXTURE_2D;
	tex->mEstimatedGpuBytes = static_cast<std::size_t>(width) * height * 6u;

	return tex;
}

Texture* Texture::createEnvironmentCubeMap(Texture* equirectangularMap, uint32_t size, unsigned int unit) {
	if (equirectangularMap == nullptr) {
		return nullptr;
	}

	Texture* tex = new Texture();
	GLuint glTex;
	glGenTextures(1, &glTex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, glTex);
	for (int i = 0; i < 6; ++i) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLint prevFbo;
	GLint prevViewport[4];
	saveFramebufferState(prevFbo, prevViewport);

	GLuint captureFbo;
	GLuint captureRbo;
	glGenFramebuffers(1, &captureFbo);
	glGenRenderbuffers(1, &captureRbo);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFbo);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRbo);

	Shader shader("assets/shaders/advanced/ibl/cubemapCapture.vert", "assets/shaders/advanced/ibl/equirectangularToCubemap.frag");
	shader.begin();
	shader.setInt("equirectangularMap", 0);
	shader.setMatrix4x4("projection", glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f));
	equirectangularMap->setUnit(0);
	equirectangularMap->bind();

	auto views = captureViews();
	glViewport(0, 0, size, size);
	for (int i = 0; i < 6; ++i) {
		shader.setMatrix4x4("view", views[i]);
		GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, glTex, 0));
		GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		renderCaptureCube();
	}
	shader.end();

	glBindTexture(GL_TEXTURE_CUBE_MAP, glTex);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	glDeleteRenderbuffers(1, &captureRbo);
	glDeleteFramebuffers(1, &captureFbo);
	restoreFramebufferState(prevFbo, prevViewport);

	tex->mTexture = glTex;
	tex->mWidth = size;
	tex->mHeight = size;
	tex->mUnit = unit;
	tex->mTextureTarget = GL_TEXTURE_CUBE_MAP;
	tex->mEstimatedGpuBytes = static_cast<std::size_t>(size) * size * 6u * 6u * 4u / 3u;
	return tex;
}

Texture* Texture::createIrradianceCubeMap(Texture* environmentMap, uint32_t size, unsigned int unit) {
	if (environmentMap == nullptr) {
		return nullptr;
	}

	Texture* tex = new Texture();
	GLuint glTex;
	glGenTextures(1, &glTex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, glTex);
	for (int i = 0; i < 6; ++i) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLint prevFbo;
	GLint prevViewport[4];
	saveFramebufferState(prevFbo, prevViewport);

	GLuint captureFbo;
	GLuint captureRbo;
	glGenFramebuffers(1, &captureFbo);
	glGenRenderbuffers(1, &captureRbo);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFbo);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRbo);

	Shader shader("assets/shaders/advanced/ibl/cubemapCapture.vert", "assets/shaders/advanced/ibl/irradianceConvolution.frag");
	shader.begin();
	shader.setInt("environmentMap", 0);
	shader.setMatrix4x4("projection", glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f));
	environmentMap->setUnit(0);
	environmentMap->bind();

	auto views = captureViews();
	glViewport(0, 0, size, size);
	for (int i = 0; i < 6; ++i) {
		shader.setMatrix4x4("view", views[i]);
		GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, glTex, 0));
		GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		renderCaptureCube();
	}
	shader.end();

	glDeleteRenderbuffers(1, &captureRbo);
	glDeleteFramebuffers(1, &captureFbo);
	restoreFramebufferState(prevFbo, prevViewport);

	tex->mTexture = glTex;
	tex->mWidth = size;
	tex->mHeight = size;
	tex->mUnit = unit;
	tex->mTextureTarget = GL_TEXTURE_CUBE_MAP;
	tex->mEstimatedGpuBytes = static_cast<std::size_t>(size) * size * 6u * 6u;
	return tex;
}

Texture* Texture::createPrefilterCubeMap(Texture* environmentMap, uint32_t size, uint32_t mipLevels, unsigned int unit) {
	if (environmentMap == nullptr) {
		return nullptr;
	}

	Texture* tex = new Texture();
	GLuint glTex;
	glGenTextures(1, &glTex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, glTex);
	for (uint32_t mip = 0; mip < mipLevels; ++mip) {
		uint32_t mipSize = static_cast<uint32_t>(size * std::pow(0.5f, static_cast<float>(mip)));
		for (int i = 0; i < 6; ++i) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip, GL_RGB16F, mipSize, mipSize, 0, GL_RGB, GL_FLOAT, nullptr);
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(mipLevels - 1));

	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap->mTexture);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	GLint prevFbo;
	GLint prevViewport[4];
	saveFramebufferState(prevFbo, prevViewport);

	GLuint captureFbo;
	GLuint captureRbo;
	glGenFramebuffers(1, &captureFbo);
	glGenRenderbuffers(1, &captureRbo);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFbo);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRbo);

	Shader shader("assets/shaders/advanced/ibl/cubemapCapture.vert", "assets/shaders/advanced/ibl/prefilter.frag");
	shader.begin();
	shader.setInt("environmentMap", 0);
	shader.setMatrix4x4("projection", glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f));
	environmentMap->setUnit(0);
	environmentMap->bind();

	auto views = captureViews();
	for (uint32_t mip = 0; mip < mipLevels; ++mip) {
		uint32_t mipSize = static_cast<uint32_t>(size * std::pow(0.5f, static_cast<float>(mip)));
		glBindRenderbuffer(GL_RENDERBUFFER, captureRbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize);
		glViewport(0, 0, mipSize, mipSize);

		float roughness = static_cast<float>(mip) / static_cast<float>(mipLevels - 1);
		shader.setFloat("roughness", roughness);
		for (int i = 0; i < 6; ++i) {
			shader.setMatrix4x4("view", views[i]);
			GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, glTex, mip));
			GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
			renderCaptureCube();
		}
	}
	shader.end();

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glDeleteRenderbuffers(1, &captureRbo);
	glDeleteFramebuffers(1, &captureFbo);
	restoreFramebufferState(prevFbo, prevViewport);

	tex->mTexture = glTex;
	tex->mWidth = size;
	tex->mHeight = size;
	tex->mUnit = unit;
	tex->mTextureTarget = GL_TEXTURE_CUBE_MAP;
	std::size_t mipPixels = 0;
	for (uint32_t mip = 0; mip < mipLevels; ++mip) {
		const uint32_t mipSize = std::max(1u, size >> mip);
		mipPixels += static_cast<std::size_t>(mipSize) * mipSize;
	}
	tex->mEstimatedGpuBytes = mipPixels * 6u * 6u;
	return tex;
}

Texture* Texture::createBrdfLut(uint32_t size, unsigned int unit) {
	Texture* tex = new Texture();
	GLuint glTex;
	glGenTextures(1, &glTex);
	glBindTexture(GL_TEXTURE_2D, glTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, size, size, 0, GL_RG, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLint prevFbo;
	GLint prevViewport[4];
	saveFramebufferState(prevFbo, prevViewport);

	GLuint captureFbo;
	glGenFramebuffers(1, &captureFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFbo);
	GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glTex, 0));
	glViewport(0, 0, size, size);
	GL_CALL(glClear(GL_COLOR_BUFFER_BIT));

	Shader shader("assets/shaders/advanced/ibl/brdf.vert", "assets/shaders/advanced/ibl/brdf.frag");
	shader.begin();
	renderCaptureQuad();
	shader.end();

	glDeleteFramebuffers(1, &captureFbo);
	restoreFramebufferState(prevFbo, prevViewport);
	glBindTexture(GL_TEXTURE_2D, 0);

	tex->mTexture = glTex;
	tex->mWidth = size;
	tex->mHeight = size;
	tex->mUnit = unit;
	tex->mTextureTarget = GL_TEXTURE_2D;
	tex->mEstimatedGpuBytes = static_cast<std::size_t>(size) * size * 4u;
	return tex;
}



Texture::Texture() {

}
Texture::Texture(const std::string& path, unsigned int unit, unsigned int internalFormat) {
	mUnit = unit;

	//1 stbImage 读取图片
	int channels;

	//--反转y轴
	stbi_set_flip_vertically_on_load(true);

	unsigned char* data = stbi_load(path.c_str(), &mWidth, &mHeight, &channels, STBI_rgb_alpha);

	//2 生成纹理并且激活单元绑定
	glGenTextures(1, &mTexture);
	//--激活纹理单元--
	glActiveTexture(GL_TEXTURE0 + mUnit);
	// 绑定纹理对象。
	glBindTexture(GL_TEXTURE_2D, mTexture);

	//3 传输纹理数据,开辟显存
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	
//	glGenerateMipmap(GL_TEXTURE_2D);

	// 释放图像数据。
	stbi_image_free(data);

	//4 设置纹理的过滤方式
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);

	//5 设置纹理的包裹方式
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);//u
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);//v
	mEstimatedGpuBytes = static_cast<std::size_t>(mWidth) * mHeight * 4u;
}

Texture::Texture(
	unsigned int unit,
	unsigned char* dataIn,
	uint32_t widthIn,
	uint32_t heightIn,
	unsigned int internalFormat
) {
	mUnit = unit;

	//1 stbImage 读取图片
	int channels;

	//--反转y轴
	stbi_set_flip_vertically_on_load(true);

	//计算整张图片的大小
	//Assimp规定：如果内嵌纹理是png或者jpg压缩格式的话，height = 0，width就代表了图片大小
	uint32_t dataInSize = 0;
	if (!heightIn) {
		dataInSize = widthIn;
	}
	else {
		//如果内嵌图片不是压缩格式，height = 正常高度，width = 正常宽度
		//偷懒：统一认为数据格式都是RGBA
		dataInSize = widthIn * heightIn * 4;//字节为单位
	}


	unsigned char* data = stbi_load_from_memory(dataIn, dataInSize, &mWidth, &mHeight, &channels, STBI_rgb_alpha);

	//2 生成纹理并且激活单元绑定
	glGenTextures(1, &mTexture);
	//--激活纹理单元--
	glActiveTexture(GL_TEXTURE0 + mUnit);
	//--绑定纹理对象--
	glBindTexture(GL_TEXTURE_2D, mTexture);

	//3 传输纹理数据,开辟显存
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glGenerateMipmap(GL_TEXTURE_2D);

	// 释放图像数据。
	stbi_image_free(data);

	//4 设置纹理的过滤方式
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);

	//5 设置纹理的包裹方式
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);//u
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);//v
	mEstimatedGpuBytes = static_cast<std::size_t>(mWidth) * mHeight * 4u * 4u / 3u;
}

Texture::Texture(unsigned int width, unsigned int height, unsigned int unit, unsigned int internalFormat) {
	mWidth = width;
	mHeight = height;
	mUnit = unit;

	glGenTextures(1, &mTexture);
	glActiveTexture(GL_TEXTURE0 + mUnit);
	glBindTexture(GL_TEXTURE_2D, mTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mEstimatedGpuBytes = static_cast<std::size_t>(mWidth) * mHeight * 4u;
}

//paths:右左上下后前(+x -x +y -y +z -z)
Texture::Texture(const std::vector<std::string>& paths, unsigned int unit, unsigned int internalFormat) {
	mUnit = unit;
	mTextureTarget = GL_TEXTURE_CUBE_MAP;

	//cubemap不需要反转y轴
	stbi_set_flip_vertically_on_load(false);

	//1 创建CubeMap对象
	glGenTextures(1, &mTexture);
	glActiveTexture(GL_TEXTURE0 + mUnit);
	glBindTexture(GL_TEXTURE_CUBE_MAP, mTexture);


	//2 循环读取六张贴图，并且放置到cubemap的六个GPU空间内
	int channels;
	int width = 0, height = 0;
	unsigned char* data = nullptr;
	for (int i = 0; i < paths.size(); i++) {
		data = stbi_load(paths[i].c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (data != nullptr) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
		else {
			std::cout << "Error: CubeMap Texture failed to load at path　－" << paths[i] << std::endl;
			stbi_image_free(data);
		}
	}
	mWidth = width;
	mHeight = height;
	mEstimatedGpuBytes = static_cast<std::size_t>(width) * height * paths.size() * 4u;

	//3 设置纹理参数
	glTexParameteri(mTextureTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(mTextureTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(mTextureTarget, GL_TEXTURE_WRAP_S, GL_REPEAT);//u
	glTexParameteri(mTextureTarget, GL_TEXTURE_WRAP_T, GL_REPEAT);//v
}


Texture::~Texture() {
	if (mTexture != 0) {
		glDeleteTextures(1, &mTexture);
	}
}

void Texture::bind() {
	//先切换纹理单元，然后绑定texture对象
	glActiveTexture(GL_TEXTURE0 + mUnit);
	glBindTexture(mTextureTarget, mTexture);
}
