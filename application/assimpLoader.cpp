#include "assimpLoader.h"
#include "skinnedModel.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "../glframework/material/phongMaterial.h"
#include "assimp/GltfMaterial.h"

namespace {
constexpr unsigned int kImportFlags =
	aiProcess_Triangulate |
	aiProcess_GenSmoothNormals |
	aiProcess_CalcTangentSpace |
	aiProcess_JoinIdenticalVertices |
	aiProcess_ImproveCacheLocality;
}

Object* AssimpLoader::load(const std::string& path) {
	return load(path, nullptr, 0.0f, nullptr);
}

Object* AssimpLoader::load(
	const std::string& path,
	PbrMaterial* materialTemplate,
	float targetExtent,
	std::vector<PbrMaterial*>* loadedMaterials
) {
	const std::size_t lastIndex = path.find_last_of("/\\");
	const std::string rootPath = lastIndex == std::string::npos
		? std::string()
		: path.substr(0, lastIndex + 1);

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, kImportFlags);
	if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
		std::cerr << "Failed to load model '" << path << "': "
			<< importer.GetErrorString() << std::endl;
		return nullptr;
	}

	Object* rootNode = new Object();
	std::vector<PbrMaterial*> materialCache(scene->mNumMaterials, nullptr);
	processNode(
		scene->mRootNode,
		rootNode,
		scene,
		rootPath,
		materialTemplate,
		&materialCache
	);
	if (loadedMaterials != nullptr) {
		for (PbrMaterial* material : materialCache) {
			if (material != nullptr) {
				loadedMaterials->push_back(material);
			}
		}
	}

	if (targetExtent > 0.0f) {
		const float maxValue = std::numeric_limits<float>::max();
		aiVector3D minBounds(maxValue, maxValue, maxValue);
		aiVector3D maxBounds(-maxValue, -maxValue, -maxValue);
		calculateBounds(
			scene->mRootNode,
			scene,
			aiMatrix4x4(),
			minBounds,
			maxBounds
		);

		const aiVector3D size = maxBounds - minBounds;
		const float maxExtent = std::max(size.x, std::max(size.y, size.z));
		if (maxExtent > std::numeric_limits<float>::epsilon()) {
			const float normalizedScale = targetExtent / maxExtent;
			const aiVector3D center = (minBounds + maxBounds) * 0.5f;
			rootNode->setScale(glm::vec3(normalizedScale));
			rootNode->setPosition(glm::vec3(
				-center.x * normalizedScale,
				-center.y * normalizedScale,
				-center.z * normalizedScale
			));
		}
	}

	return rootNode;
}

