#pragma once

#include "core.h"
#include <cstddef>

class Geometry {
public:
	Geometry();
	Geometry(
		const std::vector<float>& positions,
		const std::vector<float>& normals,
		const std::vector<float>& uvs,
		const std::vector<unsigned int>& indices
	);

	Geometry(
		const std::vector<float>& positions,
		const std::vector<float>& normals,
		const std::vector<float>& uvs,
		const std::vector<float>& colors,
		const std::vector<unsigned int>& indices
	);

	Geometry(
		const std::vector<float>& positions,
		const std::vector<float>& normals,
		const std::vector<float>& uvs,
		const std::vector<unsigned int>& indices,
		const std::vector<float>& tangents
	);

	~Geometry();

	static Geometry* createBox(float size);
	static Geometry* createSphere(float radius, int latitudeSegments = 60, int longitudeSegments = 60);
	static Geometry* createRock(
		float radius,
		int latitudeSegments = 12,
		int longitudeSegments = 18,
		float irregularity = 0.22f
	);
	static Geometry* createPlane(float width, float height);
	static Geometry* createScreenPlane();
	Geometry* createSimplified(float targetRatio, float targetError) const;

	void setSkinningData(
		const std::vector<glm::ivec4>& boneIds,
		const std::vector<glm::vec4>& boneWeights
	);

	GLuint getVao()const { return mVao; }
	uint32_t getIndicesCount()const { return mIndicesCount; }
	std::size_t getEstimatedGpuBytes() const { return mEstimatedGpuBytes; }
	const glm::vec3& getMinBounds() const { return mMinBounds; }
	const glm::vec3& getMaxBounds() const { return mMaxBounds; }
	bool hasSkinningData() const { return mHasSkinningData; }
	float getSimplificationError() const { return mSimplificationError; }

private:
	void updateBounds(const std::vector<float>& positions);

	GLuint mVao{ 0 };

	GLuint mPosVbo{ 0 };
	GLuint mUvVbo{ 0 };
	GLuint mNormalVbo{ 0 };
	GLuint mColorVbo{ 0 };
	GLuint mTangentVbo{ 0 };
	GLuint mBoneIdsVbo{ 0 };
	GLuint mBoneWeightsVbo{ 0 };

	GLuint mEbo{ 0 };

	uint32_t mIndicesCount{ 0 };
	std::size_t mEstimatedGpuBytes{ 0 };
	glm::vec3 mMinBounds{ 0.0f };
	glm::vec3 mMaxBounds{ 0.0f };
	bool mHasSkinningData{ false };
	float mSimplificationError{ 0.0f };
	std::vector<float> mPositions;
	std::vector<float> mNormals;
	std::vector<float> mUvs;
	std::vector<float> mColors;
	std::vector<float> mTangents;
	std::vector<unsigned int> mIndices;
};
