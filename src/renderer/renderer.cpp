#include "../includes.hpp"
#include "../ui/game_ui.hpp"
#include "DSPRecorder.hpp"

#include <Geode/modify/CCCircleWave.hpp>
#include <Geode/modify/CCParticleSystem.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/ShaderLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>

#ifdef GEODE_IS_WINDOWS
#include "../utils/subprocess.hpp"
#endif

#ifndef GEODE_IS_IOS

static bool writePCMWav(const std::filesystem::path& outPath, std::span<const float> pcm, FMOD::System* system);
bool m_cbsWasEnabled;

class $modify(CCParticleSystem) {
    void initParticle(sCCParticle* p0) {
        CCParticleSystem::initParticle(p0);

        if (!Mod::get()->getSavedValue<bool>("render_hide_levelcomplete")) return;

        if (auto particle = typeinfo_cast<CCParticleSystemQuad*>(this)) {
            PlayLayer* pl = PlayLayer::get();
            if (!pl) return;
            if (this->getParent() != pl) return;

            particle->setVisible(false);
        }
    }
};

class $modify(CCCircleWave) {
    static CCCircleWave* create(float v1, float v2, float v3, bool v4, bool v5) {
        CCCircleWave* ret = CCCircleWave::create(v1, v2, v3, v4, v5);
        if (!Global::get().renderer.recording ||
            !PlayLayer::get()->m_levelEndAnimationStarted) return ret;
        if (Mod::get()->getSavedValue<bool>("render_hide_levelcomplete"))
            ret->setVisible(false);
        return ret;
    }
};

class $modify(PlayLayer) {
    void showCompleteText() {
        PlayLayer::showCompleteText();

        if (!Global::get().renderer.recording) return;

        if (m_levelEndAnimationStarted && Mod::get()->getSavedValue<bool>("render_hide_levelcomplete")) {
                const char* spriteName = m_isPracticeMode ?
                    "GJ_practiceComplete_001.png" :
                    "GJ_levelComplete_001.png";

            if (auto* node = getChildBySpriteFrameName(this, spriteName)) {
                node->stopAllActions();
                node->setVisible(false);
            }
        }
    }
};

class $modify(EndLevelLayer) {
    void customSetup() {
        EndLevelLayer::customSetup();
        if (!PlayLayer::get()) return;
        if (Global::get().renderer.recording &&
            PlayLayer::get()->m_levelEndAnimationStarted &&
            Mod::get()->getSavedValue<bool>("render_hide_endscreen")) {
            Loader::get()->queueInMainThread([this] { setVisible(false); });
        }
    }
};

class $modify(FMODAudioEngine) {
    int playEffect(gd::string path, float speed, float p2, float volume) {
        if (path == "explode_11.ogg" && Global::get().renderer.recording) return 0;
        return FMODAudioEngine::playEffect(path, speed, p2, volume);
    }
};

// class $modify(InternalRecorderSLHook, ShaderLayer) {
//     void visit() {
//         if (Global::get().renderer.recording) {
//             setScaleY(-1);
//             ShaderLayer::visit();
//             return setScaleY(1);
//         }
//         ShaderLayer::visit();
//     }
// };

class $modify(CCScheduler) {
    void update(float dt) {
        Renderer& r = Global::get().renderer;
        if (!r.recording) return CCScheduler::update(dt);

        r.applyWinSize();

        float fps = static_cast<float>(r.fps);
        if (fps < 1) fps = 1;

        float newDt = (1.f / fps) * (fps / static_cast<float>(Global::getTPS()));

        CCScheduler::update(newDt);

        r.restoreWinSize();
    }
};

class $modify(GJBaseGameLayer) {
    void update(float dt) {
        Renderer& r = Global::get().renderer;

        if (!r.recording || m_gameState.m_currentProgress <= 0)
            return GJBaseGameLayer::update(dt);

        float endscreen = Mod::get()->getSavedValue<float>("render_seconds_after");

        if (r.levelFinished) {
            r.timeAfter += dt;
            if (r.timeAfter >= r.stopAfter) {
                r.stop();
                return GJBaseGameLayer::update(dt);
            }
        }

        if (!r.recording)
            return GJBaseGameLayer::update(dt);

        float fps      = static_cast<float>(r.fps);
        float timewarp = m_gameState.m_timeWarp;

        r.totalTime += dt;

        float frameDt = (1.f / fps) * timewarp;
        float time    = r.totalTime + r.extra_t - r.lastFrame_t;

        DSPRecorder::get()->tryUnpause(r.totalTime);

        if (time >= frameDt) {
            r.extra_t     = time - frameDt;
            r.lastFrame_t = r.totalTime;

            r.capturing = true;
            r.captureFrame();
            r.capturing = false;
        }

        GJBaseGameLayer::update(dt);
    }
};

