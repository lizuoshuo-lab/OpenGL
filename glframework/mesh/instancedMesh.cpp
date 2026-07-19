#include "instancedMesh.h"
#include<algorithm>

InstancedMesh::InstancedMesh(
	Geometry* geometry, 
	Material* material, 
	unsigned int instanceCount
):Mesh(geometry, material) {
	mType = ObjectType::InstancedMesh;
	mInstanceCount = instanceCount;
	mInstanceCapacity = std::max(1u, instanceCount);
	mLogicalInstanceTotal = instanceCount;
	mLogicalVisibleCount = instanceCount;
	mInstanceMatrices.resize(instanceCount);
	mInstanceLodFades.resize(instanceCount, 1.0f);

	glGenBuffers(1, &mMatrixVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * mInstanceCapacity, nullptr, GL_DYNAMIC_DRAW);
	if (mInstanceCount > 0) {
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::mat4) * mInstanceCount, mInstanceMatrices.data());
	}
	glGenBuffers(1, &mFadeVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mFadeVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mInstanceCapacity, nullptr, GL_DYNAMIC_DRAW);
	if (mInstanceCount > 0) {
		glBufferSubData(
			GL_ARRAY_BUFFER,
			0,
			sizeof(float) * mInstanceCount,
			mInstanceLodFades.data()
		);
	}
	bindInstanceAttributes();
	glBindVertexArray(0);
}

InstancedMesh::~InstancedMesh() {
	if (mMatrixVbo != 0) {
		glDeleteBuffers(1, &mMatrixVbo);
		mMatrixVbo = 0;
	}
	if (mFadeVbo != 0) {
		glDeleteBuffers(1, &mFadeVbo);
		mFadeVbo = 0;
	}
}

void InstancedMesh::updateMatrices() {
	glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
	if (mInstanceCount > 0) {
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::mat4) * mInstanceCount, mInstanceMatrices.data());
	}
	glBindBuffer(GL_ARRAY_BUFFER, mFadeVbo);
	if (mInstanceCount > 0) {
		glBufferSubData(
			GL_ARRAY_BUFFER,
			0,
			sizeof(float) * mInstanceCount,
			mInstanceLodFades.data()
		);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancedMesh::setMatrices(const std::vector<glm::mat4>& matrices) {
	setInstances(matrices, std::vector<float>(matrices.size(), 1.0f));
}

void InstancedMesh::setInstances(
	const std::vector<glm::mat4>& matrices,
	const std::vector<float>& lodFades
) {
	mInstanceMatrices = matrices;
	mInstanceLodFades = lodFades.size() == matrices.size()
		? lodFades
		: std::vector<float>(matrices.size(), 1.0f);
	mInstanceCount = static_cast<unsigned int>(mInstanceMatrices.size());
	mLogicalVisibleCount = mInstanceCount;

	if (mInstanceCount > mInstanceCapacity) {
		mInstanceCapacity = std::max(1u, mInstanceCount);
		glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
		glBufferData(
			GL_ARRAY_BUFFER,
			sizeof(glm::mat4) * mInstanceCapacity,
			mInstanceMatrices.data(),
			GL_DYNAMIC_DRAW
		);
		glBindBuffer(GL_ARRAY_BUFFER, mFadeVbo);
		glBufferData(
			GL_ARRAY_BUFFER,
			sizeof(float) * mInstanceCapacity,
			mInstanceLodFades.data(),
			GL_DYNAMIC_DRAW
		);
	}
	else if (mInstanceCount > 0) {
		glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
		glBufferSubData(
			GL_ARRAY_BUFFER,
			0,
			sizeof(glm::mat4) * mInstanceCount,
			mInstanceMatrices.data()
		);
		glBindBuffer(GL_ARRAY_BUFFER, mFadeVbo);
		glBufferSubData(
			GL_ARRAY_BUFFER,
			0,
			sizeof(float) * mInstanceCount,
			mInstanceLodFades.data()
		);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancedMesh::bindInstanceAttributes() const {
	glBindVertexArray(mGeometry->getVao());
	glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
	for (int column = 0; column < 4; ++column) {
		glEnableVertexAttribArray(4 + column);
		glVertexAttribPointer(
			4 + column,
			4,
			GL_FLOAT,
			GL_FALSE,
			sizeof(glm::mat4),
			reinterpret_cast<void*>(sizeof(float) * column * 4)
		);
		glVertexAttribDivisor(4 + column, 1);
	}
	glBindBuffer(GL_ARRAY_BUFFER, mFadeVbo);
	glEnableVertexAttribArray(10);
	glVertexAttribPointer(10, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr);
	glVertexAttribDivisor(10, 1);
}

void InstancedMesh::sortMatrices(glm::mat4 viewMatrix) {
	std::vector<std::size_t> order(mInstanceMatrices.size());
	for (std::size_t index = 0; index < order.size(); ++index) {
		order[index] = index;
	}
	std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
		const glm::vec4 leftPosition = viewMatrix * mInstanceMatrices[left] *
			glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		const glm::vec4 rightPosition = viewMatrix * mInstanceMatrices[right] *
			glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		return leftPosition.z < rightPosition.z;
	});
	std::vector<glm::mat4> sortedMatrices;
	std::vector<float> sortedFades;
	sortedMatrices.reserve(order.size());
	sortedFades.reserve(order.size());
	for (std::size_t index : order) {
		sortedMatrices.push_back(mInstanceMatrices[index]);
		sortedFades.push_back(mInstanceLodFades[index]);
	}
	mInstanceMatrices = std::move(sortedMatrices);
	mInstanceLodFades = std::move(sortedFades);
}
