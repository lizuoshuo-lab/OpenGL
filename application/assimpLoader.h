#pragma once
#include "../glframework/core.h"
#include "../glframework/object.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "../glframework/mesh/mesh.h"

#include "../glframework/texture.h"
#include "../glframework/material/advanced/pbrMaterial.h"

class SkinnedModel;

class AssimpLoader {
public:
	static Object* load(const std::string& path);
	static Object* load(
		const std::string& path,
		PbrMaterial* materialTemplate,
		float targetExtent,
		std::vector<PbrMaterial*>* loadedMaterials = nullptr
	);
	static SkinnedModel* loadSkinned(
		const std::string& path,
		PbrMaterial* materialTemplate,
		float targetExtent,
		std::vector<PbrMaterial*>* loadedMaterials = nullptr
	);

private:
	static void processNode(aiNode* ainode, 
		Object* parent, 
		const aiScene* scene,
		const std::string& rootPath,
		PbrMaterial* materialTemplate,
		std::vector<PbrMaterial*>* materialCache
	);

	static Mesh* processMesh(
		aiMesh* aimesh, 
		const aiScene* scene,
		const std::string& rootPath,
		PbrMaterial* materialTemplate,
		std::vector<PbrMaterial*>* materialCache
	);

	static PbrMaterial* createPbrMaterial(
		const aiMaterial* material,
		const aiScene* scene,
		const std::string& rootPath,
		const PbrMaterial* materialTemplate
	);

	static void calculateBounds(
		aiNode* node,
		const aiScene* scene,
		const aiMatrix4x4& parentTransform,
		aiVector3D& minBounds,
		aiVector3D& maxBounds
	);

	static Texture* processTexture(
		const aiMaterial* aimat,
		const aiTextureType& type,
		const aiScene* scene,
		const std::string& rootPath,
		unsigned int internalFormat = GL_SRGB_ALPHA
	);

	static glm::mat4 getMat4f(aiMatrix4x4 value);
};
