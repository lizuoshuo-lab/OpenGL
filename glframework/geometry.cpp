#include "geometry.h"
#include "meshoptimizer.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

Geometry::Geometry() {

}

Geometry::Geometry(
	const std::vector<float>& positions,
	const std::vector<float>& normals,
	const std::vector<float>& uvs,
	const std::vector<unsigned int>& indices
) {
	mPositions = positions;
	mNormals = normals;
	mUvs = uvs;
	mIndices = indices;
	mIndicesCount = static_cast<uint32_t>(indices.size());
	updateBounds(positions);
	mEstimatedGpuBytes =
		(positions.size() + normals.size() + uvs.size()) * sizeof(float) +
		indices.size() * sizeof(unsigned int);

	glGenBuffers(1, &mPosVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mPosVbo);
	glBufferData(GL_ARRAY_BUFFER, positions.size()*sizeof(float), positions.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &mUvVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mUvVbo);
	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(float), uvs.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &mNormalVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mNormalVbo);
	glBufferData(GL_ARRAY_BUFFER, normals.size()*sizeof(float), normals.data(), GL_STATIC_DRAW);

	//3 EBO创建
	glGenBuffers(1, &mEbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	//4 VAO创建
	glGenVertexArrays(1, &mVao);
	glBindVertexArray(mVao);

	glBindBuffer(GL_ARRAY_BUFFER, mPosVbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, mUvVbo);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, mNormalVbo);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	//5.4 加入ebo到当前的vao
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);

	glBindVertexArray(0);
}

Geometry::Geometry(
	const std::vector<float>& positions,
	const std::vector<float>& normals,
	const std::vector<float>& uvs,
	const std::vector<float>& colors,
	const std::vector<unsigned int>& indices
) {
	mPositions = positions;
	mNormals = normals;
	mUvs = uvs;
	mColors = colors;
	mIndices = indices;
	mIndicesCount = static_cast<uint32_t>(indices.size());
	updateBounds(positions);
	mEstimatedGpuBytes =
		(positions.size() + normals.size() + uvs.size() + colors.size()) * sizeof(float) +
		indices.size() * sizeof(unsigned int);

	glGenBuffers(1, &mPosVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mPosVbo);
	glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &mUvVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mUvVbo);
	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(float), uvs.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &mNormalVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mNormalVbo);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &mColorVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mColorVbo);
	glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(float), colors.data(), GL_STATIC_DRAW);

	//3 EBO创建
	glGenBuffers(1, &mEbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	//4 VAO创建
	glGenVertexArrays(1, &mVao);
	glBindVertexArray(mVao);

	glBindBuffer(GL_ARRAY_BUFFER, mPosVbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, mUvVbo);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, mNormalVbo);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, mColorVbo);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	//5.4 加入ebo到当前的vao
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);

	glBindVertexArray(0);
}

Geometry::Geometry(
	const std::vector<float>& positions,
	const std::vector<float>& normals,
	const std::vector<float>& uvs,
	const std::vector<unsigned int>& indices,
	const std::vector<float>& tangents
) {
	mPositions = positions;
	mNormals = normals;
	mUvs = uvs;
	mTangents = tangents;
	mIndices = indices;
	mIndicesCount = static_cast<uint32_t>(indices.size());
	updateBounds(positions);
	mEstimatedGpuBytes =
		(positions.size() + normals.size() + uvs.size() + tangents.size()) * sizeof(float) +
		indices.size() * sizeof(unsigned int);

	glGenBuffers(1, &mPosVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mPosVbo);
	glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &mUvVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mUvVbo);
	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(float), uvs.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &mNormalVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mNormalVbo);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &mTangentVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mTangentVbo);
	glBufferData(GL_ARRAY_BUFFER, tangents.size() * sizeof(float), tangents.data(), GL_STATIC_DRAW);

	//3 EBO创建
	glGenBuffers(1, &mEbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	//4 VAO创建
	glGenVertexArrays(1, &mVao);
	glBindVertexArray(mVao);

	glBindBuffer(GL_ARRAY_BUFFER, mPosVbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, mUvVbo);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, mNormalVbo);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, mTangentVbo);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	//5.4 加入ebo到当前的vao
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);

	glBindVertexArray(0);
}

