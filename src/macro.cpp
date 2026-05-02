#include "includes.hpp"
#include "ui/game_ui.hpp"
#include "ui/record_layer.hpp"

#include <Geode/modify/PlayLayer.hpp>

void Macro::recordAction(int frame, int button, bool player2, bool hold) {
    PlayLayer *pl = PlayLayer::get();
    if (!pl)
        return;

    auto &g = Global::get();

    if (g.macro.inputs.empty())
        Macro::updateInfo(pl);

    if (g.tpsEnabled)
        g.macro.framerate = g.tps;

    if (Macro::flipControls())
        player2 = !player2;

    for (int i = (int)g.macro.inputs.size() - 1; i >= 0; i--) {
        auto &last = g.macro.inputs[i];
        if (last.button == button && last.player2 == player2) {
            if (last.down == hold)
                return;
            break;
        }
    }

    if (!hold) {
        bool hasPress = false;
        for (int i = (int)g.macro.inputs.size() - 1; i >= 0; i--) {
            auto &inp = g.macro.inputs[i];
            if (inp.button == button && inp.player2 == player2 && inp.down) {
                hasPress = true;
                break;
            }
        }
        if (!hasPress)
            return;
    }

    g.macro.inputs.push_back(input(frame, button, player2, hold));
}

void Macro::recordFrameFix(int frame, PlayerObject *p1, PlayerObject *p2) {
    float p1Rotation = p1->getRotation();
    float p2Rotation = p2->getRotation();

    while (p1Rotation < 0 || p1Rotation > 360)
        p1Rotation += p1Rotation < 0 ? 360.f : -360.f;

    while (p2Rotation < 0 || p2Rotation > 360)
        p2Rotation += p2Rotation < 0 ? 360.f : -360.f;

    gdr_legacy::FrameData p1Data;
    p1Data.pos = p1->getPosition();
    p1Data.rotation = p1Rotation;
    p1Data.yVelocity = p1->m_yVelocity;
    p1Data.xVelocity = p1->m_platformerXVelocity;
    p1Data.isDashing = p1->m_isDashing;
    p1Data.isOnGround = p1->m_isOnGround;

    gdr_legacy::FrameData p2Data;
    if (p2) {
        p2Data.pos = p2->getPosition();
        p2Data.rotation = p2Rotation;
        p2Data.yVelocity = p2->m_yVelocity;
        p2Data.xVelocity = p2->m_platformerXVelocity;
        p2Data.isDashing = p2->m_isDashing;
        p2Data.isOnGround = p2->m_isOnGround;
    }

    Global::get().macro.frameFixes.push_back({frame, p1Data, p2Data});
}

bool Macro::flipControls() {
    PlayLayer *pl = PlayLayer::get();
    if (!pl)
        return GameManager::get()->getGameVariable(
            GameVar::Flip2PlayerControls);

    return pl->m_isPlatformer ? false
                              : GameManager::get()->getGameVariable(
                                    GameVar::Flip2PlayerControls);
}

void Macro::autoSave(GJGameLevel *level, int number) {
    if (!level)
        level =
            PlayLayer::get() != nullptr ? PlayLayer::get()->m_level : nullptr;
    if (!level)
        return;

    std::string levelname = level->m_levelName;
#ifdef GEODE_IS_IOS
    std::filesystem::path autoSavesPath =
        Mod::get()->getSaveDir() / "autosaves";
#else
    std::filesystem::path autoSavesPath =
        Mod::get()->getSettingValue<std::filesystem::path>("autosaves_folder");
#endif
    std::filesystem::path path =
        autoSavesPath / fmt::format("autosave_{}_{}", levelname, number);

    if (!std::filesystem::exists(autoSavesPath))
        return;

    std::string username = GJAccountManager::sharedState() != nullptr
                               ? GJAccountManager::sharedState()->m_username
                               : "";
    int result = Macro::save(
        username, fmt::format("AutoSave {} in level {}", number, levelname),
        geode::utils::string::pathToString(path));

    if (result != 0)
        log::debug("Failed to autosave macro. ID: {}. Path: {}", result, path);
}