SkinnedModel* AssimpLoader::loadSkinned(
	const std::string& path,
	PbrMaterial* materialTemplate,
	float targetExtent,
	std::vector<PbrMaterial*>* loadedMaterials
) {
	if (materialTemplate == nullptr) {
		std::cerr << "A PBR material template is required for skinned models" << std::endl;
		return nullptr;
	}

	const std::size_t lastIndex = path.find_last_of("/\\");
	const std::string rootPath = lastIndex == std::string::npos
		? std::string()
		: path.substr(0, lastIndex + 1);

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, kImportFlags);
	if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
		std::cerr << "Failed to load skinned model '" << path << "': "
			<< importer.GetErrorString() << std::endl;
		return nullptr;
	}

	SkinnedModel* model = new SkinnedModel();
	model->mRoot = new Object();
	model->mGlobalInverseRoot = glm::inverse(getMat4f(scene->mRootNode->mTransformation));

	std::unordered_map<std::string, int> nodeLookup;
	std::vector<int> meshNodeIndices(scene->mNumMeshes, -1);
	std::function<void(aiNode*, int)> copyNode = [&](aiNode* source, int parentIndex) {
		SkinnedModel::Node node;
		node.name = source->mName.C_Str();
		node.parent = parentIndex;
		node.bindLocal = getMat4f(source->mTransformation);

		aiVector3D scale;
		aiQuaternion rotation;
		aiVector3D translation;
		source->mTransformation.Decompose(scale, rotation, translation);
		node.bindTranslation = glm::vec3(translation.x, translation.y, translation.z);
		node.bindRotation = glm::normalize(glm::quat(
			rotation.w,
			rotation.x,
			rotation.y,
			rotation.z
		));
		node.bindScale = glm::vec3(scale.x, scale.y, scale.z);
		node.global = parentIndex >= 0
			? model->mNodes[parentIndex].global * node.bindLocal
			: node.bindLocal;

		const int nodeIndex = static_cast<int>(model->mNodes.size());
		model->mNodes.push_back(node);
		nodeLookup[node.name] = nodeIndex;
		for (unsigned int i = 0; i < source->mNumMeshes; ++i) {
			const unsigned int meshIndex = source->mMeshes[i];
			if (meshIndex < meshNodeIndices.size()) {
				meshNodeIndices[meshIndex] = nodeIndex;
			}
		}

		for (unsigned int i = 0; i < source->mNumChildren; ++i) {
			copyNode(source->mChildren[i], nodeIndex);
		}
	};
	copyNode(scene->mRootNode, -1);

	std::vector<PbrMaterial*> materialCache(scene->mNumMaterials, nullptr);
	std::unordered_set<int> uniqueBoneNodes;
	for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* sourceMesh = scene->mMeshes[meshIndex];
		std::vector<float> positions;
		std::vector<float> normals;
		std::vector<float> uvs;
		std::vector<float> tangents;
		std::vector<unsigned int> indices;
		positions.reserve(sourceMesh->mNumVertices * 3);
		normals.reserve(sourceMesh->mNumVertices * 3);
		uvs.reserve(sourceMesh->mNumVertices * 2);
		tangents.reserve(sourceMesh->mNumVertices * 3);

		for (unsigned int vertexIndex = 0;
			vertexIndex < sourceMesh->mNumVertices;
			++vertexIndex) {
			const aiVector3D& position = sourceMesh->mVertices[vertexIndex];
			const aiVector3D& normal = sourceMesh->mNormals[vertexIndex];
			positions.insert(positions.end(), { position.x, position.y, position.z });
			normals.insert(normals.end(), { normal.x, normal.y, normal.z });

			if (sourceMesh->mTextureCoords[0] != nullptr) {
				uvs.insert(uvs.end(), {
					sourceMesh->mTextureCoords[0][vertexIndex].x,
					sourceMesh->mTextureCoords[0][vertexIndex].y
				});
			}
			else {
				uvs.insert(uvs.end(), { 0.0f, 0.0f });
			}

			if (sourceMesh->HasTangentsAndBitangents()) {
				const aiVector3D& tangent = sourceMesh->mTangents[vertexIndex];
				tangents.insert(tangents.end(), { tangent.x, tangent.y, tangent.z });
			}
			else {
				const glm::vec3 n = glm::normalize(glm::vec3(
					normal.x,
					normal.y,
					normal.z
				));
				const glm::vec3 reference = std::abs(n.y) < 0.999f
					? glm::vec3(0.0f, 1.0f, 0.0f)
					: glm::vec3(1.0f, 0.0f, 0.0f);
				const glm::vec3 tangent = glm::normalize(glm::cross(reference, n));
				tangents.insert(tangents.end(), { tangent.x, tangent.y, tangent.z });
			}
		}

		for (unsigned int faceIndex = 0; faceIndex < sourceMesh->mNumFaces; ++faceIndex) {
			const aiFace& face = sourceMesh->mFaces[faceIndex];
			for (unsigned int index = 0; index < face.mNumIndices; ++index) {
				indices.push_back(face.mIndices[index]);
			}
		}

		Geometry* geometry = new Geometry(positions, normals, uvs, indices, tangents);
		PbrMaterial* material = nullptr;
		if (sourceMesh->mMaterialIndex < materialCache.size()) {
			PbrMaterial*& cachedMaterial = materialCache[sourceMesh->mMaterialIndex];
			if (cachedMaterial == nullptr) {
				cachedMaterial = createPbrMaterial(
					scene->mMaterials[sourceMesh->mMaterialIndex],
					scene,
					rootPath,
					materialTemplate
				);
			}
			material = cachedMaterial;
		}
		if (material == nullptr) {
			material = materialTemplate;
		}

		Mesh* mesh = new Mesh(geometry, material);
		model->mRoot->addChild(mesh);

		SkinnedModel::SkinBinding binding;
		binding.mesh = mesh;
		std::vector<std::vector<std::pair<int, float>>> influences(
			sourceMesh->mNumVertices
		);
		for (unsigned int boneIndex = 0; boneIndex < sourceMesh->mNumBones; ++boneIndex) {
			if (binding.boneNodes.size() >= Mesh::kMaxBoneMatrices) {
				std::cerr << "Mesh '" << sourceMesh->mName.C_Str()
					<< "' exceeds the " << Mesh::kMaxBoneMatrices
					<< " bone GPU palette limit" << std::endl;
				break;
			}

			const aiBone* sourceBone = sourceMesh->mBones[boneIndex];
			const auto nodeIterator = nodeLookup.find(sourceBone->mName.C_Str());
			if (nodeIterator == nodeLookup.end()) {
				std::cerr << "Missing skeleton node for bone '"
					<< sourceBone->mName.C_Str() << "'" << std::endl;
				continue;
			}

			const int localBoneIndex = static_cast<int>(binding.boneNodes.size());
			binding.boneNodes.push_back(nodeIterator->second);
			binding.boneOffsets.push_back(getMat4f(sourceBone->mOffsetMatrix));
			uniqueBoneNodes.insert(nodeIterator->second);
			for (unsigned int weightIndex = 0;
				weightIndex < sourceBone->mNumWeights;
				++weightIndex) {
				const aiVertexWeight& weight = sourceBone->mWeights[weightIndex];
				if (weight.mVertexId < influences.size() && weight.mWeight > 0.0f) {
					influences[weight.mVertexId].push_back({
						localBoneIndex,
						weight.mWeight
					});
				}
			}
		}

		if (!binding.boneNodes.empty()) {
			std::vector<glm::ivec4> boneIds(sourceMesh->mNumVertices, glm::ivec4(0));
			std::vector<glm::vec4> boneWeights(sourceMesh->mNumVertices, glm::vec4(0.0f));
			for (std::size_t vertexIndex = 0; vertexIndex < influences.size(); ++vertexIndex) {
				auto& vertexInfluences = influences[vertexIndex];
				std::sort(
					vertexInfluences.begin(),
					vertexInfluences.end(),
					[](const auto& left, const auto& right) {
						return left.second > right.second;
					}
				);
				const std::size_t influenceCount = std::min<std::size_t>(
					4,
					vertexInfluences.size()
				);
				float totalWeight = 0.0f;
				for (std::size_t influenceIndex = 0;
					influenceIndex < influenceCount;
					++influenceIndex) {
					boneIds[vertexIndex][static_cast<glm::length_t>(influenceIndex)] =
						vertexInfluences[influenceIndex].first;
					boneWeights[vertexIndex][static_cast<glm::length_t>(influenceIndex)] =
						vertexInfluences[influenceIndex].second;
					totalWeight += vertexInfluences[influenceIndex].second;
				}
				if (totalWeight > std::numeric_limits<float>::epsilon()) {
					boneWeights[vertexIndex] /= totalWeight;
				}
			}

			geometry->setSkinningData(boneIds, boneWeights);
			mesh->setBoneMatrices(std::vector<glm::mat4>(
				binding.boneNodes.size(),
				glm::mat4(1.0f)
			));
			model->mSkinBindings.push_back(std::move(binding));
		}
		else if (meshIndex < meshNodeIndices.size() && meshNodeIndices[meshIndex] >= 0) {
			mesh->setLocalMatrix(
				model->mGlobalInverseRoot *
				model->mNodes[meshNodeIndices[meshIndex]].global
			);
		}
	}
	model->mBoneCount = static_cast<int>(uniqueBoneNodes.size());

	for (unsigned int animationIndex = 0;
		animationIndex < scene->mNumAnimations;
		++animationIndex) {
		const aiAnimation* sourceAnimation = scene->mAnimations[animationIndex];
		SkinnedModel::Clip clip;
		clip.name = sourceAnimation->mName.length > 0
			? sourceAnimation->mName.C_Str()
			: "Animation " + std::to_string(animationIndex + 1);
		clip.durationTicks = sourceAnimation->mDuration;
		clip.ticksPerSecond = sourceAnimation->mTicksPerSecond > 0.0
			? sourceAnimation->mTicksPerSecond
			: 25.0;
		clip.channelByNode.assign(model->mNodes.size(), -1);

		for (unsigned int channelIndex = 0;
			channelIndex < sourceAnimation->mNumChannels;
			++channelIndex) {
			const aiNodeAnim* sourceChannel = sourceAnimation->mChannels[channelIndex];
			const auto nodeIterator = nodeLookup.find(sourceChannel->mNodeName.C_Str());
			if (nodeIterator == nodeLookup.end()) {
				continue;
			}

			SkinnedModel::Channel channel;
			channel.nodeIndex = nodeIterator->second;
			channel.positions.reserve(sourceChannel->mNumPositionKeys);
			for (unsigned int keyIndex = 0;
				keyIndex < sourceChannel->mNumPositionKeys;
				++keyIndex) {
				const aiVectorKey& key = sourceChannel->mPositionKeys[keyIndex];
				channel.positions.push_back({
					key.mTime,
					glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z)
				});
				clip.durationTicks = std::max(clip.durationTicks, key.mTime);
			}

			channel.rotations.reserve(sourceChannel->mNumRotationKeys);
			for (unsigned int keyIndex = 0;
				keyIndex < sourceChannel->mNumRotationKeys;
				++keyIndex) {
				const aiQuatKey& key = sourceChannel->mRotationKeys[keyIndex];
				channel.rotations.push_back({
					key.mTime,
					glm::normalize(glm::quat(
						key.mValue.w,
						key.mValue.x,
						key.mValue.y,
						key.mValue.z
					))
				});
				clip.durationTicks = std::max(clip.durationTicks, key.mTime);
			}

			channel.scales.reserve(sourceChannel->mNumScalingKeys);
			for (unsigned int keyIndex = 0;
				keyIndex < sourceChannel->mNumScalingKeys;
				++keyIndex) {
				const aiVectorKey& key = sourceChannel->mScalingKeys[keyIndex];
				channel.scales.push_back({
					key.mTime,
					glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z)
				});
				clip.durationTicks = std::max(clip.durationTicks, key.mTime);
			}

			clip.channelByNode[channel.nodeIndex] =
				static_cast<int>(clip.channels.size());
			clip.channels.push_back(std::move(channel));
		}
		model->mClips.push_back(std::move(clip));
	}

	if (loadedMaterials != nullptr) {
		for (PbrMaterial* material : materialCache) {
			if (material != nullptr) {
				loadedMaterials->push_back(material);
			}
		}
	}

	if (targetExtent > 0.0f) {
		const float maxValue = std::numeric_limits<float>::max();
		aiVector3D sourceMinimum(maxValue, maxValue, maxValue);
		aiVector3D sourceMaximum(-maxValue, -maxValue, -maxValue);
		calculateBounds(
			scene->mRootNode,
			scene,
			aiMatrix4x4(),
			sourceMinimum,
			sourceMaximum
		);

		glm::vec3 minimum(maxValue);
		glm::vec3 maximum(-maxValue);
		for (int x = 0; x < 2; ++x) {
			for (int y = 0; y < 2; ++y) {
				for (int z = 0; z < 2; ++z) {
					const glm::vec4 sourceCorner(
						x == 0 ? sourceMinimum.x : sourceMaximum.x,
						y == 0 ? sourceMinimum.y : sourceMaximum.y,
						z == 0 ? sourceMinimum.z : sourceMaximum.z,
						1.0f
					);
					const glm::vec3 corner = glm::vec3(
						model->mGlobalInverseRoot * sourceCorner
					);
					minimum = glm::min(minimum, corner);
					maximum = glm::max(maximum, corner);
				}
			}
		}

		const glm::vec3 size = maximum - minimum;
		const float maxExtent = std::max(size.x, std::max(size.y, size.z));
		if (maxExtent > std::numeric_limits<float>::epsilon()) {
			const float normalizedScale = targetExtent / maxExtent;
			const glm::vec3 center = (minimum + maximum) * 0.5f;
			model->mRoot->setScale(glm::vec3(normalizedScale));
			model->mRoot->setPosition(-center * normalizedScale);
		}
	}

	int defaultClip = 0;
	for (int clipIndex = 0; clipIndex < model->clipCount(); ++clipIndex) {
		if (model->clipName(clipIndex).find("Walk") != std::string::npos) {
			defaultClip = clipIndex;
			break;
		}
	}
	model->setClip(defaultClip);
	std::cout << "Loaded skinned model '" << path << "': "
		<< model->nodeCount() << " nodes, "
		<< model->boneCount() << " bones, "
		<< model->clipCount() << " clips" << std::endl;
	return model;
}

