#pragma once
#include"core.h"
#include <cstddef>
#include <string>

class Texture {
public:
	static Texture* createTexture(const std::string& path, unsigned int unit);
	static Texture* createTextureFromMemory(
		const std::string& path, 
		unsigned int unit,
		unsigned char* dataIn,
		uint32_t widthIn,
		uint32_t heightIn,
		unsigned int internalFormat = GL_SRGB_ALPHA
	);
	static Texture* createSolidTexture(
		const std::string& name,
		unsigned char red,
		unsigned char green,
		unsigned char blue,
		unsigned char alpha = 255,
		unsigned int internalFormat = GL_RGBA
	);

	static Texture* createColorAttachment(
		unsigned int width, 
		unsigned int height, 
		unsigned int unit
	);

	static Texture* createDepthStencilAttachment(
		unsigned int width,
		unsigned int height,
		unsigned int unit
	);

	static Texture* createDepthAttachment(
		unsigned int width,
		unsigned int height,
		unsigned int unit
	);

	static Texture* createDepthAttachmentCSMArray(
		unsigned int width,
		unsigned int height,
		unsigned int layerNum,
		unsigned int unit
	);

	static Texture* createMultiSampleTexture(
		unsigned int width,
		unsigned int height,
		unsigned int samples,
		unsigned int format,
		unsigned int unit
	);

	static Texture* createHDRTexture(
		unsigned int width,
		unsigned int height,
		unsigned int unit
	);



	static Texture* createNearestTexture(std::string path);

	static Texture* createExrCubeMap(std::vector<std::string> paths);

	static Texture* createExrTexture(const std::string& path);

	static Texture* createEnvironmentCubeMap(Texture* equirectangularMap, uint32_t size = 512, unsigned int unit = 0);
	static Texture* createIrradianceCubeMap(Texture* environmentMap, uint32_t size = 32, unsigned int unit = 0);
	static Texture* createPrefilterCubeMap(Texture* environmentMap, uint32_t size = 128, uint32_t mipLevels = 5, unsigned int unit = 0);
	static Texture* createBrdfLut(uint32_t size = 512, unsigned int unit = 0);

	Texture();
	Texture(const std::string& path, unsigned int unit, unsigned int internalFormat = GL_RGBA);
	Texture(
		unsigned int unit,
		unsigned char* dataIn,
		uint32_t widthIn,
		uint32_t heightIn,
		unsigned int internalFormat = GL_RGBA
	);
	Texture(unsigned int width, unsigned int height, unsigned int unit, unsigned int internalFormat = GL_RGBA);
	Texture(const std::vector<std::string>& paths, unsigned int unit, unsigned int internalFormat = GL_RGBA);

	~Texture();

	void bind();
	void setUnit(unsigned int unit) { mUnit = unit; }

	int getWidth()const { return mWidth; }
	int getHeight()const { return mHeight; }
	GLuint getTexture()const { return mTexture; }
	std::size_t getEstimatedGpuBytes() const { return mEstimatedGpuBytes; }

private:
	GLuint mTexture{ 0 };
	int mWidth{ 0 };
	int mHeight{ 0 };
	unsigned int mUnit{ 0 };
	unsigned int mTextureTarget{ GL_TEXTURE_2D };
	std::size_t mEstimatedGpuBytes{ 0 };

	//注意：静态！！属于类的不属于某个对象
	static std::map<std::string, Texture*> mTextureCache;
};