class $modify(RendererPlayLayerHook, PlayLayer) {
    void onQuit() {
        if (Global::get().renderer.recording) Global::get().renderer.stop();
        PlayLayer::onQuit();
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        Global::get().renderer.levelFinished = true;
    }

    void resetLevel() {
        auto& r = Global::get().renderer;
        if (r.recording) {
            r.levelFinished = false;
            r.timeAfter     = 0.f;
        }
        PlayLayer::resetLevel();
    }
};

bool Renderer::shouldUseAPI() {
    return Loader::get()->isModLoaded("eclipse.ffmpeg-api");
}

bool Renderer::toggle() {
    auto& g = Global::get();
    auto gm = GameManager::sharedState();
    auto cbf = Loader::get()->getLoadedMod("syzzi.click_between_frames");
    if (cbf && !cbf->getSettingValue<bool>("soft-toggle")) {
        geode::createQuickPopup(
            "Render",
            "Disable <cr>CBF</c> in Geode to render a level.",
            "Close", "Disable",
            [cbf](auto, bool btn2) {
                if (btn2) {
                    cbf->setSettingValue<bool>("soft-toggle", false);
                }
            }
        );
        return false;
    }

    m_cbsWasEnabled = gm->getGameVariable(GameVar::ClickBetweenSteps);
    if (m_cbsWasEnabled) {
        gm->setGameVariable(GameVar::ClickBetweenSteps, false);
    }

    bool foundApi = shouldUseAPI();

#ifdef GEODE_IS_WINDOWS
    std::filesystem::path ffmpegSettingPath =
        Mod::get()->getSettingValue<std::filesystem::path>("ffmpeg_path");
    bool foundExe = std::filesystem::exists(ffmpegSettingPath) &&
        geode::utils::string::pathToString(ffmpegSettingPath.filename()) == "ffmpeg.exe";
#endif

    if (g.renderer.recording) {
        g.renderer.stop();
    } else {
#ifdef GEODE_IS_WINDOWS
        if (!foundApi && !foundExe) {
            geode::createQuickPopup("Error",
                "<cl>FFmpeg</c> not found. Either install FFmpeg API, or set the path "
                "to ffmpeg.exe in mod settings.\nOpen download link?",
                "Cancel", "Yes", [](auto, bool btn2) {
                    if (btn2) {
                        FLAlertLayer::create("Info",
                            "Unzip the downloaded file and look for <cl>ffmpeg.exe</c> "
                            "in the 'bin' folder.", "OK")->show();
                        utils::web::openLinkInBrowser(
                            "https://www.gyan.dev/ffmpeg/builds/ffmpeg-git-essentials.7z");
                    }
                });
            return false;
        }
        if (!foundApi)
            g.renderer.ffmpegPath = geode::utils::string::pathToString(ffmpegSettingPath);
#else
        if (!foundApi) {
            geode::createQuickPopup(
                "Error",
                "The <cl>FFmpeg API</c> mod is required for rendering on this platform.",
                "Close", "Open Mod Page",
                [](auto, bool btn2) {
                    if (btn2) {
                        geode::openInfoPopup("eclipse.ffmpeg-api");
                    }
                }
            );
            return false;
        }
#endif

        if (!PlayLayer::get()) {
            FLAlertLayer::create("Warning", "<cl>Open a level</c> to start rendering it.", "OK")->show();
            return false;
        }

#ifdef GEODE_IS_IOS
        std::filesystem::path renderFolder = Mod::get()->getSaveDir() / "renders";
#else
        std::filesystem::path renderFolder =
            Mod::get()->getSettingValue<std::filesystem::path>("render_folder");
#endif

        if (!std::filesystem::exists(renderFolder)) {
            if (!utils::file::createDirectoryAll(renderFolder).isOk()) {
                FLAlertLayer::create("Error",
                    "There was an error getting the renders folder. ID: 11", "OK")->show();
                return false;
            }
        }

        g.renderer.usingApi = foundApi;
        g.renderer.start();
    }

    Interface::updateLabels();
    return true;
}