void AssimpLoader::processNode(
	aiNode* aiNode,
	Object* parent,
	const aiScene* scene,
	const std::string& rootPath,
	PbrMaterial* materialTemplate,
	std::vector<PbrMaterial*>* materialCache
) {
	Object* node = new Object();
	parent->addChild(node);

	node->setLocalMatrix(getMat4f(aiNode->mTransformation));

	for (unsigned int i = 0; i < aiNode->mNumMeshes; ++i) {
		const unsigned int meshId = aiNode->mMeshes[i];
		Mesh* mesh = processMesh(
			scene->mMeshes[meshId],
			 scene,
			 rootPath,
			 materialTemplate,
			 materialCache
		);
		node->addChild(mesh);
	}

	for (unsigned int i = 0; i < aiNode->mNumChildren; ++i) {
		processNode(
			aiNode->mChildren[i],
			node,
			scene,
			rootPath,
			materialTemplate,
			materialCache
		);
	}
}

Mesh* AssimpLoader::processMesh(
	aiMesh* aiMesh,
	const aiScene* scene,
	const std::string& rootPath,
	PbrMaterial* materialTemplate,
	std::vector<PbrMaterial*>* materialCache
) {
	std::vector<float> positions;
	std::vector<float> normals;
	std::vector<float> uvs;
	std::vector<float> tangents;
	std::vector<unsigned int> indices;

	positions.reserve(aiMesh->mNumVertices * 3);
	normals.reserve(aiMesh->mNumVertices * 3);
	uvs.reserve(aiMesh->mNumVertices * 2);
	tangents.reserve(aiMesh->mNumVertices * 3);

	for (unsigned int i = 0; i < aiMesh->mNumVertices; ++i) {
		const aiVector3D& position = aiMesh->mVertices[i];
		const aiVector3D& normal = aiMesh->mNormals[i];
		positions.insert(positions.end(), { position.x, position.y, position.z });
		normals.insert(normals.end(), { normal.x, normal.y, normal.z });

		if (aiMesh->mTextureCoords[0]) {
			uvs.insert(uvs.end(), {
				aiMesh->mTextureCoords[0][i].x,
				aiMesh->mTextureCoords[0][i].y
			});
		}
		else {
			uvs.insert(uvs.end(), { 0.0f, 0.0f });
		}

		if (aiMesh->HasTangentsAndBitangents()) {
			const aiVector3D& tangent = aiMesh->mTangents[i];
			tangents.insert(tangents.end(), { tangent.x, tangent.y, tangent.z });
		}
		else {
			const glm::vec3 n = glm::normalize(glm::vec3(normal.x, normal.y, normal.z));
			const glm::vec3 reference = std::abs(n.y) < 0.999f
				? glm::vec3(0.0f, 1.0f, 0.0f)
				: glm::vec3(1.0f, 0.0f, 0.0f);
			const glm::vec3 tangent = glm::normalize(glm::cross(reference, n));
			tangents.insert(tangents.end(), { tangent.x, tangent.y, tangent.z });
		}
	}

	for (unsigned int i = 0; i < aiMesh->mNumFaces; ++i) {
		const aiFace& face = aiMesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; ++j) {
			indices.push_back(face.mIndices[j]);
		}
	}

	Geometry* geometry = materialTemplate != nullptr
		? new Geometry(positions, normals, uvs, indices, tangents)
		: new Geometry(positions, normals, uvs, indices);

	Material* material = nullptr;
	if (materialTemplate != nullptr && aiMesh->mMaterialIndex < materialCache->size()) {
		PbrMaterial*& cachedMaterial = (*materialCache)[aiMesh->mMaterialIndex];
		if (cachedMaterial == nullptr) {
			cachedMaterial = createPbrMaterial(
				scene->mMaterials[aiMesh->mMaterialIndex],
				scene,
				rootPath,
				materialTemplate
			);
		}
		material = cachedMaterial;
	}
	if (material == nullptr) {
		PhongMaterial* phongMaterial = new PhongMaterial();
		Texture* diffuse = nullptr;
		if (aiMesh->mMaterialIndex >= 0) {
			const aiMaterial* aiMaterial = scene->mMaterials[aiMesh->mMaterialIndex];
			diffuse = processTexture(
				aiMaterial,
				aiTextureType_DIFFUSE,
				scene,
				rootPath
			);
		}
		if (diffuse == nullptr) {
			diffuse = Texture::createTexture("assets/textures/default.png", 0);
		}
		phongMaterial->mDiffuse = diffuse;
		material = phongMaterial;
	}

	return new Mesh(geometry, material);
}