Geometry::~Geometry() {
	if (mVao != 0) {
		glDeleteVertexArrays(1, &mVao);
	}
	if (mPosVbo != 0) {
		glDeleteBuffers(1, &mPosVbo);
	}
	if (mUvVbo != 0) {
		glDeleteBuffers(1, &mUvVbo);
	}
	if (mEbo != 0) {
		glDeleteBuffers(1, &mEbo);
	}
	if (mNormalVbo != 0) {
		glDeleteBuffers(1, &mNormalVbo);
	}
	if (mColorVbo != 0) {
		glDeleteBuffers(1, &mColorVbo);
	}
	if (mTangentVbo != 0) {
		glDeleteBuffers(1, &mTangentVbo);
	}
	if (mBoneIdsVbo != 0) {
		glDeleteBuffers(1, &mBoneIdsVbo);
	}
	if (mBoneWeightsVbo != 0) {
		glDeleteBuffers(1, &mBoneWeightsVbo);
	}
}

Geometry* Geometry::createSimplified(float targetRatio, float targetError) const {
	const std::size_t vertexCount = mPositions.size() / 3;
	if (vertexCount == 0 || mIndices.size() < 6) {
		return nullptr;
	}

	const float ratio = std::clamp(targetRatio, 0.01f, 1.0f);
	std::size_t targetIndexCount = static_cast<std::size_t>(
		static_cast<double>(mIndices.size()) * ratio
	);
	targetIndexCount = std::max<std::size_t>(3, targetIndexCount - targetIndexCount % 3);

	std::vector<unsigned int> simplifiedIndices(mIndices.size());
	float resultError = 0.0f;
	std::size_t resultCount = 0;
	const bool hasNormals = mNormals.size() == vertexCount * 3;
	const bool hasUvs = mUvs.size() == vertexCount * 2;
	if (hasNormals && hasUvs) {
		std::vector<float> attributes(vertexCount * 5);
		for (std::size_t vertex = 0; vertex < vertexCount; ++vertex) {
			attributes[vertex * 5 + 0] = mNormals[vertex * 3 + 0];
			attributes[vertex * 5 + 1] = mNormals[vertex * 3 + 1];
			attributes[vertex * 5 + 2] = mNormals[vertex * 3 + 2];
			attributes[vertex * 5 + 3] = mUvs[vertex * 2 + 0];
			attributes[vertex * 5 + 4] = mUvs[vertex * 2 + 1];
		}
		const float attributeWeights[5] = { 0.75f, 0.75f, 0.75f, 16.0f, 16.0f };
		resultCount = meshopt_simplifyWithAttributes(
			simplifiedIndices.data(),
			mIndices.data(),
			mIndices.size(),
			mPositions.data(),
			vertexCount,
			sizeof(float) * 3,
			attributes.data(),
			sizeof(float) * 5,
			attributeWeights,
			5,
			nullptr,
			targetIndexCount,
			std::max(targetError, 0.0001f),
			meshopt_SimplifyPrune,
			&resultError
		);
	}
	else {
		resultCount = meshopt_simplify(
			simplifiedIndices.data(),
			mIndices.data(),
			mIndices.size(),
			mPositions.data(),
			vertexCount,
			sizeof(float) * 3,
			targetIndexCount,
			std::max(targetError, 0.0001f),
			meshopt_SimplifyPrune,
			&resultError
		);
	}

	if (resultCount < 3 || resultCount >= mIndices.size()) {
		return nullptr;
	}
	simplifiedIndices.resize(resultCount - resultCount % 3);
	Geometry* simplified = nullptr;
	if (!mTangents.empty()) {
		simplified = new Geometry(
			mPositions,
			mNormals,
			mUvs,
			simplifiedIndices,
			mTangents
		);
	}
	else if (!mColors.empty()) {
		simplified = new Geometry(
			mPositions,
			mNormals,
			mUvs,
			mColors,
			simplifiedIndices
		);
	}
	else {
		simplified = new Geometry(mPositions, mNormals, mUvs, simplifiedIndices);
	}
	simplified->mSimplificationError = resultError;
	return simplified;
}

