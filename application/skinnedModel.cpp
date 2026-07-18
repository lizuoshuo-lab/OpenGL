#include "skinnedModel.h"

#include <algorithm>
#include <cmath>

glm::vec3 SkinnedModel::sampleVectorKeys(
	const std::vector<VectorKey>& keys,
	double time,
	const glm::vec3& fallback
) {
	if (keys.empty()) {
		return fallback;
	}
	if (keys.size() == 1 || time <= keys.front().time) {
		return keys.front().value;
	}
	if (time >= keys.back().time) {
		return keys.back().value;
	}

	const auto upper = std::upper_bound(
		keys.begin(),
		keys.end(),
		time,
		[](double value, const VectorKey& key) {
			return value < key.time;
		}
	);
	const std::size_t next = static_cast<std::size_t>(upper - keys.begin());
	const std::size_t previous = next - 1;
	const double span = keys[next].time - keys[previous].time;
	const float factor = span > 0.0
		? static_cast<float>((time - keys[previous].time) / span)
		: 0.0f;
	return glm::mix(keys[previous].value, keys[next].value, factor);
}

glm::quat SkinnedModel::sampleRotationKeys(
	const std::vector<RotationKey>& keys,
	double time,
	const glm::quat& fallback
) {
	if (keys.empty()) {
		return fallback;
	}
	if (keys.size() == 1 || time <= keys.front().time) {
		return glm::normalize(keys.front().value);
	}
	if (time >= keys.back().time) {
		return glm::normalize(keys.back().value);
	}

	const auto upper = std::upper_bound(
		keys.begin(),
		keys.end(),
		time,
		[](double value, const RotationKey& key) {
			return value < key.time;
		}
	);
	const std::size_t next = static_cast<std::size_t>(upper - keys.begin());
	const std::size_t previous = next - 1;
	const double span = keys[next].time - keys[previous].time;
	const float factor = span > 0.0
		? static_cast<float>((time - keys[previous].time) / span)
		: 0.0f;
	return glm::normalize(glm::slerp(
		keys[previous].value,
		keys[next].value,
		factor
	));
}

const std::string& SkinnedModel::clipName(int index) const {
	static const std::string empty;
	if (index < 0 || index >= clipCount()) {
		return empty;
	}
	return mClips[index].name;
}

void SkinnedModel::setClip(int index) {
	if (mClips.empty()) {
		mClipIndex = -1;
		mTimeSeconds = 0.0;
		evaluatePose();
		return;
	}
	mClipIndex = std::clamp(index, 0, clipCount() - 1);
	mTimeSeconds = 0.0;
	evaluatePose();
}

float SkinnedModel::durationSeconds() const {
	if (mClipIndex < 0 || mClipIndex >= clipCount()) {
		return 0.0f;
	}
	const Clip& clip = mClips[mClipIndex];
	return clip.ticksPerSecond > 0.0
		? static_cast<float>(clip.durationTicks / clip.ticksPerSecond)
		: 0.0f;
}

float SkinnedModel::normalizedTime() const {
	const float duration = durationSeconds();
	return duration > 0.0f
		? std::clamp(static_cast<float>(mTimeSeconds) / duration, 0.0f, 1.0f)
		: 0.0f;
}

void SkinnedModel::setNormalizedTime(float normalizedTime) {
	const float duration = durationSeconds();
	mTimeSeconds = static_cast<double>(
		std::clamp(normalizedTime, 0.0f, 1.0f) * duration
	);
	evaluatePose();
}

void SkinnedModel::reset() {
	mTimeSeconds = 0.0;
	evaluatePose();
}

void SkinnedModel::update(float deltaSeconds) {
	if (!mPlaying || mClipIndex < 0 || mClipIndex >= clipCount()) {
		return;
	}

	const double duration = static_cast<double>(durationSeconds());
	if (duration <= 0.0) {
		return;
	}

	mTimeSeconds += static_cast<double>(deltaSeconds * mPlaybackSpeed);
	if (mLooping) {
		mTimeSeconds = std::fmod(mTimeSeconds, duration);
		if (mTimeSeconds < 0.0) {
			mTimeSeconds += duration;
		}
	}
	else if (mTimeSeconds >= duration) {
		mTimeSeconds = duration;
		mPlaying = false;
	}
	else if (mTimeSeconds < 0.0) {
		mTimeSeconds = 0.0;
		mPlaying = false;
	}

	evaluatePose();
}

void SkinnedModel::evaluatePose() {
	const Clip* clip = mClipIndex >= 0 && mClipIndex < clipCount()
		? &mClips[mClipIndex]
		: nullptr;
	const double timeTicks = clip != nullptr
		? mTimeSeconds * clip->ticksPerSecond
		: 0.0;

	for (std::size_t nodeIndex = 0; nodeIndex < mNodes.size(); ++nodeIndex) {
		Node& node = mNodes[nodeIndex];
		glm::mat4 local = node.bindLocal;

		if (clip != nullptr && nodeIndex < clip->channelByNode.size()) {
			const int channelIndex = clip->channelByNode[nodeIndex];
			if (channelIndex >= 0) {
				const Channel& channel = clip->channels[channelIndex];
				const glm::vec3 translation = sampleVectorKeys(
					channel.positions,
					timeTicks,
					node.bindTranslation
				);
				const glm::quat rotation = sampleRotationKeys(
					channel.rotations,
					timeTicks,
					node.bindRotation
				);
				const glm::vec3 scale = sampleVectorKeys(
					channel.scales,
					timeTicks,
					node.bindScale
				);

				local = glm::translate(glm::mat4(1.0f), translation) *
					glm::mat4_cast(rotation) *
					glm::scale(glm::mat4(1.0f), scale);
			}
		}

		node.global = node.parent >= 0
			? mNodes[node.parent].global * local
			: local;
	}

	for (SkinBinding& binding : mSkinBindings) {
		if (binding.mesh == nullptr) {
			continue;
		}
		std::vector<glm::mat4> palette(binding.boneNodes.size(), glm::mat4(1.0f));
		for (std::size_t boneIndex = 0; boneIndex < binding.boneNodes.size(); ++boneIndex) {
			const int nodeIndex = binding.boneNodes[boneIndex];
			if (nodeIndex < 0 || nodeIndex >= static_cast<int>(mNodes.size())) {
				continue;
			}
			palette[boneIndex] = mGlobalInverseRoot *
				mNodes[nodeIndex].global *
				binding.boneOffsets[boneIndex];
		}
		binding.mesh->setBoneMatrices(palette);
	}
}
