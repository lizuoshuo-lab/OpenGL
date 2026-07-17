#include "object.h"

Object::Object() {
	mType = ObjectType::Object;
}

Object::~Object() {

}

void Object::setPosition(glm::vec3 pos) {
	mPosition = pos;
	mUseLocalMatrix = false;
}

void Object::setLocalMatrix(const glm::mat4& matrix) {
	mLocalMatrix = matrix;
	mUseLocalMatrix = true;
}

void Object::rotateX(float angle) {
	mAngleX += angle;
	mUseLocalMatrix = false;
}

void Object::rotateY(float angle) {
	mAngleY += angle;
	mUseLocalMatrix = false;
}

void Object::rotateZ(float angle) {
	mAngleZ += angle;
	mUseLocalMatrix = false;
}

void Object::setAngleX(float angle) {
	mAngleX = angle;
	mUseLocalMatrix = false;
}

void Object::setAngleY(float angle) {
	mAngleY = angle;
	mUseLocalMatrix = false;
}

void Object::setAngleZ(float angle) {
	mAngleZ = angle;
	mUseLocalMatrix = false;
}

void Object::setScale(glm::vec3 scale) {
	mScale = scale;
	mUseLocalMatrix = false;
}

glm::mat4 Object::getModelMatrix() const {
	glm::mat4 parentMatrix{ 1.0f };
	if (mParent != nullptr) {
		parentMatrix = mParent->getModelMatrix();
	}

	if (mUseLocalMatrix) {
		return parentMatrix * mLocalMatrix;
	}

	glm::mat4 localMatrix = glm::translate(glm::mat4(1.0f), mPosition);
	localMatrix = glm::rotate(localMatrix, glm::radians(mAngleX), glm::vec3(1.0f, 0.0f, 0.0f));
	localMatrix = glm::rotate(localMatrix, glm::radians(mAngleY), glm::vec3(0.0f, 1.0f, 0.0f));
	localMatrix = glm::rotate(localMatrix, glm::radians(mAngleZ), glm::vec3(0.0f, 0.0f, 1.0f));
	localMatrix = glm::scale(localMatrix, mScale);
	return parentMatrix * localMatrix;
}

glm::vec3 Object::getDirection()const {
	auto modelMatrix = glm::mat3(getModelMatrix());
	auto dir = glm::normalize(-modelMatrix[2]);

	return dir;
}


void Object::addChild(Object* obj) {
	auto iter = std::find(mChildren.begin(), mChildren.end(), obj);
	if (iter != mChildren.end()) {
		std::cerr << "Duplicated Child added" << std::endl;
		return;
	}

	mChildren.push_back(obj);
	obj->mParent = this;
}

std::vector<Object*>  Object::getChildren() {
	return mChildren;
}

Object* Object::getParent() {
	return mParent;
}