void Macro::tryAutosave(GJGameLevel *level, CheckpointObject *cp) {
    auto &g = Global::get();

    if (g.state != state::recording)
        return;
    if (!g.autosaveEnabled)
        return;
    if (!g.checkpoints.contains(cp))
        return;

#ifdef GEODE_IS_IOS
    std::filesystem::path autoSavesPath = g.mod->getSaveDir() / "autosaves";
#else
    std::filesystem::path autoSavesPath =
        g.mod->getSettingValue<std::filesystem::path>("autosaves_folder");
#endif

    if (!std::filesystem::exists(autoSavesPath))
        return log::debug("Failed to access auto saves path.");

    std::string levelname = level->m_levelName;
    std::filesystem::path path =
        autoSavesPath /
        fmt::format("autosave_{}_{}", levelname, g.currentSession);
    std::error_code ec;
    std::filesystem::remove(geode::utils::string::pathToString(path) + ".gdr",
                            ec);
    if (ec)
        log::warn("Failed to remove previous autosave");

    autoSave(level, g.currentSession);
}

void Macro::updateInfo(PlayLayer *pl) {
    if (!pl)
        return;

    auto &g = Global::get();

    GJGameLevel *level = pl->m_level;
    if (level->m_lowDetailModeToggled != g.macro.ldm)
        g.macro.ldm = level->m_lowDetailModeToggled;

    int id = level->m_levelID.value();
    if (static_cast<int>(g.macro.levelInfo.id) != id)
        g.macro.levelInfo.id = id;

    std::string name = level->m_levelName;
    if (name != g.macro.levelInfo.name)
        g.macro.levelInfo.name = name;

    std::string author = GJAccountManager::sharedState()->m_username;
    if (g.macro.author != author)
        g.macro.author = author;

    if (g.macro.author == "")
        g.macro.author = "N/A";

    g.macro.botInfo.name = "xdBot";
    g.macro.botInfo.version = getModVersionInt();
    g.macro.xdBotMacro = true;
}

void Macro::updateTPS() {
    auto &g = Global::get();

    if (g.state != state::none && !g.macro.inputs.empty()) {
        g.previousTpsEnabled = g.tpsEnabled;
        g.previousTps = g.tps;

        g.setTpsEnabled(g.macro.framerate != 240.f);
        if (g.tpsEnabled)
            g.setTps(g.macro.framerate);

    } else if (g.previousTps != 0.f) {
        g.setTpsEnabled(g.previousTpsEnabled);
        g.setTps(g.previousTps);
        g.previousTps = 0.f;
    }

    if (g.layer)
        static_cast<RecordLayer *>(g.layer)->updateTPS();
}

LegacyMacro Macro::toLegacy() const {
    LegacyMacro legacy;
    legacy.author = author;
    legacy.description = description;
    legacy.duration = duration;
    legacy.gameVersion = static_cast<float>(gameVersion) / 1000.0f;
    legacy.framerate = framerate;
    legacy.seed = seed;
    legacy.coins = coins;
    legacy.ldm = ldm;
    legacy.botInfo.name = botInfo.name;
    legacy.botInfo.version = getModVersionString();
    legacy.levelInfo.id = levelInfo.id;
    legacy.levelInfo.name = levelInfo.name;

    for (const auto &inp : inputs) {
        legacy.inputs.push_back(legacy_input(
            static_cast<int>(inp.frame), inp.button, inp.player2, inp.down));
    }

    legacy.frameFixes = frameFixes;

    return legacy;
}

Macro Macro::fromLegacy(const LegacyMacro &legacy) {
    Macro macro;
    macro.author = legacy.author;
    macro.description = legacy.description;
    macro.duration = legacy.duration;
    macro.gameVersion =
        static_cast<int>(std::round(legacy.gameVersion * 1000.0f));
    macro.framerate = legacy.framerate;
    macro.seed = legacy.seed;
    macro.coins = legacy.coins;
    macro.ldm = legacy.ldm;
    macro.botInfo.name = legacy.botInfo.name;
    std::string versionStr = legacy.botInfo.version;
    auto firstSpace = versionStr.find_first_of(" \n\r\t");
    if (firstSpace != std::string::npos)
        versionStr = versionStr.substr(0, firstSpace);

    float parsedVer =
        geode::utils::numFromString<float>(versionStr).unwrapOr(0.f);
    macro.botInfo.version = static_cast<int>(std::round(parsedVer * 1000.f));
    macro.isLegacy =
        macro.botInfo.name == "xdBot" && macro.botInfo.version < 2208;
    macro.levelInfo.id = legacy.levelInfo.id;
    macro.levelInfo.name = legacy.levelInfo.name;
    macro.frameFixes = legacy.frameFixes;

    for (const auto &inp : legacy.inputs) {
        macro.inputs.push_back(input(static_cast<uint64_t>(inp.frame),
                                     static_cast<uint8_t>(inp.button),
                                     inp.player2, inp.down));
    }

    macro.xdBotMacro = legacy.botInfo.name == "xdBot";
    macro.isLegacy = true;

    return macro;
}

