#include "DSPRecorder.hpp"
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/loader/Log.hpp>

using namespace geode::prelude;

#ifndef GEODE_IS_IOS
DSPRecorder* DSPRecorder::get() {
    static DSPRecorder instance;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        instance.init();
    }
    return &instance;
}

void DSPRecorder::init() {
    auto* system = FMODAudioEngine::sharedEngine()->m_system;

    FMOD_DSP_DESCRIPTION desc = {};
    std::strncpy(desc.name, "xdbot_capture", sizeof(desc.name));
    desc.numinputbuffers  = 1;
    desc.numoutputbuffers = 1;
    desc.read = [](FMOD_DSP_STATE*, float* inbuffer, float* outbuffer,
                   unsigned int length, int, int* outchannels) -> FMOD_RESULT {
        auto* recorder = DSPRecorder::get();
        if (!recorder->m_recording) return FMOD_OK;

        int channels = *outchannels;
        {
            auto guard = recorder->m_data.lock();
            guard->insert(guard->end(), inbuffer, inbuffer + length * channels);
        }

        std::memcpy(outbuffer, inbuffer, length * channels * sizeof(float));

        FMOD::ChannelGroup* master = nullptr;
        FMODAudioEngine::sharedEngine()->m_system->getMasterChannelGroup(&master);
        if (master) master->setPaused(true);

        return FMOD_OK;
    };

    system->createDSP(&desc, &m_dsp);
    system->getMasterChannelGroup(&m_masterGroup);
}

void DSPRecorder::start() {
    if (m_recording) return;
    m_masterGroup->addDSP(0, m_dsp);
    {
        auto guard = m_data.lock();
        guard->clear();
    }
    m_recording = true;
    log::info("DSPRecorder: started");
}

void DSPRecorder::stop() {
    if (!m_recording) return;
    m_recording = false;
    geode::queueInMainThread([this] {
        m_masterGroup->removeDSP(m_dsp);
        m_masterGroup->setPaused(false);
        log::info("DSPRecorder: stopped, {} samples captured", m_data.lock()->size());
    });
}

std::vector<float> DSPRecorder::getData() {
    auto guard = m_data.lock();
    return std::move(*guard);
}

void DSPRecorder::tryUnpause(float time) const {
    if (!m_recording) return;
    if (!m_masterGroup) return;
    auto* system = FMODAudioEngine::sharedEngine()->m_system;
    int sampleRate = 0, channels = 0;
    system->getSoftwareFormat(&sampleRate, nullptr, &channels);
    if (sampleRate <= 0 || channels <= 0) return;

    float songTime;
    {
        auto guard = m_data.lock();
        songTime = static_cast<float>(guard->size()) /
                   (static_cast<float>(sampleRate) * static_cast<float>(channels));
    }
    if (time >= songTime) m_masterGroup->setPaused(false);
}

#endif