void Geometry::setSkinningData(
	const std::vector<glm::ivec4>& boneIds,
	const std::vector<glm::vec4>& boneWeights
) {
	if (mVao == 0 || boneIds.empty() || boneIds.size() != boneWeights.size()) {
		return;
	}

	glBindVertexArray(mVao);

	glGenBuffers(1, &mBoneIdsVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mBoneIdsVbo);
	glBufferData(
		GL_ARRAY_BUFFER,
		boneIds.size() * sizeof(glm::ivec4),
		boneIds.data(),
		GL_STATIC_DRAW
	);
	glEnableVertexAttribArray(8);
	glVertexAttribIPointer(8, 4, GL_INT, sizeof(glm::ivec4), nullptr);

	glGenBuffers(1, &mBoneWeightsVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mBoneWeightsVbo);
	glBufferData(
		GL_ARRAY_BUFFER,
		boneWeights.size() * sizeof(glm::vec4),
		boneWeights.data(),
		GL_STATIC_DRAW
	);
	glEnableVertexAttribArray(9);
	glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);

	glBindVertexArray(0);
	mEstimatedGpuBytes +=
		boneIds.size() * sizeof(glm::ivec4) +
		boneWeights.size() * sizeof(glm::vec4);
	mHasSkinningData = true;
}

void Geometry::updateBounds(const std::vector<float>& positions) {
	if (positions.size() < 3) {
		mMinBounds = glm::vec3(0.0f);
		mMaxBounds = glm::vec3(0.0f);
		return;
	}

	const float maxValue = std::numeric_limits<float>::max();
	mMinBounds = glm::vec3(maxValue);
	mMaxBounds = glm::vec3(-maxValue);
	for (std::size_t i = 0; i + 2 < positions.size(); i += 3) {
		const glm::vec3 position(positions[i], positions[i + 1], positions[i + 2]);
		mMinBounds = glm::min(mMinBounds, position);
		mMaxBounds = glm::max(mMaxBounds, position);
	}
}

