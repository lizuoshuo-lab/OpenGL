#pragma once 
#include "mesh.h"

class InstancedMesh :public Mesh {
public:
	InstancedMesh(Geometry* geometry, Material* material, unsigned int instanceCount);
	~InstancedMesh();

	void updateMatrices();
	void setMatrices(const std::vector<glm::mat4>& matrices);
	void sortMatrices(glm::mat4 viewMatrix);
	void setLogicalInstanceTotal(unsigned int count) { mLogicalInstanceTotal = count; }
	unsigned int getLogicalInstanceTotal() const { return mLogicalInstanceTotal; }
	void setLogicalVisibleCount(unsigned int count) { mLogicalVisibleCount = count; }
	unsigned int getLogicalVisibleCount() const { return mLogicalVisibleCount; }

public:
	unsigned int				mInstanceCount{ 0 };
	std::vector<glm::mat4>		mInstanceMatrices{};
	unsigned int				mMatrixVbo{ 0 };

private:
	unsigned int mInstanceCapacity{ 0 };
	unsigned int mLogicalInstanceTotal{ 0 };
	unsigned int mLogicalVisibleCount{ 0 };
};
