#pragma once

#include "render_settings.hpp"

#include <Geode/loader/Event.hpp>
#include <Geode/Result.hpp>
#include <functional>

namespace ffmpeg::events {
namespace impl {

#define DEFAULT_RESULT_ERROR geode::Err("Event was not handled")

	// Listeners should supply a pointer to an internal recorder implementation
	// by returning it when CreateRecorderEvent is sent.
	// Create: returns created pointer via out-parameter
	class CreateRecorderEvent : public geode::ThreadSafeEvent<CreateRecorderEvent, bool(void*&)> {};

	// Listener will be called with the pointer to delete/cleanup.
	class DeleteRecorderEvent : public geode::ThreadSafeEvent<DeleteRecorderEvent, void(void*)> {};

	// Initialize recorder: listener should return a geode::Result<>
	// Initialize: listener should write result into out param
	class InitRecorderEvent : public geode::ThreadSafeEvent<InitRecorderEvent, bool(void*, geode::Result<>&, const RenderSettings&)> {};

	class StopRecorderEvent : public geode::ThreadSafeEvent<StopRecorderEvent, void(void*)> {};

	// Provide a write-frame function. Listener should return a callable which accepts (void* impl, const std::vector<uint8_t>&)
	using write_frame_fn_t = std::function<geode::Result<>(void*, const std::vector<uint8_t>&)>;
	// Provide write-frame function via out param
	class GetWriteFrameFunctionEvent : public geode::ThreadSafeEvent<GetWriteFrameFunctionEvent, bool(write_frame_fn_t&)> {};

	// Provide codecs via out param
	class CodecRecorderEvent : public geode::ThreadSafeEvent<CodecRecorderEvent, bool(std::vector<std::string>&)> {};

	// Mixing: write result into out param
	class MixVideoAudioEvent : public geode::ThreadSafeEvent<MixVideoAudioEvent, bool(geode::Result<>&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&)> {};

	class MixVideoRawEvent : public geode::ThreadSafeEvent<MixVideoRawEvent, bool(geode::Result<>&, const std::filesystem::path&, const std::vector<float>&, const std::filesystem::path&)> {};

#undef DEFAULT_RESULT_ERROR
}

// Recorder wrapper that communicates with a platform-specific backend via Geode events.
class Recorder {
public:
	Recorder() {
		// Ask listeners to create a recorder and return implementation pointer via out-param
		void* ptr = nullptr;
		if (impl::CreateRecorderEvent().send(ptr)) {
			m_ptr = static_cast<Dummy*>(ptr);
		} else {
			m_ptr = nullptr;
		}
	}

	~Recorder() {
		if (m_ptr) {
			impl::DeleteRecorderEvent().send(reinterpret_cast<void*>(m_ptr));
			m_ptr = nullptr;
		}
	}

	bool isValid() const { return m_ptr != nullptr; }

	geode::Result<> init(RenderSettings const& settings) {
		if (!m_ptr) return geode::Err("Recorder not initialized");
		geode::Result<> res = geode::Err("Event was not handled");
		if (!impl::InitRecorderEvent().send(reinterpret_cast<void*>(m_ptr), res, settings))
			return geode::Err("Event was not handled");
		return res;
	}

	void stop() {
		if (!m_ptr) return;
		impl::StopRecorderEvent().send(reinterpret_cast<void*>(m_ptr));
	}

	geode::Result<> writeFrame(const std::vector<uint8_t>& frameData) {
		impl::write_frame_fn_t fn;
		if (!impl::GetWriteFrameFunctionEvent().send(fn))
			return geode::Err("Failed to obtain write-frame function");
		if (!fn) return geode::Err("No write-frame function available");
		return fn(reinterpret_cast<void*>(m_ptr), frameData);
	}

	static std::vector<std::string> getAvailableCodecs() {
		std::vector<std::string> out;
		if (!impl::CodecRecorderEvent().send(out))
			return {};
		return out;
	}

private:
	// opaque impl pointer returned by platform-specific listener
	struct Dummy {};
	Dummy* m_ptr = nullptr;
};

class AudioMixer {
public:
	AudioMixer() = delete;

	static geode::Result<> mixVideoAudio(const std::filesystem::path& videoFile, const std::filesystem::path& audioFile, const std::filesystem::path& outputMp4File) {
		geode::Result<> out = geode::Err("Event was not handled");
		if (!impl::MixVideoAudioEvent().send(out, videoFile, audioFile, outputMp4File))
			return geode::Err("Event was not handled");
		return out;
	}

	static geode::Result<> mixVideoRaw(const std::filesystem::path& videoFile, const std::vector<float>& raw, const std::filesystem::path& outputMp4File) {
		geode::Result<> out = geode::Err("Event was not handled");
		if (!impl::MixVideoRawEvent().send(out, videoFile, raw, outputMp4File))
			return geode::Err("Event was not handled");
		return out;
	}
};

} // namespace ffmpeg::events