Geometry* Geometry::createBox(float size) {
	Geometry* geometry = new Geometry();
	geometry->mIndicesCount = 36;

	float halfSize = size / 2.0f;
	geometry->mMinBounds = glm::vec3(-halfSize);
	geometry->mMaxBounds = glm::vec3(halfSize);

	float positions[] = {
		// Front face
		-halfSize, -halfSize, halfSize, halfSize, -halfSize, halfSize, halfSize, halfSize, halfSize, -halfSize, halfSize, halfSize,
		// Back face
		-halfSize, -halfSize, -halfSize, -halfSize, halfSize, -halfSize, halfSize, halfSize, -halfSize, halfSize, -halfSize, -halfSize,
		// Top face
		-halfSize, halfSize, halfSize, halfSize, halfSize, halfSize, halfSize, halfSize, -halfSize, -halfSize, halfSize, -halfSize,
		// Bottom face
		-halfSize, -halfSize, -halfSize, halfSize, -halfSize, -halfSize, halfSize, -halfSize, halfSize, -halfSize, -halfSize, halfSize,
		// Right face
		halfSize, -halfSize, halfSize, halfSize, -halfSize, -halfSize, halfSize, halfSize, -halfSize, halfSize, halfSize, halfSize,
		// Left face
		-halfSize, -halfSize, -halfSize, -halfSize, -halfSize, halfSize, -halfSize, halfSize, halfSize, -halfSize, halfSize, -halfSize
	};

	float uvs[] = {
		0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
		0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
		0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
		0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
		0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
		0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	};

	float normals[] = {
		//前面
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		//后面
		0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, -1.0f,

		//上面
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,

		//下面
		0.0f, -1.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, -1.0f, 0.0f,

		//右面
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,

		//左面
		-1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
	};

	float tangents[] = {
		// Front face
		1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
		// Back face
		-1.0f, 0.0f, 0.0f,  -1.0f, 0.0f, 0.0f,  -1.0f, 0.0f, 0.0f,  -1.0f, 0.0f, 0.0f,
		// Top face
		1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
		// Bottom face
		1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
		// Right face
		0.0f, 0.0f, -1.0f,  0.0f, 0.0f, -1.0f,  0.0f, 0.0f, -1.0f,  0.0f, 0.0f, -1.0f,
		// Left face
		0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f
	};

	unsigned int indices[] = {
		0, 1, 2, 2, 3, 0,   // Front face
		4, 5, 6, 6, 7, 4,   // Back face
		8, 9, 10, 10, 11, 8,  // Top face
		12, 13, 14, 14, 15, 12, // Bottom face
		16, 17, 18, 18, 19,  16, // Right face
		20, 21, 22, 22, 23, 20  // Left face
	};
	geometry->mEstimatedGpuBytes =
		sizeof(positions) + sizeof(uvs) + sizeof(normals) + sizeof(tangents) + sizeof(indices);

	//2 VBO创建
	glGenBuffers(1, &geometry->mPosVbo);
	glBindBuffer(GL_ARRAY_BUFFER, geometry->mPosVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);

	glGenBuffers(1, &geometry->mUvVbo);
	glBindBuffer(GL_ARRAY_BUFFER, geometry->mUvVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);

	glGenBuffers(1, &geometry->mNormalVbo);
	glBindBuffer(GL_ARRAY_BUFFER, geometry->mNormalVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(normals), normals, GL_STATIC_DRAW);

	glGenBuffers(1, &geometry->mTangentVbo);
	glBindBuffer(GL_ARRAY_BUFFER, geometry->mTangentVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(tangents), tangents, GL_STATIC_DRAW);

	//3 EBO创建
	glGenBuffers(1, &geometry->mEbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->mEbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	//4 VAO创建
	glGenVertexArrays(1, &geometry->mVao);
	glBindVertexArray(geometry->mVao);

	glBindBuffer(GL_ARRAY_BUFFER, geometry->mPosVbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, geometry->mUvVbo);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, geometry->mNormalVbo);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, geometry->mTangentVbo);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	//5.4 加入ebo到当前的vao
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->mEbo);

	glBindVertexArray(0);

	return geometry;
}