void Renderer::applyWinSize() {
    if (newDesignResolution.width == 0 || newDesignResolution.height == 0) return;

    auto view = cocos2d::CCEGLView::get();
    CCDirector::sharedDirector()->m_obWinSizeInPoints = newDesignResolution;
    view->setDesignResolutionSize(
        newDesignResolution.width, newDesignResolution.height,
        ResolutionPolicy::kResolutionExactFit);
    view->m_fScaleX = newScreenScale.width;
    view->m_fScaleY = newScreenScale.height;
}

void Renderer::restoreWinSize() {
    if (oldDesignResolution.width == 0 || oldDesignResolution.height == 0) return;

    auto view = cocos2d::CCEGLView::get();
    CCDirector::sharedDirector()->m_obWinSizeInPoints = oldDesignResolution;
    view->setDesignResolutionSize(
        oldDesignResolution.width, oldDesignResolution.height,
        ResolutionPolicy::kResolutionExactFit);
    view->m_fScaleX = originalScreenScale.width;
    view->m_fScaleY = originalScreenScale.height;
}

void Renderer::fixUIObjects() {
    auto pl = PlayLayer::get();
    if (!pl) return;

    for (auto obj : pl->m_objects->asExt<GameObject>()) {
        auto it = pl->m_uiObjectPositions.find(obj->m_uniqueID);
        if (it == pl->m_uiObjectPositions.end()) continue;
        obj->setStartPos(it->second);
    }

    pl->positionUIObjects();
}

void Renderer::start() {
    PlayLayer* pl  = PlayLayer::get();
    Mod*       mod = Mod::get();
    fmod = FMODAudioEngine::sharedEngine();

    fps            = geode::utils::numFromString<int>(
                         mod->getSavedValue<std::string>("render_fps")).unwrapOr(60);
    codec          = mod->getSavedValue<std::string>("render_codec");
    bitrate        = mod->getSavedValue<std::string>("render_bitrate") + "M";
    extraArgs      = mod->getSavedValue<std::string>("render_args");
    videoArgs      = mod->getSavedValue<std::string>("render_video_args");
    extraAudioArgs = mod->getSavedValue<std::string>("render_audio_args");
    SFXVolume      = mod->getSavedValue<double>("render_sfx_volume");
    musicVolume    = mod->getSavedValue<double>("render_music_volume");
    stopAfter      = geode::utils::numFromString<float>(
                         mod->getSavedValue<std::string>("render_seconds_after")).unwrapOr(0.f);
    audioMode      = AudioMode::Record;

    std::string extension = mod->getSavedValue<std::string>("render_file_extension");
    auto timestamp = asp::time::SystemTime::now().timeSinceEpoch().millis();
    std::string filename = fmt::format("render_{}_{}{}",
        std::string_view(pl->m_level->m_levelName),
        geode::utils::numToString(timestamp), extension);

#ifdef GEODE_IS_IOS
    path = geode::utils::string::pathToString(mod->getSaveDir() / "renders" / filename);
#else
    path = geode::utils::string::pathToString(
        mod->getSettingValue<std::filesystem::path>("render_folder") / filename);
#endif

    width  = geode::utils::numFromString<int>(
                 mod->getSavedValue<std::string>("render_width2")).unwrapOr(1920);
    height = geode::utils::numFromString<int>(
                 mod->getSavedValue<std::string>("render_height")).unwrapOr(1080);
    if (width  % 2 != 0) width++;
    if (height % 2 != 0) height++;

    auto view = cocos2d::CCEGLView::get();
    oldDesignResolution = view->getDesignResolutionSize();
    originalScreenScale = cocos2d::CCSize(view->m_fScaleX, view->m_fScaleY);

    float aspectRatio   = static_cast<float>(width) / static_cast<float>(height);
    newDesignResolution = cocos2d::CCSize(roundf(320.f * aspectRatio), 320.f);

    float retinaRatio = geode::utils::getDisplayFactor();
    newScreenScale = cocos2d::CCSize(
        static_cast<float>(width)  / newDesignResolution.width  / retinaRatio,
        static_cast<float>(height) / newDesignResolution.height / retinaRatio);

    recording     = true;
    levelFinished = false;
    capturing     = false;
    timeAfter     = 0.f;
    totalTime     = 0.f;
    extra_t       = 0.f;
    lastFrame_t   = 0.f;

    m_renderTexture        = xdBotRenderTexture();
    m_renderTexture.width  = width;
    m_renderTexture.height = height;
    m_currentFrame.resize(width * height * 4, 0);

    m_renderTexture.begin();

    if (oldDesignResolution != newDesignResolution) {
        applyWinSize();
        fixUIObjects();
    }

    DSPRecorder::get()->start();

    if (!pl->m_levelEndAnimationStarted && pl->m_isPaused)
        Global::get().restart = true;

    if (Global::get().state != state::playing && !Global::get().macro.inputs.empty())
        Macro::togglePlaying();

    if (!mod->setSavedValue("first_render_", true)) {
        FLAlertLayer::create("Warning",
            "If you have a macro for the level, <cl>let it run</c> to allow the level to render.",
            "OK")->show();
    }

    async::runtime().spawnBlocking<void>([this]() { recordThread(); });
}