gdr::Result<std::vector<uint8_t>> Macro::exportGDR2() {
    return gdr::Replay<Macro, input>::exportData();
}

std::vector<uint8_t> Macro::exportGDR1() {
    LegacyMacro legacy = toLegacy();
    return legacy.exportData(false);
}

std::vector<uint8_t> Macro::exportJSON() {
    LegacyMacro legacy = toLegacy();
    return legacy.exportData(true);
}

void Macro::saveExtension(binary_writer &writer) const {
    writer << static_cast<uint64_t>(frameFixes.size());

    for (const auto &fix : frameFixes) {
        writer << static_cast<uint64_t>(fix.frame);
        writer << fix.p1.pos.x;
        writer << fix.p1.pos.y;
        writer << fix.p1.rotation;
        writer << fix.p1.rotate;
        writer << fix.p2.pos.x;
        writer << fix.p2.pos.y;
        writer << fix.p2.rotation;
        writer << fix.p2.rotate;
    }
}

void Macro::parseExtension(binary_reader &reader) {
    uint64_t count = 0;
    reader >> count;

    frameFixes.clear();
    frameFixes.reserve(count);

    for (uint64_t i = 0; i < count; i++) {
        gdr_legacy::FrameFix fix;
        uint64_t frame = 0;
        reader >> frame;
        fix.frame = static_cast<int>(frame);

        reader >> fix.p1.pos.x;
        reader >> fix.p1.pos.y;
        reader >> fix.p1.rotation;
        reader >> fix.p1.rotate;
        reader >> fix.p2.pos.x;
        reader >> fix.p2.pos.y;
        reader >> fix.p2.rotation;
        reader >> fix.p2.rotate;

        frameFixes.push_back(fix);
    }
}

Macro Macro::importData(std::vector<uint8_t> &data) {
    if (data.size() >= 3 && data[0] == 'G' && data[1] == 'D' &&
        data[2] == 'R') {
        std::span<uint8_t> span(data.data(), data.size());
        auto result = gdr::Replay<Macro, input>::importData(span);
        if (result.isOk()) {
            Macro macro = std::move(result).unwrap();
            macro.xdBotMacro = macro.botInfo.name == "xdBot";
            return macro;
        }
        log::warn("GDR2 import failed: {}", result.unwrapErr());
    }

    {
        std::span<uint8_t const> span(data.data(), data.size());
        auto result = gdr::convert<Macro, input>(span);
        if (result.isOk()) {
            Macro macro = std::move(result).unwrap();
            macro.xdBotMacro = macro.botInfo.name == "xdBot";
            return macro;
        }
        log::warn("GDR converter import failed: {}", result.unwrapErr());
    }

    {
        LegacyMacro legacy = LegacyMacro::importData(data);
        if (!legacy.inputs.empty() || !legacy.botInfo.name.empty()) {
            Macro macro = fromLegacy(legacy);
            return macro;
        }
    }

    log::error("Failed to import macro data in any format");
    return Macro();
}

int Macro::save(std::string author, std::string desc, std::string path,
                SaveFormat format) {
    auto &g = Global::get();

    if (g.macro.inputs.empty())
        return 31;

    std::string extension;
    switch (format) {
    case SaveFormat::GDR2:
        extension = ".gdr2";
        break;
    case SaveFormat::GDR1:
        extension = ".gdr";
        break;
    case SaveFormat::JSON:
        extension = ".gdr.json";
        break;
    }

    int iterations = 0;

    while (std::filesystem::exists(path + extension)) {
        iterations++;

        if (iterations > 1) {
            int length = 3 + geode::utils::numToString(iterations - 1).length();
            path.erase(path.length() - length, length);
        }

        path += fmt::format(" ({})", geode::utils::numToString(iterations));
    }

    path += extension;

    log::debug("Saving macro to path: {}", path);

    g.macro.author = author;
    g.macro.description = desc;
    g.macro.duration = g.macro.inputs.back().frame / g.macro.framerate;

    std::vector<uint8_t> data;

    switch (format) {
    case SaveFormat::GDR2: {
        auto result = g.macro.exportGDR2();
        if (result.isErr()) {
            log::error("GDR2 export failed: {}", result.unwrapErr());
            return 23;
        }
        data = std::move(result).unwrap();
        break;
    }
    case SaveFormat::GDR1:
        data = g.macro.exportGDR1();
        break;
    case SaveFormat::JSON:
        data = g.macro.exportJSON();
        break;
    }

    if (data.empty())
        return 23;

    auto writeResult = geode::utils::file::writeBinary(path, data);
    if (writeResult.isErr()) {
        log::error("Failed to write file: {}", writeResult.unwrapErr());
        return 20;
    }

    return 0;
}