Geometry* Geometry::createSphere(float radius, int latitudeSegments, int longitudeSegments) {
	Geometry* geometry = new Geometry();
	geometry->mMinBounds = glm::vec3(-radius);
	geometry->mMaxBounds = glm::vec3(radius);

	//目标：1 位置 2 uv 3 索引
	//1 主要变量声明
	std::vector<GLfloat> positions{};
	std::vector<GLfloat> uvs{};
	std::vector<GLfloat> normals{};
	std::vector<GLfloat> tangents{};

	std::vector<GLuint> indices{};

	//声明纬线与经线的数量
	int numLatLines = std::max(3, latitudeSegments);
	int numLongLines = std::max(3, longitudeSegments);

	//2 通过两层循环（纬线在外，经线在内）->位置、uv、法线
	for (int i = 0; i <= numLatLines; i++) {
		for (int j = 0; j <= numLongLines; j++) {
			float phi = i * glm::pi<float>() / numLatLines;
			float theta = j * 2 * glm::pi<float>() / numLongLines;

			float y = radius * cos(phi);
			float x = radius * sin(phi) * cos(theta);
			float z = radius * sin(phi) * sin(theta);

			positions.push_back(x);
			positions.push_back(y);
			positions.push_back(z);

			float u = 1.0f - static_cast<float>(j) / static_cast<float>(numLongLines);
			float v = 1.0f - static_cast<float>(i) / static_cast<float>(numLatLines);

			uvs.push_back(u);
			uvs.push_back(v);

			//注意：法线方向没有问题，法线的长度不为1
			normals.push_back(x);
			normals.push_back(y);
			normals.push_back(z);
		}
	}


	//3 通过两层循环（这里没有=号）->顶点索引
	for (int i = 0; i < numLatLines; i++) {
		for (int j = 0; j < numLongLines; j++) {
			int p1 = i * (numLongLines + 1) + j;
			int p2 = p1 + numLongLines + 1;
			int p3 = p1 + 1;
			int p4 = p2 + 1;

			indices.push_back(p1);
			indices.push_back(p2);
			indices.push_back(p3);

			indices.push_back(p3);
			indices.push_back(p2);
			indices.push_back(p4);
		}
	}

	
	//********开始计算切线********
	tangents.resize(positions.size());
	//以三角形为单位进行indices的遍历
	for (std::size_t i = 0; i < indices.size(); i += 3) {
		//1 取出当前三角形的三个顶点的索引
		int idx0 = indices[i];
		int idx1 = indices[i + 1];
		int idx2 = indices[i + 2];

		//2 根据三个顶点的索引，从positions数组中找到三个顶点的位置信息
		auto p0 = glm::vec3(positions[idx0 * 3], positions[idx0 * 3+1], positions[idx0 * 3+2]);
		auto p1 = glm::vec3(positions[idx1 * 3], positions[idx1 * 3+1], positions[idx1 * 3+2]);
		auto p2 = glm::vec3(positions[idx2 * 3], positions[idx2 * 3+1], positions[idx2 * 3+2]);

		if (p0 == p1 || p1 == p2 || p2 == p0) {
			continue;
		}

		//3 根据三个顶点的索引，从uvs数组中找到三个顶点的uv信息
		auto uv0 = glm::vec2(uvs[idx0 * 2], uvs[idx0 * 2+1]);
		auto uv1 = glm::vec2(uvs[idx1 * 2], uvs[idx1 * 2+1]);
		auto uv2 = glm::vec2(uvs[idx2 * 2], uvs[idx2 * 2+1]);

		//4 根据公式，计算当前三角形的tangent
		glm::vec3 e0 = p1 - p0;
		glm::vec3 e1 = p2 - p1;

		glm::vec2 duv0 = uv1 - uv0;
		glm::vec2 duv1 = uv2 - uv1;

		float f = 1.0f / (duv0.x * duv1.y - duv1.x * duv0.y);

		glm::vec3 tangent;
		tangent.x = f * (duv1.y * e0.x - duv0.y * e1.x);
		tangent.y = f * (duv1.y * e0.y - duv0.y * e1.y);
		tangent.z = f * (duv1.y * e0.z - duv0.y * e1.z);
		tangent = glm::normalize(tangent);

		//5 针对本三角形的三个顶点的normal，使tangent正交化(三个不同的tangent）
		auto normal0 = glm::normalize(glm::vec3(normals[idx0 * 3], normals[idx0 * 3 + 1], normals[idx0 * 3 + 2]));
		auto normal1 = glm::normalize(glm::vec3(normals[idx1 * 3], normals[idx1 * 3 + 1], normals[idx1 * 3 + 2]));
		auto normal2 = glm::normalize(glm::vec3(normals[idx2 * 3], normals[idx2 * 3 + 1], normals[idx2 * 3 + 2]));

		auto tangent0 = tangent - glm::dot(tangent, normal0) * normal0;
		auto tangent1 = tangent - glm::dot(tangent, normal1) * normal1;
		auto tangent2 = tangent - glm::dot(tangent, normal2) * normal2;

		//6 累加到每个顶点的tangent属性上
		tangents[idx0 * 3] += tangent0.x;
		tangents[idx0 * 3 + 1] += tangent0.y;
		tangents[idx0 * 3 + 2] += tangent0.z;

		tangents[idx1 * 3] += tangent1.x;
		tangents[idx1 * 3 + 1] += tangent1.y;
		tangents[idx1 * 3 + 2] += tangent1.z;

		tangents[idx2 * 3] += tangent2.x;
		tangents[idx2 * 3 + 1] += tangent2.y;
		tangents[idx2 * 3 + 2] += tangent2.z;
	}

	//7 对每个顶点的最终tangent（累加值）进行normalize
	for (int i = 0; i < tangents.size(); i += 3) {
		glm::vec3 tangent = glm::vec3(tangents[i], tangents[i + 1], tangents[i + 2]);
		tangent = glm::normalize(tangent);
		tangents[i] = tangent.x;
		tangents[i + 1] = tangent.y;
		tangents[i + 2] = tangent.z;
	}




	//4 生成vbo与vao
	GLuint& posVbo = geometry->mPosVbo, uvVbo = geometry->mUvVbo, normalVbo = geometry->mNormalVbo;
	glGenBuffers(1, &posVbo);
	glBindBuffer(GL_ARRAY_BUFFER, posVbo);
	glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &uvVbo);
	glBindBuffer(GL_ARRAY_BUFFER, uvVbo);
	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(float), uvs.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &normalVbo);
	glBindBuffer(GL_ARRAY_BUFFER, normalVbo);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &geometry->mTangentVbo);
	glBindBuffer(GL_ARRAY_BUFFER, geometry->mTangentVbo);
	glBufferData(GL_ARRAY_BUFFER, tangents.size() * sizeof(float), tangents.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &geometry->mEbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->mEbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

	glGenVertexArrays(1, &geometry->mVao);
	glBindVertexArray(geometry->mVao);

	glBindBuffer(GL_ARRAY_BUFFER, posVbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, uvVbo);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, normalVbo);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, geometry->mTangentVbo);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->mEbo);

	glBindVertexArray(0);

	geometry->mIndicesCount = static_cast<uint32_t>(indices.size());
	geometry->mEstimatedGpuBytes =
		(positions.size() + uvs.size() + normals.size() + tangents.size()) * sizeof(float) +
		indices.size() * sizeof(GLuint);


	return geometry;
}