void Renderer::stop() {
    if (!recording) return;
    recording = false;

    m_frameReady.set(true);

     if (m_cbsWasEnabled) {
        GameManager::sharedState()->setGameVariable(GameVar::ClickBetweenSteps, true);
    }

    m_renderTexture.end();
    restoreWinSize();
    DSPRecorder::get()->stop();
}

void Renderer::captureFrame() {
    m_frameReady.wait_for(false);

    if (!recording) return;

    m_renderTexture.capture(PlayLayer::get(), m_currentFrame, m_frameReady);
}

void xdBotRenderTexture::begin() {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);

    auto bytesPerRow = width * 4;
    if      (bytesPerRow % 8 == 0) glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
    else if (bytesPerRow % 4 == 0) glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    else if (bytesPerRow % 2 == 0) glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    else                           glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, texture, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);
}

void xdBotRenderTexture::end() {
    if (texture) { glDeleteTextures(1,    &texture); texture = 0; }
    if (fbo)     { glDeleteFramebuffers(1, &fbo);    fbo     = 0; }
}

void xdBotRenderTexture::capture(cocos2d::CCNode* node, std::vector<uint8_t>& buffer,
                                  Spinlock& frameReady) {
    glViewport(0, 0, width, height);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    node->visit();

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());

    frameReady.set(true);

    glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);
    CCDirector::sharedDirector()->setViewport();
}