bool Macro::loadXDFile(std::filesystem::path path) {
    Macro newMacro = Macro::XDtoGDR(path);
    if (newMacro.description == "fail")
        return false;

    Global::get().macro = newMacro;
    return true;
}

Macro Macro::XDtoGDR(std::filesystem::path path) {
    Macro newMacro;
    newMacro.author = "N/A";
    newMacro.description = "N/A";
    newMacro.gameVersion = static_cast<int>(
        std::round(static_cast<double>(GEODE_GD_VERSION) * 1000.0));

    auto readResult = geode::utils::file::readString(path);
    if (readResult.isErr()) {
        newMacro.description = "fail";
        return newMacro;
    }

    float fpsMultiplier = 1.f;

    auto lines = geode::utils::string::split(readResult.unwrap(), "\n");
    for (auto &line : lines) {
        auto action = geode::utils::string::split(line, "|");

        if (action.empty())
            continue;

        if (action.size() < 4) {
            if (action[0] == "android")
                fpsMultiplier = 4.f;
            else
                fpsMultiplier =
                    240.f /
                    geode::utils::numFromString<float>(action[0]).unwrapOr(
                        240.f);
            continue;
        }

        int frame = static_cast<int>(
            round(geode::utils::numFromString<float>(action[0]).unwrapOr(0.f) *
                  fpsMultiplier));
        int button = geode::utils::numFromString<int>(action[2]).unwrapOr(0);
        bool hold = action[1] == "1";
        bool player2 = action[3] == "1";
        bool posOnly = action.size() > 4 && action[4] == "1";

        if (!posOnly)
            newMacro.inputs.push_back(input(frame, button, player2, hold));
        else {
            cocos2d::CCPoint p1Pos = ccp(
                geode::utils::numFromString<float>(action[5]).unwrapOr(0.f),
                geode::utils::numFromString<float>(action[6]).unwrapOr(0.f));
            cocos2d::CCPoint p2Pos = ccp(
                geode::utils::numFromString<float>(action[11]).unwrapOr(0.f),
                geode::utils::numFromString<float>(action[12]).unwrapOr(0.f));
            newMacro.frameFixes.push_back(
                {frame, {p1Pos, 0.f, false}, {p2Pos, 0.f, false}});
        }
    }

    return newMacro;
    newMacro.xdBotMacro = true;
    newMacro.isLegacy = true;
}

void Macro::resetState(bool cp) {
    auto &g = Global::get();

    g.restart = false;
    g.state = state::none;

    if (!cp)
        g.checkpoints.clear();

    Interface::updateLabels();
    Interface::updateButtons();
}

void Macro::togglePlaying() {
    if (Global::hasIncompatibleMods() ||
        Global::enabledIncompatibleGDSettings())
        return;

    auto &g = Global::get();

    if (g.layer) {
        static_cast<RecordLayer *>(g.layer)->playing->toggle(
            Global::get().state != state::playing);
        static_cast<RecordLayer *>(g.layer)->togglePlaying(nullptr);
    } else {
        RecordLayer *layer = RecordLayer::create();
        layer->togglePlaying(nullptr);
        layer->onClose(nullptr);
    }
}

void Macro::toggleRecording() {
    if (Global::hasIncompatibleMods() ||
        Global::enabledIncompatibleGDSettings())
        return;

    auto &g = Global::get();

    if (g.layer) {
        static_cast<RecordLayer *>(g.layer)->recording->toggle(
            Global::get().state != state::recording);
        static_cast<RecordLayer *>(g.layer)->toggleRecording(nullptr);
    } else {
        RecordLayer *layer = RecordLayer::create();
        layer->toggleRecording(nullptr);
        layer->onClose(nullptr);
    }
}

bool Macro::shouldStep() {
    auto &g = Global::get();

    if (g.stepFrame)
        return true;
    if (Global::getCurrentFrame() == 0)
        return true;

    return false;
}
