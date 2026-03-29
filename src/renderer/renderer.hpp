#pragma once

#include "../includes.hpp"
#include "ffmpeg/events.hpp"
#include "spinlock.hpp"

#ifndef GEODE_IS_IOS

class xdBotRenderTexture {
public:
    unsigned width   = 0;
    unsigned height  = 0;
    int      old_fbo = 0;
    unsigned fbo     = 0;
    GLuint   texture = 0;

    void begin();
    void end();
    void capture(cocos2d::CCNode* node, std::vector<uint8_t>& buffer, Spinlock& frameReady);
};

enum class AudioMode {
    Off    = 0,
    Record = 1
};

class Renderer {
public:
    Renderer() : width(1920), height(1080), fps(60) {}

    bool recording     = false;
    bool levelFinished = false;
    bool capturing     = false;

    AudioMode audioMode   = AudioMode::Off;
    float     SFXVolume   = 1.f;
    float     musicVolume = 1.f;

    bool usingApi = false;

    float stopAfter = 3.f;
    float timeAfter = 0.f;

    float totalTime   = 0.f;
    float extra_t     = 0.f;
    float lastFrame_t = 0.f;
    float capturedLastFrameTime = 0.f; 

    unsigned width, height, fps;

    cocos2d::CCSize oldDesignResolution = {0, 0};
    cocos2d::CCSize newDesignResolution = {0, 0};
    cocos2d::CCSize originalScreenScale = {0, 0};
    cocos2d::CCSize newScreenScale      = {0, 0};

    ffmpeg::events::Recorder ffmpeg;

#ifdef GEODE_IS_WINDOWS
    std::string ffmpegPath;
#endif

    std::string codec, bitrate, extraArgs, videoArgs, extraAudioArgs, path;

    FMODAudioEngine* fmod = nullptr;

    void captureFrame();
    void applyWinSize();
    void restoreWinSize();
    void fixUIObjects();
    void start();
    void stop(int frame = 0);

    static bool toggle();
    static bool shouldUseAPI();

private:
    Spinlock             m_frameReady;
    std::vector<uint8_t> m_currentFrame;
    xdBotRenderTexture   m_renderTexture;

    void recordThread();
    void showEndScreenIfNeeded();
};

#endif