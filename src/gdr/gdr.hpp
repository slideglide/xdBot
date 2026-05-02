#pragma once

#include <Geode/loader/Mod.hpp>

#include <vector>

#include <matjson.hpp>
#include <matjson/msgpack.hpp>

#include <Geode/utils/VersionInfo.hpp>

inline std::string getModVersionString() {
    return geode::Mod::get()->getVersion().toVString();
}

inline int getModVersionInt() {
    return static_cast<int>(geode::Mod::get()->getVersion().getMajor());
}

geode::prelude::VersionInfo getVersion(std::vector<std::string> nums);

cocos2d::CCPoint dataFromString(std::string dataString);

namespace gdr_legacy {

struct Bot {
    std::string name;
    std::string version;

    inline Bot(std::string const &name, std::string const &version)
        : name(name), version(version) {}
};

struct Level {
    uint32_t id;
    std::string name;

    Level() = default;

    inline Level(std::string const &name, uint32_t id = 0)
        : name(name), id(id) {}
};

struct FrameData {
    cocos2d::CCPoint pos = {0.f, 0.f};
    float rotation = 0.f;
    bool rotate = true;
    double yVelocity = 0.0;
    double xVelocity = 0.0;
    bool isDashing = false;
    bool isOnGround = false;
};

struct FrameFix {
    int frame;
    FrameData p1;
    FrameData p2;
};

class Input {
  protected:
    Input() = default;
    template <typename, typename> friend class Replay;

  public:
    uint32_t frame;
    int button;
    bool player2;
    bool down;

    inline virtual void parseExtension(matjson::Value const &obj) {}
    inline virtual matjson::Value saveExtension() const {
        return matjson::makeObject({});
    }

    inline Input(int frame, int button, bool player2, bool down)
        : frame(frame), button(button), player2(player2), down(down) {}

    inline static Input hold(int frame, int button, bool player2 = false) {
        return Input(frame, button, player2, true);
    }

    inline static Input release(int frame, int button, bool player2 = false) {
        return Input(frame, button, player2, false);
    }

