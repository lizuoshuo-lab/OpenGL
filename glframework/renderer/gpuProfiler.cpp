#include "gpuProfiler.h"

GpuProfiler::~GpuProfiler() {
	if (mQueryRunning) {
		glEndQuery(GL_TIME_ELAPSED);
	}
	for (auto& pair : mQueries) {
		glDeleteQueries(
			static_cast<GLsizei>(pair.second.ids.size()),
			pair.second.ids.data()
		);
	}
}

GpuProfiler::QueryRecord& GpuProfiler::getOrCreate(const std::string& name) {
	auto existing = mQueries.find(name);
	if (existing != mQueries.end()) {
		return existing->second;
	}

	QueryRecord record;
	glGenQueries(static_cast<GLsizei>(record.ids.size()), record.ids.data());
	auto inserted = mQueries.emplace(name, record);
	mOrder.push_back(name);
	return inserted.first->second;
}

void GpuProfiler::beginFrame() {
	mFrameSlot = static_cast<std::size_t>(mFrameIndex % kBufferedFrames);
	for (auto& pair : mQueries) {
		QueryRecord& record = pair.second;
		record.activeThisFrame = false;
		if (!record.issued[mFrameSlot]) {
			continue;
		}

		GLint available = GL_FALSE;
		glGetQueryObjectiv(record.ids[mFrameSlot], GL_QUERY_RESULT_AVAILABLE, &available);
		if (available == GL_TRUE) {
			GLuint64 elapsedNanoseconds = 0;
			glGetQueryObjectui64v(
				record.ids[mFrameSlot],
				GL_QUERY_RESULT,
				&elapsedNanoseconds
			);
			record.milliseconds = static_cast<double>(elapsedNanoseconds) / 1000000.0;
			record.issued[mFrameSlot] = false;
		}
	}
}

void GpuProfiler::beginPass(const std::string& name) {
	if (mQueryRunning) {
		endPass();
	}

	QueryRecord& record = getOrCreate(name);
	record.activeThisFrame = true;
	mActivePass = name;
	if (record.issued[mFrameSlot]) {
		mQueryRunning = false;
		return;
	}

	glBeginQuery(GL_TIME_ELAPSED, record.ids[mFrameSlot]);
	record.issued[mFrameSlot] = true;
	mQueryRunning = true;
}

void GpuProfiler::endPass() {
	if (mQueryRunning) {
		glEndQuery(GL_TIME_ELAPSED);
	}
	mQueryRunning = false;
	mActivePass.clear();
}

void GpuProfiler::endFrame() {
	if (mQueryRunning) {
		endPass();
	}
	++mFrameIndex;
}

double GpuProfiler::getMilliseconds(const std::string& name) const {
	const auto found = mQueries.find(name);
	return found != mQueries.end() ? found->second.milliseconds : 0.0;
}

std::vector<GpuPassTiming> GpuProfiler::getTimings() const {
	std::vector<GpuPassTiming> result;
	result.reserve(mOrder.size());
	for (const std::string& name : mOrder) {
		const auto found = mQueries.find(name);
		if (found == mQueries.end()) {
			continue;
		}
		result.push_back({
			name,
			found->second.milliseconds,
			found->second.activeThisFrame
		});
	}
	return result;
}
