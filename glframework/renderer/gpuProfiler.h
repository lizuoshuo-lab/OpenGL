#pragma once

#include "../core.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct GpuPassTiming {
	std::string name;
	double milliseconds{ 0.0 };
	bool active{ false };
};

class GpuProfiler {
public:
	GpuProfiler() = default;
	~GpuProfiler();

	void beginFrame();
	void beginPass(const std::string& name);
	void endPass();
	void endFrame();

	double getMilliseconds(const std::string& name) const;
	std::vector<GpuPassTiming> getTimings() const;

private:
	static constexpr std::size_t kBufferedFrames = 4;

	struct QueryRecord {
		std::array<GLuint, kBufferedFrames> ids{};
		std::array<bool, kBufferedFrames> issued{};
		double milliseconds{ 0.0 };
		bool activeThisFrame{ false };
	};

	QueryRecord& getOrCreate(const std::string& name);

	std::unordered_map<std::string, QueryRecord> mQueries;
	std::vector<std::string> mOrder;
	std::string mActivePass;
	std::uint64_t mFrameIndex{ 0 };
	std::size_t mFrameSlot{ 0 };
	bool mQueryRunning{ false };
};