PbrMaterial* AssimpLoader::createPbrMaterial(
	const aiMaterial* material,
	const aiScene* scene,
	const std::string& rootPath,
	const PbrMaterial* materialTemplate
) {
	PbrMaterial* result = new PbrMaterial();
	result->mIrradianceMap = materialTemplate->mIrradianceMap;
	result->mPrefilterMap = materialTemplate->mPrefilterMap;
	result->mBrdfLut = materialTemplate->mBrdfLut;
	result->mEnvIntensity = materialTemplate->mEnvIntensity;
	result->mMaxReflectionLod = materialTemplate->mMaxReflectionLod;
	result->mNormalStrength = materialTemplate->mNormalStrength;
	result->mDebugView = materialTemplate->mDebugView;
	result->mBaseColorFactor = materialTemplate->mBaseColorFactor;

	result->mAlbedo = processTexture(
		material,
		aiTextureType_BASE_COLOR,
		scene,
		rootPath,
		GL_SRGB_ALPHA
	);
	if (result->mAlbedo == nullptr) {
		result->mAlbedo = processTexture(
			material,
			aiTextureType_DIFFUSE,
			scene,
			rootPath,
			GL_SRGB_ALPHA
		);
	}

	result->mNormal = processTexture(
		material,
		aiTextureType_NORMALS,
		scene,
		rootPath,
		GL_RGBA
	);
	if (result->mNormal == nullptr) {
		result->mNormal = processTexture(
			material,
			aiTextureType_NORMAL_CAMERA,
			scene,
			rootPath,
			GL_RGBA
		);
	}

	Texture* metallicRoughness = processTexture(
		material,
		aiTextureType_UNKNOWN,
		scene,
		rootPath,
		GL_RGBA
	);
	if (metallicRoughness != nullptr) {
		result->mMetallic = metallicRoughness;
		result->mRoughness = metallicRoughness;
		result->mMetallicChannel = 2;
		result->mRoughnessChannel = 1;
	}
	else {
		result->mMetallic = processTexture(
			material,
			aiTextureType_METALNESS,
			scene,
			rootPath,
			GL_RGBA
		);
		result->mRoughness = processTexture(
			material,
			aiTextureType_DIFFUSE_ROUGHNESS,
			scene,
			rootPath,
			GL_RGBA
		);
	}

	result->mAo = processTexture(
		material,
		aiTextureType_AMBIENT_OCCLUSION,
		scene,
		rootPath,
		GL_RGBA
	);
	if (result->mAo == nullptr) {
		result->mAo = processTexture(
			material,
			aiTextureType_LIGHTMAP,
			scene,
			rootPath,
			GL_RGBA
		);
	}

	if (result->mAlbedo == nullptr) {
		result->mAlbedo = Texture::createSolidTexture("pbr-white-albedo", 255, 255, 255, 255, GL_SRGB_ALPHA);
	}
	if (result->mNormal == nullptr) {
		result->mNormal = Texture::createSolidTexture("pbr-flat-normal", 128, 128, 255);
	}
	if (result->mMetallic == nullptr) {
		result->mMetallic = Texture::createSolidTexture("pbr-non-metal", 0, 0, 0);
	}
	if (result->mRoughness == nullptr) {
		result->mRoughness = Texture::createSolidTexture("pbr-medium-roughness", 153, 153, 153);
	}
	if (result->mAo == nullptr) {
		result->mAo = Texture::createSolidTexture("pbr-full-ao", 255, 255, 255);
	}

	float factor = 1.0f;
	if (material->Get(AI_MATKEY_METALLIC_FACTOR, factor) == AI_SUCCESS) {
		result->mMetallicScale = factor;
	}
	if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == AI_SUCCESS) {
		result->mRoughnessScale = factor;
	}

	aiColor4D baseColorFactor;
	if (material->Get(AI_MATKEY_BASE_COLOR, baseColorFactor) == AI_SUCCESS) {
		result->mBaseColorFactor = glm::vec4(
			baseColorFactor.r,
			baseColorFactor.g,
			baseColorFactor.b,
			baseColorFactor.a
		);
	}

	float opacity = result->mBaseColorFactor.a;
	if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
		result->mOpacity = opacity;
	}
	else {
		result->mOpacity = result->mBaseColorFactor.a;
	}

	aiString alphaMode;
	if (material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
		const std::string mode(alphaMode.C_Str());
		if (mode == "BLEND") {
			result->mBlend = true;
			result->mDepthWrite = false;
		}
		else if (mode == "MASK") {
			result->mAlphaMask = true;
			float cutoff = 0.5f;
			if (material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, cutoff) == AI_SUCCESS) {
				result->mAlphaCutoff = cutoff;
			}
		}
	}

	return result;
}

