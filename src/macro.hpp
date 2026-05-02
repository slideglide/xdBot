#pragma once

#include "gdr/gdr.hpp"
#include "practice_fixes/checkpoint.hpp"
#include "utils/utils.hpp"

#include <gdr/gdr.hpp>
#include <gdr_convert.hpp>

using namespace geode::prelude;

#define DIF(a, b) (std::fabs((a) - (b)) > 0.001f)

const std::vector<float> safeValues = {
    1.0f / 240, 1.0f / 120, 1.0f / 80, 1.0f / 60, 1.0f / 48,
    1.0f / 40,  1.0f / 30,  1.0f / 24, 1.0f / 20, 1.0f / 16,
    1.0f / 15,  1.0f / 12,  1.0f / 10, 1.0f / 8,  1.0f / 6,
    1.0f / 5,   1.0f / 4,   1.0f / 3,  1.0f / 2};

enum state { none, recording, playing };

enum class SaveFormat { GDR2, GDR1, JSON };

struct input : gdr::Input<> {
    input() = default;

    input(uint64_t frame, uint8_t button, bool player2, bool down)
        : Input(frame, button, player2, down) {}

    bool operator==(const input &other) const {
        return frame == other.frame && player2 == other.player2 &&
               button == other.button && down == other.down;
    }

    bool operator<(const input &other) const { return frame < other.frame; }
};

struct legacy_input : gdr_legacy::Input {
    legacy_input() = default;

    legacy_input(int frame, int button, bool player2, bool down)
        : Input(frame, button, player2, down) {}
};

struct LegacyMacro : gdr_legacy::Replay<LegacyMacro, legacy_input> {
    LegacyMacro() : Replay("xdBot", getModVersionString()) {}
};

struct Macro : gdr::Replay<Macro, input> {

    Macro() : Replay("xdBot", 1) { botInfo.name = "xdBot"; }

  public:
    bool canChangeFPS = true;
    uintptr_t seed = 0;
    bool xdBotMacro = true;
    bool isLegacy = false;

    std::vector<gdr_legacy::FrameFix> frameFixes;

    bool shouldParseExtension() const override {
        return botInfo.name == "xdBot";
    }

    void parseExtension(binary_reader &reader) override;
    void saveExtension(binary_writer &writer) const override;

    std::string getBotVersionString() const { return getModVersionString(); }

    LegacyMacro toLegacy() const;

    static Macro fromLegacy(const LegacyMacro &legacy);

    gdr::Result<std::vector<uint8_t>> exportGDR2();

    std::vector<uint8_t> exportGDR1();

    std::vector<uint8_t> exportJSON();

    static Macro importData(std::vector<uint8_t> &data);

    static void recordAction(int frame, int button, bool player2, bool hold);

    static void recordFrameFix(int frame, PlayerObject *p1, PlayerObject *p2);

    static int save(std::string author, std::string desc, std::string path,
                    SaveFormat format = SaveFormat::GDR2);

    static void autoSave(GJGameLevel *level, int number);

    static void tryAutosave(GJGameLevel *level, CheckpointObject *cp);

    static void updateInfo(PlayLayer *pl);

    static void updateTPS();

    static bool loadXDFile(std::filesystem::path path);

    static Macro XDtoGDR(std::filesystem::path path);

    static void resetState(bool cp = false);

    static void togglePlaying();

    static void toggleRecording();

    static bool shouldStep();

    static bool flipControls();
};

struct button {
    int button;
    bool player2;
    bool down;
};
struct CheckpointData {
    int frame;
    SupplementalPlayerState p1;
    SupplementalPlayerState p2;
    uintptr_t seed;
    int previousFrame;
};