void Renderer::recordThread() {
    geode::utils::thread::setName("xdBot Recorder Thread");

    Mod* mod = Mod::get();

    int bitrateVal = geode::utils::numFromString<int>(
        mod->getSavedValue<std::string>("render_bitrate")).unwrapOr(12) * 1000000;

    log::info("Renderer: usingApi={}, videoArgs='{}'", usingApi, videoArgs);

    ffmpeg::RenderSettings settings;
    settings.m_pixelFormat       = ffmpeg::PixelFormat::RGBA;
    settings.m_codec             = codec;
    settings.m_bitrate           = bitrateVal;
    settings.m_width             = width;
    settings.m_height            = height;
    settings.m_fps               = static_cast<uint16_t>(fps);
    settings.m_outputFile        = path;
    settings.m_colorspaceFilters = usingApi ? "" : videoArgs;
    settings.m_doVerticalFlip    = true;

#ifdef GEODE_IS_WINDOWS
    subprocess::Popen process;
#endif

    if (usingApi) {
        auto res = ffmpeg.init(settings);
        if (res.isErr()) {
            recording = false;
            m_frameReady.set(true);
            DSPRecorder::get()->stop();
            Loader::get()->queueInMainThread([] {
                FLAlertLayer::create("Error", "FFmpeg API failed to initialize.", "OK")->show();
            });
            return;
        }
    }
#ifdef GEODE_IS_WINDOWS
    else {
        std::string c  = codec.empty()   ? "" : ("-c:v " + codec + " ");
        std::string b  = bitrate.empty() ? "" : ("-b:v " + bitrate + " ");
        std::string ea = extraArgs.empty() ? "-pix_fmt yuv420p" : extraArgs;
        std::string va = videoArgs.empty()
            ? "colorspace=all=bt709:iall=bt470bg:fast=1" : videoArgs;

        float fadeInTime = geode::utils::numFromString<float>(
            mod->getSavedValue<std::string>("render_fade_in_time")).unwrapOr(0.f);
        bool fadeInVideo = mod->getSavedValue<bool>("render_fade_in") && fadeInTime != 0.f;
        std::string fadeArgs;
        if (fadeInVideo)
            fadeArgs = fmt::format(",fade=t=in:st=0:d={}", fadeInTime);

        std::string command = fmt::format(
            "\"{}\" -y -f rawvideo -pix_fmt rgba -s {}x{} -r {} -i - {}{}{} "
            "-vf \"vflip,{}{}\" -an \"{}\" ",
            ffmpegPath,
            geode::utils::numToString(width), geode::utils::numToString(height),
            geode::utils::numToString(fps),
            c, b, ea, va, fadeArgs, path);

        log::info("Renderer: {}", command);
        process = subprocess::Popen(command);
    }
#endif

    m_frameReady.set(false);
    m_frameReady.wait_for(true);

    while (recording) {
        if (usingApi) {
            auto res = ffmpeg.writeFrame(m_currentFrame);
            if (res.isErr()) {
                Loader::get()->queueInMainThread([] {
                    FLAlertLayer::create("Error", "FFmpeg API failed to write frame.", "OK")->show();
                });
                stop();
                break;
            }
        }
#ifdef GEODE_IS_WINDOWS
        else {
            process.m_stdin.write(m_currentFrame.data(), m_currentFrame.size());
        }
#endif

        if (!recording) break;

        m_frameReady.set(false);
        m_frameReady.wait_for(true);
    }

    log::debug("Renderer: record thread stopped.");

    if (usingApi) {
        ffmpeg.stop();
    }
#ifdef GEODE_IS_WINDOWS
    else {
        if (process.close()) {
            DSPRecorder::get()->stop();
            Loader::get()->queueInMainThread([] {
                FLAlertLayer::create("Error",
                    "There was an error saving the render. Wrong render Args.", "OK")->show();
            });
            return;
        }
    }
#endif

    DSPRecorder::get()->stop();
    auto pcm = DSPRecorder::get()->getData();

    Loader::get()->queueInMainThread([] {
        Notification::create("Saving Render...", NotificationIcon::Loading)->show();
    });
    asp::sleep(asp::Duration::fromMillis(100));

    if (audioMode == AudioMode::Off || pcm.empty() ||
        (SFXVolume == 0.f && musicVolume == 0.f)) {
        Loader::get()->queueInMainThread([this] {
            Notification::create("Render Saved Without Audio", NotificationIcon::Success)->show();
            showEndScreenIfNeeded();
        });
        return;
    }

    std::filesystem::path videoPath = path;
    std::filesystem::path tempPath  = videoPath.parent_path() /
        ("temp_" + geode::utils::string::pathToString(videoPath.filename()));

    int sampleRate = 48000, audioChannels = 2;
    if (fmod && fmod->m_system)
        fmod->m_system->getSoftwareFormat(&sampleRate, nullptr, &audioChannels);

    double capturedLastFrame = lastFrame_t;
    size_t expectedSamples   = static_cast<size_t>(
        capturedLastFrame * sampleRate * audioChannels);

    log::info("Renderer: pcm={} expected={} lastFrame_t={:.3f}s",
        pcm.size(), expectedSamples, capturedLastFrame);

    std::span<float> pcmSpan(pcm);
    if (pcmSpan.size() > expectedSamples)
        pcmSpan = pcmSpan.subspan(0, expectedSamples);

    log::info("Renderer: mixing audio, pcm samples={}, video duration={:.3f}s",
        pcmSpan.size(), capturedLastFrame);

    geode::Result<> mixRes = geode::Err("not started");

    if (usingApi) {
        mixRes = ffmpeg::events::AudioMixer::mixVideoRaw(videoPath, pcmSpan, tempPath);
    }
#ifdef GEODE_IS_WINDOWS
    else {
        std::filesystem::path tempWav = Mod::get()->getSaveDir() / "temp_audio_file.wav";
        if (!writePCMWav(tempWav, pcmSpan, fmod ? fmod->m_system : nullptr)) {
            Loader::get()->queueInMainThread([] {
                FLAlertLayer::create("Error",
                    "Failed to write captured audio to WAV.", "OK")->show();
            });
            return;
        }

        if (!extraAudioArgs.empty()) extraAudioArgs += " ";

        std::string cmd = fmt::format(
            "\"{}\" -y -i \"{}\" -i \"{}\" -t {} -c:v copy {}"
            "-filter:a \"[1:a]adelay=0|0\" \"{}\"",
            ffmpegPath,
            geode::utils::string::pathToString(tempWav),
            path, capturedLastFrame, extraAudioArgs,
            geode::utils::string::pathToString(tempPath));

        log::info("Renderer (recorded audio): {}", cmd);
        auto proc = subprocess::Popen(cmd);
        if (proc.close()) mixRes = geode::Err("Wrong Audio Args");
        else              mixRes = geode::Ok();

        std::error_code ec;
        std::filesystem::remove(tempWav, ec);
    }
#endif

    if (mixRes.isErr()) {
        log::error("Renderer: mix failed: {}", mixRes.unwrapErr());
        Loader::get()->queueInMainThread([err = mixRes.unwrapErr()] {
            FLAlertLayer::create("Error",
                fmt::format("Failed to mix audio: {}", err).c_str(), "OK")->show();
        });
        Loader::get()->queueInMainThread([this] { showEndScreenIfNeeded(); });
        return;
    }

    std::error_code ec;
    std::filesystem::remove(videoPath, ec);
    if (ec) {
        log::warn("Renderer: failed to remove original: {}", ec.message());
    } else {
        ec.clear();
        std::filesystem::rename(tempPath, videoPath, ec);
        if (ec) log::warn("Renderer: failed to rename output: {}", ec.message());
    }

    Loader::get()->queueInMainThread([this] {
        Notification::create("Render Saved With Audio", NotificationIcon::Success)->show();
        showEndScreenIfNeeded();
    });
}