void AssimpLoader::calculateBounds(
	aiNode* node,
	const aiScene* scene,
	const aiMatrix4x4& parentTransform,
	aiVector3D& minBounds,
	aiVector3D& maxBounds
) {
	const aiMatrix4x4 transform = parentTransform * node->mTransformation;

	for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
		const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
			const aiVector3D point = transform * mesh->mVertices[vertexIndex];
			minBounds.x = std::min(minBounds.x, point.x);
			minBounds.y = std::min(minBounds.y, point.y);
			minBounds.z = std::min(minBounds.z, point.z);
			maxBounds.x = std::max(maxBounds.x, point.x);
			maxBounds.y = std::max(maxBounds.y, point.y);
			maxBounds.z = std::max(maxBounds.z, point.z);
		}
	}

	for (unsigned int i = 0; i < node->mNumChildren; ++i) {
		calculateBounds(
			node->mChildren[i],
			scene,
			transform,
			minBounds,
			maxBounds
		);
	}
}

Texture* AssimpLoader::processTexture(
	const aiMaterial* material,
	const aiTextureType& type,
	const aiScene* scene,
	const std::string& rootPath,
	unsigned int internalFormat
) {
	aiString texturePath;
	material->Get(AI_MATKEY_TEXTURE(type, 0), texturePath);
	if (!texturePath.length) {
		return nullptr;
	}

	const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(texturePath.C_Str());
	if (embeddedTexture) {
		unsigned char* data = reinterpret_cast<unsigned char*>(embeddedTexture->pcData);
		return Texture::createTextureFromMemory(
			rootPath + texturePath.C_Str(),
			0,
			data,
			embeddedTexture->mWidth,
			embeddedTexture->mHeight,
			internalFormat
		);
	}

	return new Texture(rootPath + texturePath.C_Str(), 0, internalFormat);
}

glm::mat4 AssimpLoader::getMat4f(aiMatrix4x4 value) {
	return glm::mat4(
		value.a1, value.b1, value.c1, value.d1,
		value.a2, value.b2, value.c2, value.d2,
		value.a3, value.b3, value.c3, value.d3,
		value.a4, value.b4, value.c4, value.d4
	);
}