Geometry* Geometry::createRock(
	float radius,
	int latitudeSegments,
	int longitudeSegments,
	float irregularity
) {
	const int latitudeCount = std::max(3, latitudeSegments);
	const int longitudeCount = std::max(3, longitudeSegments);
	const float distortion = std::clamp(irregularity, 0.0f, 0.45f);
	std::vector<float> positions;
	std::vector<float> normals;
	std::vector<float> uvs;
	std::vector<float> tangents;
	std::vector<unsigned int> indices;
	positions.reserve((latitudeCount + 1) * (longitudeCount + 1) * 3);
	normals.reserve(positions.capacity());
	tangents.reserve(positions.capacity());
	uvs.reserve((latitudeCount + 1) * (longitudeCount + 1) * 2);

	for (int latitude = 0; latitude <= latitudeCount; ++latitude) {
		const float phi = static_cast<float>(latitude) * glm::pi<float>() /
			static_cast<float>(latitudeCount);
		for (int longitude = 0; longitude <= longitudeCount; ++longitude) {
			const float theta = static_cast<float>(longitude) * glm::two_pi<float>() /
				static_cast<float>(longitudeCount);
			const float variation = 1.0f + distortion * (
				0.55f * std::sin(theta * 3.0f + phi * 2.0f) +
				0.30f * std::sin(theta * 7.0f - phi * 5.0f) +
				0.15f * std::cos(theta * 11.0f + phi * 3.0f)
			);
			const float sinPhi = std::sin(phi);
			const glm::vec3 position(
				radius * variation * sinPhi * std::cos(theta),
				radius * variation * std::cos(phi) * 0.72f,
				radius * variation * sinPhi * std::sin(theta)
			);
			const glm::vec3 normal = glm::normalize(glm::vec3(
				position.x,
				position.y / (0.72f * 0.72f),
				position.z
			));
			const glm::vec3 tangent = glm::normalize(glm::vec3(
				-std::sin(theta),
				0.0f,
				std::cos(theta)
			));
			positions.insert(positions.end(), { position.x, position.y, position.z });
			normals.insert(normals.end(), { normal.x, normal.y, normal.z });
			tangents.insert(tangents.end(), { tangent.x, tangent.y, tangent.z });
			uvs.push_back(1.0f - static_cast<float>(longitude) /
				static_cast<float>(longitudeCount));
			uvs.push_back(1.0f - static_cast<float>(latitude) /
				static_cast<float>(latitudeCount));
		}
	}

	for (int latitude = 0; latitude < latitudeCount; ++latitude) {
		for (int longitude = 0; longitude < longitudeCount; ++longitude) {
			const unsigned int first =
				latitude * (longitudeCount + 1) + longitude;
			const unsigned int second = first + longitudeCount + 1;
			indices.insert(indices.end(), {
				first,
				second,
				first + 1,
				first + 1,
				second,
				second + 1
			});
		}
	}

	return new Geometry(positions, normals, uvs, indices, tangents);
}


