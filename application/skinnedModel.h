#pragma once

#include <string>
#include <vector>

#include "../glframework/core.h"
#include "../glframework/mesh/mesh.h"
#include "../glframework/object.h"

class AssimpLoader;

class SkinnedModel {
public:
	Object* root() const { return mRoot; }

	int clipCount() const { return static_cast<int>(mClips.size()); }
	const std::string& clipName(int index) const;
	int clipIndex() const { return mClipIndex; }
	void setClip(int index);

	bool isPlaying() const { return mPlaying; }
	void setPlaying(bool playing) { mPlaying = playing; }
	bool isLooping() const { return mLooping; }
	void setLooping(bool looping) { mLooping = looping; }
	float playbackSpeed() const { return mPlaybackSpeed; }
	void setPlaybackSpeed(float speed) { mPlaybackSpeed = speed; }

	float durationSeconds() const;
	float timeSeconds() const { return static_cast<float>(mTimeSeconds); }
	float normalizedTime() const;
	void setNormalizedTime(float normalizedTime);
	void reset();
	void update(float deltaSeconds);

	int nodeCount() const { return static_cast<int>(mNodes.size()); }
	int boneCount() const { return mBoneCount; }
	int meshCount() const { return static_cast<int>(mSkinBindings.size()); }

private:
	struct VectorKey {
		double time{ 0.0 };
		glm::vec3 value{ 0.0f };
	};

	struct RotationKey {
		double time{ 0.0 };
		glm::quat value{ 1.0f, 0.0f, 0.0f, 0.0f };
	};

	struct Node {
		std::string name;
		int parent{ -1 };
		glm::mat4 bindLocal{ 1.0f };
		glm::vec3 bindTranslation{ 0.0f };
		glm::quat bindRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
		glm::vec3 bindScale{ 1.0f };
		glm::mat4 global{ 1.0f };
	};

	struct Channel {
		int nodeIndex{ -1 };
		std::vector<VectorKey> positions;
		std::vector<RotationKey> rotations;
		std::vector<VectorKey> scales;
	};

	struct Clip {
		std::string name;
		double durationTicks{ 0.0 };
		double ticksPerSecond{ 25.0 };
		std::vector<Channel> channels;
		std::vector<int> channelByNode;
	};

	struct SkinBinding {
		Mesh* mesh{ nullptr };
		std::vector<int> boneNodes;
		std::vector<glm::mat4> boneOffsets;
	};

	static glm::vec3 sampleVectorKeys(
		const std::vector<VectorKey>& keys,
		double time,
		const glm::vec3& fallback
	);
	static glm::quat sampleRotationKeys(
		const std::vector<RotationKey>& keys,
		double time,
		const glm::quat& fallback
	);
	void evaluatePose();

	Object* mRoot{ nullptr };
	std::vector<Node> mNodes;
	std::vector<Clip> mClips;
	std::vector<SkinBinding> mSkinBindings;
	glm::mat4 mGlobalInverseRoot{ 1.0f };
	int mClipIndex{ -1 };
	int mBoneCount{ 0 };
	double mTimeSeconds{ 0.0 };
	bool mPlaying{ true };
	bool mLooping{ true };
	float mPlaybackSpeed{ 1.0f };

	friend class AssimpLoader;
};
