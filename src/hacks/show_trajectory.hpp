#pragma once

#include <Geode/Geode.hpp>

#include "../practice_fixes/practice_fixes.hpp"

class TrajectoryNode : public cocos2d::CCDrawNode {
public:
    static TrajectoryNode* create() {
        TrajectoryNode* ret = new TrajectoryNode();
        if (ret->init()) {
            ret->autorelease();
            ret->m_bUseArea = false;
            return ret;
        }

        delete ret;
        return nullptr;

    }
};

class ShowTrajectory {

public:
    struct PredictionConfig {
        bool applyReplayInputs = true;
        bool draw = true;
        int maxLength = 10'000'000;
    };

    struct PredictionResult {
        cocos2d::CCPoint position = {};
        cocos2d::CCRect hitbox = {};
        cocos2d::CCRect innerHitbox = {};
        float rotation = 0.f;
        bool player1 = true;
        bool holding = false;
        int score = 0;
    };

    enum Mode : int {
        Hold = 1 << 0,
        Swift = 1 << 1,
        Release = 1 << 2,
        Left = 1 << 3,
        Right = 1 << 4,
    };


    static auto& get() {
        static ShowTrajectory instance;
        return instance;
    }


    void updateMergedColor();

    void setColor1(cocos2d::ccColor3B color);

    void setColor2(cocos2d::ccColor3B color);

    static void trajectoryOff();

    static cocos2d::CCDrawNode* trajectoryNode(bool create = true);
    static void detachTrajectoryNode();

    static void updateTrajectory(PlayLayer* pl);

    static void refreshIfNeeded();

    static PredictionResult simulate(
        PlayLayer* pl,
        bool player1,
        int mode,
        bool simulateBothPlayers,
        PredictionConfig config
    );

    static CCDrawNode* createNode();

    static PredictionResult createTrajectory(
        PlayLayer* pl,
        bool player1,
        int mode,
        bool simulateBothPlayers,
        GJGameState const& baseGameState,
        EffectManagerState* baseEffectState,
        PredictionConfig config
    );
    static bool iterate(
        PlayLayer* pl,
        PlayerObject* player,
        int mode,
        cocos2d::ccColor4F color,
        PredictionConfig const& config
    );
    static void applyInitialInput(PlayLayer* pl, PlayerObject* player, PlayerObject* realPlayer, int mode);
    static PlayerObject* ensureFakePlayer(PlayLayer* pl, bool player1);
    static void hideFakePlayer(PlayerObject* player);
    static void releaseFakePlayers();

    static void drawPlayerHitbox(PlayerObject* player, CCDrawNode* drawNode);

    static std::vector<cocos2d::CCPoint> getVertices(PlayerObject* player, cocos2d::CCRect rect, float rotation);

    static void handlePortal(PlayerObject* player, int id);

    static bool isFakePlayer(PlayerObject* player);

    static bool hasActivated(PlayerObject* player, EnhancedGameObject* object);

    static bool realPlayerHasActivated(PlayerObject* player, EnhancedGameObject* object);

    static void rememberActivated(PlayerObject* player, EnhancedGameObject* object);

    static void snapshotObject(EffectGameObject* object);

    static void restoreSnapshots();

    static cocos2d::ccColor3B ccc3BFromccc4F(cocos2d::ccColor4F color) {
        return ccc3((int)(color.r * 255), (int)(color.g * 255), (int)(color.b * 255));
    }

    PlayerObject* fakePlayer1 = nullptr;
    PlayerObject* fakePlayer2 = nullptr;

    bool creatingTrajectory = false;
    bool cancelTrajectory = false;
    bool deadP1 = false;
    bool deadP2 = false;
    bool miniScale = false;
    
    struct ObjSnapshot {
        EffectGameObject* obj;
        RingObject*       ring;
        bool isActivated;
        bool activatedByP1;
        bool activatedByP2;
        bool claimTouch;
        bool isDisabled;
        bool isDisabled2;
    };

    std::vector<ObjSnapshot> objSnapshot;
    std::unordered_set<uintptr_t> snapshotObjects;
    std::unordered_map<uintptr_t, bool> activatedObjectsP1;
    std::unordered_map<uintptr_t, bool> activatedObjectsP2;

    float deathRotation = 0.f;
    float delta = 0.25f;

    int length = 312;
    float width = 0.5f;

    cocos2d::ccColor4F color1;
    cocos2d::ccColor4F color2;
    cocos2d::ccColor4F color3;

    std::vector<cocos2d::CCPoint> player1Trajectory;
    std::vector<cocos2d::CCPoint> player2Trajectory;

};