Geometry* Geometry::createPlane(float width, float height) {
	Geometry* geometry = new Geometry();
	geometry->mIndicesCount = 6;

	float halfW = width / 2.0f;
	float halfH = height / 2.0f;
	geometry->mMinBounds = glm::vec3(-halfW, -halfH, 0.0f);
	geometry->mMaxBounds = glm::vec3(halfW, halfH, 0.0f);

	float positions[] = {
		-halfW, -halfH, 0.0f,
		halfW, -halfH, 0.0f,
		halfW, halfH, 0.0f,
		-halfW, halfH, 0.0f,
	};

	float uvs[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

	float normals[] = {
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
	};

	float tangents[] = {
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
	};

	unsigned int indices[] = {
		0, 1, 2,
		2, 3, 0
	};
	geometry->mEstimatedGpuBytes =
		sizeof(positions) + sizeof(uvs) + sizeof(normals) + sizeof(tangents) + sizeof(indices);

	//2 VBO创建
	glGenBuffers(1, &geometry->mPosVbo);
	glBindBuffer(GL_ARRAY_BUFFER, geometry->mPosVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);

	glGenBuffers(1, &geometry->mUvVbo);
	glBindBuffer(GL_ARRAY_BUFFER, geometry->mUvVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);

	glGenBuffers(1, &geometry->mNormalVbo);
	glBindBuffer(GL_ARRAY_BUFFER, geometry->mNormalVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(normals), normals, GL_STATIC_DRAW);

	glGenBuffers(1, &geometry->mTangentVbo);
	glBindBuffer(GL_ARRAY_BUFFER, geometry->mTangentVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(tangents), tangents, GL_STATIC_DRAW);

	//3 EBO创建
	glGenBuffers(1, &geometry->mEbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->mEbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	//4 VAO创建
	glGenVertexArrays(1, &geometry->mVao);
	glBindVertexArray(geometry->mVao);

	glBindBuffer(GL_ARRAY_BUFFER, geometry->mPosVbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, geometry->mUvVbo);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, geometry->mNormalVbo);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, geometry->mTangentVbo);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	//5.4 加入ebo到当前的vao
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->mEbo);

	glBindVertexArray(0);

	return geometry;
}

Geometry* Geometry::createScreenPlane() {
	Geometry* geometry = new Geometry();
	geometry->mIndicesCount = 6;
	geometry->mMinBounds = glm::vec3(-1.0f, -1.0f, 0.0f);
	geometry->mMaxBounds = glm::vec3(1.0f, 1.0f, 0.0f);

	// 构建数据positions uv
	float positions[] = {
		-1.0f,  1.0f,
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,
	};

	float uvs[] = {
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};

	unsigned int indices[] = {
		0, 1, 2,
		0, 2, 3
	};
	geometry->mEstimatedGpuBytes = sizeof(positions) + sizeof(uvs) + sizeof(indices);

	//创建vao vbo等
	GLuint& posVbo = geometry->mPosVbo, uvVbo = geometry->mUvVbo;
	glGenBuffers(1, &posVbo);
	glBindBuffer(GL_ARRAY_BUFFER, posVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);

	glGenBuffers(1, &uvVbo);
	glBindBuffer(GL_ARRAY_BUFFER, uvVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);

	glGenBuffers(1, &geometry->mEbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->mEbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glGenVertexArrays(1, &geometry->mVao);
	glBindVertexArray(geometry->mVao);

	glBindBuffer(GL_ARRAY_BUFFER, posVbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, uvVbo);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->mEbo);
	glBindVertexArray(0);

	return geometry;
}
