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

	glGenBuffers(1, &mMatrixVbo);
	glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * mInstanceCapacity, nullptr, GL_DYNAMIC_DRAW);
	if (mInstanceCount > 0) {
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::mat4) * mInstanceCount, mInstanceMatrices.data());
	}

	glBindVertexArray(mGeometry->getVao());
	glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);

	for (int i = 0; i < 4; i++) {
		glEnableVertexAttribArray(4 + i);
		glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(float) * i * 4));
		glVertexAttribDivisor(4 + i, 1);//逐实例更新
	}
	glBindVertexArray(0);
}

InstancedMesh::~InstancedMesh() {
	if (mMatrixVbo != 0) {
		glDeleteBuffers(1, &mMatrixVbo);
		mMatrixVbo = 0;
	}
}

void InstancedMesh::updateMatrices() {
	glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
	if (mInstanceCount > 0) {
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::mat4) * mInstanceCount, mInstanceMatrices.data());
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancedMesh::setMatrices(const std::vector<glm::mat4>& matrices) {
	mInstanceMatrices = matrices;
	mInstanceCount = static_cast<unsigned int>(mInstanceMatrices.size());
	mLogicalVisibleCount = mInstanceCount;

	glBindBuffer(GL_ARRAY_BUFFER, mMatrixVbo);
	if (mInstanceCount > mInstanceCapacity) {
		mInstanceCapacity = std::max(1u, mInstanceCount);
		glBufferData(
			GL_ARRAY_BUFFER,
			sizeof(glm::mat4) * mInstanceCapacity,
			mInstanceMatrices.data(),
			GL_DYNAMIC_DRAW
		);
	}
	else if (mInstanceCount > 0) {
		glBufferSubData(
			GL_ARRAY_BUFFER,
			0,
			sizeof(glm::mat4) * mInstanceCount,
			mInstanceMatrices.data()
		);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void InstancedMesh::sortMatrices(glm::mat4 viewMatrix) {
	std::sort(
		mInstanceMatrices.begin(),
		mInstanceMatrices.end(),
		[viewMatrix](const glm::mat4& a, const glm::mat4& b) {

			//1 计算a的相机系的Z
			auto modelMatrixA = a;
			auto worldPositionA = modelMatrixA * glm::vec4(0.0, 0.0, 0.0, 1.0);
			auto cameraPositionA = viewMatrix * worldPositionA;

			//2 计算b的相机系的Z
			auto modelMatrixB = b;
			auto worldPositionB = modelMatrixB * glm::vec4(0.0, 0.0, 0.0, 1.0);
			auto cameraPositionB = viewMatrix * worldPositionB;

			return cameraPositionA.z < cameraPositionB.z;
		}
	);
}