    inline bool operator<(Input const &other) const {
        return frame < other.frame;
    }
};

template <typename S = void, typename T = Input> class Replay {
    Replay() = default;

  public:
    using InputType = T;
    using Self = std::conditional_t<std::is_same_v<S, void>, Replay<S, T>, S>;

    std::string author;
    std::string description;

    float duration;
    float gameVersion;
    float version = 1.0;

    float framerate = 240.f;

    int seed = 0;
    int coins = 0;

    bool ldm = false;

    Bot botInfo;
    Level levelInfo;

    std::vector<InputType> inputs;
    std::vector<FrameFix> frameFixes;

    uint32_t frameForTime(double time) {
        return static_cast<uint32_t>(time * (double)framerate);
    }

    virtual void parseExtension(matjson::Value const &obj) {}
    virtual matjson::Value saveExtension() const {
        return matjson::makeObject({});
    }

    Replay(std::string const &botName, std::string const &botVersion)
        : botInfo(botName, botVersion) {}

    static Self importData(std::vector<uint8_t> const &data,
                           bool importInputs = true) {
        Self replay;

        auto parseResult =
            matjson::msgpack::parse(std::span<const uint8_t>(data));
        if (!parseResult) {
            parseResult = matjson::parse(std::string_view(
                reinterpret_cast<const char *>(data.data()), data.size()));
            if (!parseResult)
                return replay;
        }

        matjson::Value const &replayJson = parseResult.unwrap();

        if (auto v = replayJson["gameVersion"].asDouble())
            replay.gameVersion = static_cast<float>(v.unwrap());
        if (auto v = replayJson["description"].asString())
            replay.description = v.unwrap();
        if (auto v = replayJson["version"].asDouble())
            replay.version = static_cast<float>(v.unwrap());
        if (auto v = replayJson["duration"].asDouble())
            replay.duration = static_cast<float>(v.unwrap());
        if (auto v = replayJson["author"].asString())
            replay.author = v.unwrap();
        if (auto v = replayJson["seed"].asInt())
            replay.seed = static_cast<int>(v.unwrap());
        if (auto v = replayJson["coins"].asInt())
            replay.coins = static_cast<int>(v.unwrap());
        if (auto v = replayJson["ldm"].asBool())
            replay.ldm = v.unwrap();
        if (auto v = replayJson["bot"]["name"].asString())
            replay.botInfo.name = v.unwrap();
        if (auto v = replayJson["bot"]["version"].asString())
            replay.botInfo.version = v.unwrap();
        if (auto v = replayJson["level"]["id"].asInt())
            replay.levelInfo.id = static_cast<uint32_t>(v.unwrap());
        if (auto v = replayJson["level"]["name"].asString())
            replay.levelInfo.name = v.unwrap();
        if (auto v = replayJson["framerate"].asDouble())
            replay.framerate = static_cast<float>(v.unwrap());

        std::string_view ver = replay.botInfo.version;
        bool rotation = ver.find("beta.") == std::string_view::npos &&
                        ver.find("alpha.") == std::string_view::npos;
        if (replay.botInfo.name == "xdBot" && ver == "v2.0.0")
            rotation = true;

        int offset = replay.botInfo.name == "xdBot" ? 1 : 0;
        if (offset == 1) {
            if (!ver.empty() && ver.front() == 'v')
                ver = ver.substr(1);
            auto splitVer = geode::utils::string::split(std::string(ver), ".");
            if (splitVer.size() <= 3) {
                if (getVersion(splitVer) >= getVersion({"2", "3", "6"}))
                    offset = 0;
            }
        }

        replay.parseExtension(replayJson);

        if (!importInputs)
            return replay;

        if (replayJson.contains("inputs") && replayJson["inputs"].isArray()) {
            for (auto const &inputJson :
                 replayJson["inputs"].asArray().unwrap()) {
                if (!inputJson.contains("frame"))
                    continue;
                auto frameVal = inputJson["frame"].asInt();
                if (!frameVal)
                    continue;

                InputType input;
                input.frame = static_cast<uint32_t>(frameVal.unwrap()) + offset;
                if (auto v = inputJson["btn"].asInt())
                    input.button = static_cast<int>(v.unwrap());
                if (auto v = inputJson["2p"].asBool())
                    input.player2 = v.unwrap();
                if (auto v = inputJson["down"].asBool())
                    input.down = v.unwrap();
                input.parseExtension(inputJson);
                replay.inputs.push_back(std::move(input));
            }
        }

        if (!replayJson.contains("frameFixes") ||
            !replayJson["frameFixes"].isArray())
            return replay;

        for (auto const &frameFixJson :
             replayJson["frameFixes"].asArray().unwrap()) {
            if (!frameFixJson.contains("frame"))
                continue;
            auto frameVal = frameFixJson["frame"].asInt();
            if (!frameVal)
                continue;

            FrameFix frameFix;
            frameFix.frame = static_cast<int>(frameVal.unwrap()) + offset;

            if (frameFixJson.contains("player1")) {
                if (auto v = frameFixJson["player1"].asString())
                    frameFix.p1.pos = dataFromString(v.unwrap());
                frameFix.p1.rotate = false;
                if (auto v = frameFixJson["player2"].asString())
                    frameFix.p2.pos = dataFromString(v.unwrap());
                frameFix.p2.rotate = false;
            } else if (frameFixJson.contains("player1X")) {
                frameFix.p1.pos =
                    ccp(static_cast<float>(
                            frameFixJson["player1X"].asDouble().unwrapOr(0.0)),
                        static_cast<float>(
                            frameFixJson["player1Y"].asDouble().unwrapOr(0.0)));
                frameFix.p2.pos =
                    ccp(static_cast<float>(
                            frameFixJson["player2X"].asDouble().unwrapOr(0.0)),
                        static_cast<float>(
                            frameFixJson["player2Y"].asDouble().unwrapOr(0.0)));
                frameFix.p1.rotate = false;
                frameFix.p2.rotate = false;
            } else if (frameFixJson.contains("p1")) {
                if (replay.botInfo.name != "xdBot")
                    rotation = false;
                auto const &p1 = frameFixJson["p1"];
                if (p1.contains("x"))
                    frameFix.p1.pos.x =
                        static_cast<float>(p1["x"].asDouble().unwrapOr(0.0));
                if (p1.contains("y"))
                    frameFix.p1.pos.y =
                        static_cast<float>(p1["y"].asDouble().unwrapOr(0.0));
                if (p1.contains("r") && rotation)
                    frameFix.p1.rotation =
                        static_cast<float>(p1["r"].asDouble().unwrapOr(0.0));
                if (frameFixJson.contains("p2")) {
                    auto const &p2 = frameFixJson["p2"];
                    if (p2.contains("x"))
                        frameFix.p2.pos.x = static_cast<float>(
                            p2["x"].asDouble().unwrapOr(0.0));
                    if (p2.contains("y"))
                        frameFix.p2.pos.y = static_cast<float>(
                            p2["y"].asDouble().unwrapOr(0.0));
                    if (p2.contains("r") && rotation)
                        frameFix.p2.rotation = static_cast<float>(
                            p2["r"].asDouble().unwrapOr(0.0));
                }
            } else
                continue;

            replay.frameFixes.push_back(std::move(frameFix));
        }

        return replay;
    }

    std::vector<uint8_t> exportData(bool exportJson = false) {
        matjson::Value replayJson = saveExtension();

        replayJson["gameVersion"] = gameVersion;
        replayJson["description"] = description;
        replayJson["version"] = version;
        replayJson["duration"] = duration;
        replayJson["bot"]["name"] = botInfo.name;
        replayJson["bot"]["version"] = botInfo.version;
        replayJson["level"]["id"] = static_cast<int>(levelInfo.id);
        replayJson["level"]["name"] = levelInfo.name;
        replayJson["author"] = author;
        replayJson["seed"] = seed;
        replayJson["coins"] = coins;
        replayJson["ldm"] = ldm;
        replayJson["framerate"] = framerate;

        auto inputsArr = std::vector<matjson::Value>{};
        inputsArr.reserve(inputs.size());
        for (InputType const &inp : inputs) {
            matjson::Value inputJson = inp.saveExtension();
            inputJson["frame"] = static_cast<int>(inp.frame);
            inputJson["btn"] = inp.button;
            inputJson["2p"] = inp.player2;
            inputJson["down"] = inp.down;
            inputsArr.push_back(std::move(inputJson));
        }
        replayJson["inputs"] = std::move(inputsArr);

        auto frameFixesArr = std::vector<matjson::Value>{};
        frameFixesArr.reserve(frameFixes.size());
        for (FrameFix const &fix : frameFixes) {
            matjson::Value p1Json = matjson::makeObject({});
            matjson::Value p2Json = matjson::makeObject({});

            if (fix.p1.pos.x != 0.f)
                p1Json["x"] = fix.p1.pos.x;
            if (fix.p1.pos.y != 0.f)
                p1Json["y"] = fix.p1.pos.y;
            if (fix.p1.rotation != 0.f)
                p1Json["r"] = fix.p1.rotation;

            if (fix.p2.pos.x != 0.f)
                p2Json["x"] = fix.p2.pos.x;
            if (fix.p2.pos.y != 0.f)
                p2Json["y"] = fix.p2.pos.y;
            if (fix.p2.rotation != 0.f)
                p2Json["r"] = fix.p2.rotation;

            if (p1Json.size() == 0 && p2Json.size() == 0)
                continue;

            matjson::Value frameFixJson = matjson::makeObject({});
            frameFixJson["frame"] = fix.frame;
            frameFixJson["p1"] = std::move(p1Json);
            if (fix.p2.pos.y != 0.f)
                frameFixJson["p2"] = std::move(p2Json);

            frameFixesArr.push_back(std::move(frameFixJson));
        }
        replayJson["frameFixes"] = std::move(frameFixesArr);

        if (exportJson) {
            std::string str = replayJson.dump(matjson::NO_INDENTATION);
            return std::vector<uint8_t>(str.begin(), str.end());
        }

        return matjson::msgpack::serialize(replayJson);
    }
};

} // namespace gdr_legacy