static bool writePCMWav(const std::filesystem::path& outPath,
                        std::span<const float> pcm, FMOD::System* system) {
    int sampleRate = 48000, channels = 2;
    if (system) system->getSoftwareFormat(&sampleRate, nullptr, &channels);

    std::vector<int16_t> pcm16(pcm.size());
    for (size_t i = 0; i < pcm.size(); i++) {
        float sample = std::clamp(pcm[i], -1.f, 1.f);
        pcm16[i] = static_cast<int16_t>(sample * 32767.f);
    }

    uint32_t dataSize      = static_cast<uint32_t>(pcm16.size() * sizeof(int16_t));
    uint32_t byteRate      = sampleRate * channels * sizeof(int16_t);
    uint16_t blockAlign    = static_cast<uint16_t>(channels * sizeof(int16_t));
    uint16_t bitsPerSample = 16;
    uint16_t audioFmt      = 1;
    uint16_t ch16          = static_cast<uint16_t>(channels);
    uint32_t sr32          = static_cast<uint32_t>(sampleRate);
    uint32_t riffSize      = 36 + dataSize;
    uint32_t fmtSize       = 16;

    std::string data;
    data.append("RIFF", 4);
    data.append(reinterpret_cast<const char*>(&riffSize),      4);
    data.append("WAVE", 4);
    data.append("fmt ", 4);
    data.append(reinterpret_cast<const char*>(&fmtSize),       4);
    data.append(reinterpret_cast<const char*>(&audioFmt),      2);
    data.append(reinterpret_cast<const char*>(&ch16),          2);
    data.append(reinterpret_cast<const char*>(&sr32),          4);
    data.append(reinterpret_cast<const char*>(&byteRate),      4);
    data.append(reinterpret_cast<const char*>(&blockAlign),    2);
    data.append(reinterpret_cast<const char*>(&bitsPerSample), 2);
    data.append("data", 4);
    data.append(reinterpret_cast<const char*>(&dataSize),      4);
    data.append(reinterpret_cast<const char*>(pcm16.data()),   dataSize);

    auto span = std::span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    return geode::utils::file::writeBinary(outPath, span).isOk();
}

void Renderer::showEndScreenIfNeeded() {
    if (!Mod::get()->getSavedValue<bool>("render_hide_endscreen")) return;
    if (PlayLayer* pl = PlayLayer::get())
        if (EndLevelLayer* layer = pl->getChildByType<EndLevelLayer>(0))
            layer->setVisible(true);
}

#endif
