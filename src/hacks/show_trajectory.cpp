#include "show_trajectory.hpp"

#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

#include "../physics/trajectory_physics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

ShowTrajectory& t = ShowTrajectory::get();

namespace {
PlayerObject* physicsFakePlayer1() {
    return t.fakePlayer1;
}

PlayerObject* physicsFakePlayer2() {
    return t.fakePlayer2;
}

xdbot::trajectory_physics::Context showTrajectoryPhysicsContext {
    physicsFakePlayer1,
    physicsFakePlayer2,
    ShowTrajectory::isFakePlayer,
    ShowTrajectory::hasActivated,
    ShowTrajectory::realPlayerHasActivated,
    ShowTrajectory::snapshotObject,
    ShowTrajectory::rememberActivated,
};

uint64_t quantize(float value) {
    return static_cast<uint64_t>(std::llround(static_cast<double>(value) * 1000.0));
}

uint64_t trajectoryRefreshSignature(Bot const& bot, PlayLayer* pl) {
    uint64_t signature = 1469598103934665603ull;
    auto mix = [&signature](uint64_t value) {
        signature ^= value + 0x9e3779b97f4a7c15ull + (signature << 6) + (signature >> 2);
    };
    auto mixBool = [&mix](bool value) {
        mix(value ? 1ull : 0ull);
    };
    auto mixFloat = [&mix](float value) {
        mix(quantize(value));
    };
    auto mixColor = [&mixFloat](cocos2d::ccColor4F const& color) {
        mixFloat(color.r);
        mixFloat(color.g);
        mixFloat(color.b);
        mixFloat(color.a);
    };
    auto mixPlayer = [&](PlayerObject* player) {
        mix(reinterpret_cast<uintptr_t>(player));
        if (!player)
            return;

        mixFloat(player->getPositionX());
        mixFloat(player->getPositionY());
        mixFloat(player->getRotation());
        mixFloat(static_cast<float>(player->m_yVelocity));
        mixFloat(static_cast<float>(player->m_platformerXVelocity));
        mixBool(player->m_jumpBuffered);
        mixBool(player->m_holdingButtons[1]);
        mixBool(player->m_holdingButtons[2]);
        mixBool(player->m_holdingButtons[3]);
        mixBool(player->m_holdingLeft);
        mixBool(player->m_holdingRight);
        mixBool(player->m_isDead);
        mixBool(player->m_isDashing);
        mixBool(player->m_isUpsideDown);
        mixBool(player->m_isGoingLeft);
    };

    auto& trajectory = ShowTrajectory::get();
    mix(reinterpret_cast<uintptr_t>(pl));
    mix(static_cast<uint64_t>(Bot::getCurrentFrame()));
    mix(static_cast<uint64_t>(pl->m_gameState.m_currentProgress));
    mixFloat(pl->m_gameState.m_timeWarp);
    mixBool(pl->m_gameState.m_isDualMode);
    mixBool(pl->m_isPaused);
    mixBool(pl->m_isPracticeMode);
    mixBool(pl->m_isPlatformer);
    mixBool(pl->m_levelSettings && pl->m_levelSettings->m_twoPlayerMode);
    mixBool(pl->m_levelSettings && pl->m_levelSettings->m_platformerMode);
    mixBool(bot.trajectoryBothSides);
    mix(static_cast<uint64_t>(std::max(trajectory.length, 0)));
    mixFloat(Bot::getTPS());
    mixColor(trajectory.color1);
    mixColor(trajectory.color2);
    mixColor(trajectory.color3);
    mixPlayer(pl->m_player1);
    mixPlayer(pl->m_player2);
    return signature;
}

void applyReplayButton(PlayerObject* player, ReplayInput const& input) {
    if (!player)
        return;

    auto button = static_cast<PlayerButton>(input.button);
    if (input.down) {
        player->pushButton(button);
    } else {
        player->releaseButton(button);
    }
}

void applyReplayInputsForPrediction(
    PlayLayer* pl,
    size_t& inputIndex,
    uint64_t frame,
    bool simulateBothPlayers,
    bool enabled
) {
    auto& bot = Bot::get();
    if (!enabled || bot.state != state::playing || !pl)
        return;

    auto const& inputs = bot.replay.inputs;
    while (inputIndex < inputs.size() && inputs[inputIndex].frame <= frame) {
        auto const& input = inputs[inputIndex];
        bool player2 = !input.player2;
        PlayerObject* player = player2 ? t.fakePlayer2 : t.fakePlayer1;
        PlayerObject* other = player2 ? t.fakePlayer1 : t.fakePlayer2;

        if (static_cast<int>(input.frame) != bot.respawnFrame) {
            applyReplayButton(player, input);
            if (simulateBothPlayers && pl->m_gameState.m_isDualMode)
                applyReplayButton(other, input);
        }

        inputIndex++;
    }
}

bool shouldLogSlowTrajectory(int64_t totalMs) {
    return totalMs >= 50;
}
}

$execute {
    xdbot::trajectory_physics::setContext(&showTrajectoryPhysicsContext);

    t.color1 = ccc4FFromccc3B(Mod::get()->getSavedValue<cocos2d::ccColor3B>("trajectory_color1"));
    t.color2 = ccc4FFromccc3B(Mod::get()->getSavedValue<cocos2d::ccColor3B>("trajectory_color2"));
    t.length = geode::utils::numFromString<int>(Mod::get()->getSavedValue<std::string>("trajectory_length")).unwrapOr(0);
    t.width = Mod::get()->getSavedValue<float>("trajectory_width");
    if (t.width <= 0.f)
        t.width = 0.5f;
    t.updateMergedColor();

};

void ShowTrajectory::trajectoryOff() {
    if (auto* node = t.trajectoryNode(false)) {
        node->clear();
        node->setVisible(false);
    }
}

void ShowTrajectory::refreshIfNeeded() {
    static bool cachedSignatureValid = false;
    static uint64_t cachedSignature = 0;

    auto& bot = Bot::get();
    if (!bot.showTrajectory) {
        cachedSignatureValid = false;
        return;
    }

    if (bot.creatingTrajectory)
        return;

    auto* pl = PlayLayer::get();
    if (!pl || pl->m_isPaused || !pl->m_player1) {
        cachedSignatureValid = false;
        return;
    }

    auto* node = trajectoryNode();
    uint64_t signature = trajectoryRefreshSignature(bot, pl);
    if (cachedSignatureValid && cachedSignature == signature && node && node->isVisible())
        return;

    cachedSignature = signature;
    cachedSignatureValid = true;
    updateTrajectory(pl);
}

bool ShowTrajectory::isFakePlayer(PlayerObject* player) {
    return player && (player == t.fakePlayer1 || player == t.fakePlayer2);
}

bool ShowTrajectory::hasActivated(PlayerObject* player, EnhancedGameObject* object) {
    if (!player || !object || object->m_isMultiActivate)
        return false;

    auto key = reinterpret_cast<uintptr_t>(object);
    if (player == t.fakePlayer1)
        return t.activatedObjectsP1.contains(key);
    if (player == t.fakePlayer2)
        return t.activatedObjectsP2.contains(key);

    return false;
}

bool ShowTrajectory::realPlayerHasActivated(PlayerObject* player, EnhancedGameObject* object) {
    if (!player || !object)
        return false;

    PlayerObject* realPlayer = player;
    auto* pl = PlayLayer::get();
    if (pl && player == t.fakePlayer1)
        realPlayer = pl->m_player1;
    else if (pl && player == t.fakePlayer2)
        realPlayer = pl->m_player2;

    if (!realPlayer)
        return false;

    bool platformerActivated = !realPlayer->m_isPlatformer
        ? object->m_isMultiActivate
        : object->m_isNoMultiActivate;
    if (platformerActivated)
        return false;

    if (!realPlayer->m_isSecondPlayer)
        return object->m_activatedByPlayer1;

    return object->m_activatedByPlayer2;
}

void ShowTrajectory::rememberActivated(PlayerObject* player, EnhancedGameObject* object) {
    if (!player || !object)
        return;

    if (player == t.fakePlayer1)
        t.activatedObjectsP1.insert({reinterpret_cast<uintptr_t>(object), true});
    else if (player == t.fakePlayer2)
        t.activatedObjectsP2.insert({reinterpret_cast<uintptr_t>(object), true});
}

void ShowTrajectory::snapshotObject(EffectGameObject* object) {
    if (!object)
        return;

    auto key = reinterpret_cast<uintptr_t>(object);
    if (t.snapshotObjects.contains(key))
        return;
    t.snapshotObjects.insert(key);

    auto* ring = typeinfo_cast<RingObject*>(object);
    t.objSnapshot.push_back({
        object,
        ring,
        object->m_isActivated,
        object->m_activatedByPlayer1,
        object->m_activatedByPlayer2,
        ring ? ring->m_claimTouch : false,
        object->m_isDisabled,
        object->m_isDisabled2,
    });
}

void ShowTrajectory::restoreSnapshots() {
    for (auto const& snapshot : t.objSnapshot) {
        if (!snapshot.obj)
            continue;

        snapshot.obj->m_isActivated = snapshot.isActivated;
        snapshot.obj->m_activatedByPlayer1 = snapshot.activatedByP1;
        snapshot.obj->m_activatedByPlayer2 = snapshot.activatedByP2;
        snapshot.obj->m_isDisabled = snapshot.isDisabled;
        snapshot.obj->m_isDisabled2 = snapshot.isDisabled2;

        if (snapshot.ring) {
            snapshot.ring->m_claimTouch = snapshot.claimTouch;
        }
    }

    t.objSnapshot.clear();
    t.snapshotObjects.clear();
}

void ShowTrajectory::hideFakePlayer(PlayerObject* player) {
    if (!player)
        return;

    player->setVisible(false);
    player->setOpacity(0);
    player->m_playEffects = false;
    player->m_maybeReducedEffects = true;
    if (player->getParent())
        player->removeFromParentAndCleanup(false);
}

void ShowTrajectory::releaseFakePlayers() {
    auto releasePlayer = [](PlayerObject*& player) {
        if (!player)
            return;

        hideFakePlayer(player);
        player = nullptr;
    };

    releasePlayer(t.fakePlayer1);
    releasePlayer(t.fakePlayer2);
}

PlayerObject* ShowTrajectory::ensureFakePlayer(PlayLayer* pl, bool player1) {
    if (!pl)
        return nullptr;

    PlayerObject*& fake = player1 ? t.fakePlayer1 : t.fakePlayer2;
    if (!fake) {
        fake = PlayerObject::create(1, 1, pl, pl, true);
        if (!fake)
            return nullptr;

        fake->retain();
        fake->setPosition({ 0, 105 });
        fake->setID(player1 ? "xdbot-trajectory-fake-player1" : "xdbot-trajectory-fake-player2");
    }

    hideFakePlayer(fake);
    return fake;
}

void ShowTrajectory::updateTrajectory(PlayLayer* pl) {
    if (!pl || !pl->m_player1)
        return;

    geode::utils::Timer totalTimer;
    geode::utils::Timer setupTimer;
    auto& bot = Bot::get();
    bool platformerBothSides = bot.trajectoryBothSides;

    bot.safeMode = true;

    t.creatingTrajectory = true;
    bot.creatingTrajectory = true;

    t.trajectoryNode()->setVisible(true);
    t.trajectoryNode()->clear();
    xdbot::trajectory_physics::beginTrajectoryPass();

    auto baseGameState = pl->m_gameState;
    EffectManagerState baseEffectState;
    EffectManagerState* baseEffectStatePtr = nullptr;
    if (pl->m_effectManager) {
        pl->m_effectManager->saveToState(baseEffectState);
        baseEffectStatePtr = &baseEffectState;
    }

    int64_t setupMs = setupTimer.elapsed<>();
    int p1BranchCount = 0;
    int p2BranchCount = 0;
    geode::utils::Timer p1Timer;

    if (ensureFakePlayer(pl, true)) {
        bool simulateBoth = pl->m_gameState.m_isDualMode && !pl->m_levelSettings->m_twoPlayerMode;
        if (simulateBoth && pl->m_player2)
            ensureFakePlayer(pl, false);

        createTrajectory(pl, true, Hold, simulateBoth, baseGameState, baseEffectStatePtr, {});
        p1BranchCount++;
        createTrajectory(pl, true, Release, simulateBoth, baseGameState, baseEffectStatePtr, {});
        p1BranchCount++;

        if (pl->m_levelSettings->m_platformerMode && platformerBothSides) {
            createTrajectory(pl, true, Hold | Left, simulateBoth, baseGameState, baseEffectStatePtr, {});
            p1BranchCount++;
            createTrajectory(pl, true, Release | Left, simulateBoth, baseGameState, baseEffectStatePtr, {});
            p1BranchCount++;
            createTrajectory(pl, true, Hold | Right, simulateBoth, baseGameState, baseEffectStatePtr, {});
            p1BranchCount++;
            createTrajectory(pl, true, Release | Right, simulateBoth, baseGameState, baseEffectStatePtr, {});
            p1BranchCount++;
        }
    }

    int64_t p1Ms = p1Timer.elapsed<>();
    geode::utils::Timer p2Timer;

    if (pl->m_player2 && pl->m_gameState.m_isDualMode && pl->m_levelSettings->m_twoPlayerMode) {
        if (ensureFakePlayer(pl, false)) {
            createTrajectory(pl, false, Hold, false, baseGameState, baseEffectStatePtr, {});
            p2BranchCount++;
            createTrajectory(pl, false, Release, false, baseGameState, baseEffectStatePtr, {});
            p2BranchCount++;

            if (pl->m_levelSettings->m_platformerMode && platformerBothSides) {
                createTrajectory(pl, false, Hold | Left, false, baseGameState, baseEffectStatePtr, {});
                p2BranchCount++;
                createTrajectory(pl, false, Release | Left, false, baseGameState, baseEffectStatePtr, {});
                p2BranchCount++;
                createTrajectory(pl, false, Hold | Right, false, baseGameState, baseEffectStatePtr, {});
                p2BranchCount++;
                createTrajectory(pl, false, Release | Right, false, baseGameState, baseEffectStatePtr, {});
                p2BranchCount++;
            }
        }
    }
    int64_t p2Ms = p2Timer.elapsed<>();

    pl->m_gameState = baseGameState;
    if (pl->m_effectManager && baseEffectStatePtr)
        pl->m_effectManager->loadFromState(*baseEffectStatePtr);

    hideFakePlayer(t.fakePlayer1);
    hideFakePlayer(t.fakePlayer2);
    auto totalMs = totalTimer.elapsed<>();
    if (shouldLogSlowTrajectory(totalMs)) {
        auto passStats = xdbot::trajectory_physics::currentPassStats();
        log::info(
            "Trajectory slow: total={}ms setup={}ms p1={}ms p2={}ms p1Branches={} p2Branches={} length={} objs={} solid={} hazard={} effects={} obb={} spawnVisited={} spawnCandidates={} spawnTriggered={} dual={} tp={} bothSides={}",
            totalMs,
            setupMs,
            p1Ms,
            p2Ms,
            p1BranchCount,
            p2BranchCount,
            t.length,
            passStats.objectsVisited,
            passStats.solidQueued,
            passStats.hazardQueued,
            passStats.effectCandidates,
            passStats.obbChecks,
            passStats.spawnVisited,
            passStats.spawnCandidates,
            passStats.spawnTriggered,
            pl->m_gameState.m_isDualMode,
            pl->m_levelSettings ? pl->m_levelSettings->m_twoPlayerMode : false,
            platformerBothSides
        );
    }
    xdbot::trajectory_physics::endTrajectoryPass();
    t.creatingTrajectory = false;
    bot.creatingTrajectory = false;
}

ShowTrajectory::PredictionResult ShowTrajectory::simulate(
    PlayLayer* pl,
    bool player1,
    int mode,
    bool simulateBothPlayers,
    PredictionConfig config
) {
    PredictionResult result;
    result.player1 = player1;
    result.holding = (mode & Hold) != 0;
    if (!pl || !pl->m_player1)
        return result;

    auto& bot = Bot::get();
    bool wasCreatingTrajectory = t.creatingTrajectory;
    bool wasBotCreatingTrajectory = bot.creatingTrajectory;
    t.creatingTrajectory = true;
    bot.creatingTrajectory = true;
    xdbot::trajectory_physics::beginTrajectoryPass();

    auto baseGameState = pl->m_gameState;
    EffectManagerState baseEffectState;
    EffectManagerState* baseEffectStatePtr = nullptr;
    if (pl->m_effectManager) {
        pl->m_effectManager->saveToState(baseEffectState);
        baseEffectStatePtr = &baseEffectState;
    }

    if (ensureFakePlayer(pl, player1)) {
        bool canSimulateBoth = simulateBothPlayers && pl->m_gameState.m_isDualMode &&
            !pl->m_levelSettings->m_twoPlayerMode;
        if (canSimulateBoth && pl->m_player2)
            ensureFakePlayer(pl, false);

        result = createTrajectory(
            pl,
            player1,
            mode,
            canSimulateBoth,
            baseGameState,
            baseEffectStatePtr,
            config
        );
    }

    pl->m_gameState = baseGameState;
    if (pl->m_effectManager && baseEffectStatePtr)
        pl->m_effectManager->loadFromState(*baseEffectStatePtr);

    hideFakePlayer(t.fakePlayer1);
    hideFakePlayer(t.fakePlayer2);
    xdbot::trajectory_physics::endTrajectoryPass();
    t.creatingTrajectory = wasCreatingTrajectory;
    bot.creatingTrajectory = wasBotCreatingTrajectory;
    return result;
}

float rot = 0.f;
void ShowTrajectory::applyInitialInput(PlayLayer* pl, PlayerObject* player, PlayerObject* realPlayer, int mode) {
    if (!pl || !player || !realPlayer)
        return;

    switch (mode & (Hold | Swift | Release)) {
    case Hold:
        player->pushButton(PlayerButton::Jump);
        break;
    case Swift:
        player->pushButton(PlayerButton::Jump);
        player->releaseButton(PlayerButton::Jump);
        break;
    case Release:
        player->releaseButton(PlayerButton::Jump);
        player->m_jumpBuffered = false;
        break;
    default:
        break;
    }

    if (!pl->m_levelSettings->m_platformerMode)
        return;

    switch (mode & (Left | Right)) {
    case Left:
        player->releaseButton(PlayerButton::Right);
        player->pushButton(PlayerButton::Left);
        break;
    case Right:
        player->releaseButton(PlayerButton::Left);
        player->pushButton(PlayerButton::Right);
        break;
    default:
        player->releaseButton(PlayerButton::Left);
        player->releaseButton(PlayerButton::Right);
        if (realPlayer->m_isGoingLeft)
            player->pushButton(PlayerButton::Left);
        break;
    }
}

bool ShowTrajectory::iterate(
    PlayLayer* pl,
    PlayerObject* player,
    int mode,
    cocos2d::ccColor4F color,
    PredictionConfig const& config
) {
    if (!pl || !player)
        return true;

    CCPoint prevPos = player->getPosition();

    float tps = std::max(Bot::getTPS(), 1.f);
    double physicsSeconds = 1.0 / tps;
    float timeWarp = std::min(pl->m_gameState.m_timeWarp, 1.f);
    if (timeWarp <= 0.f)
        timeWarp = 1.f;

    pl->m_gameState.m_totalTime += physicsSeconds;
    pl->m_gameState.m_unkDouble3 += physicsSeconds / timeWarp;
    pl->m_gameState.m_currentProgress++;
    pl->m_gameState.m_unkUint5 += static_cast<int>(std::round(timeWarp * 1000.f));
    player->m_totalTime += physicsSeconds;

    float playerSpeed = pl->m_gameState.m_timeModRelated;
    if (playerSpeed != 0.f) {
        pl->m_gameState.m_timeModRelated = 0;
        pl->m_gameState.m_timeModRelated2 = 0;
        player->updateTimeMod(playerSpeed, true);
        timeWarp = std::min(pl->m_gameState.m_timeWarp, 1.f);
        if (timeWarp <= 0.f)
            timeWarp = 1.f;
    }

    player->m_collisionLogTop->removeAllObjects();
    player->m_collisionLogBottom->removeAllObjects();
    player->m_collisionLogLeft->removeAllObjects();
    player->m_collisionLogRight->removeAllObjects();

    bool dead = (player == t.fakePlayer1 && t.deadP1) || (player == t.fakePlayer2 && t.deadP2);
    if (dead) {
        if (config.draw)
            drawPlayerHitbox(player, t.trajectoryNode());
        return true;
    }

    t.delta = (1.f / tps) * 60.f * timeWarp;
    player->m_playEffects = false;
    player->update(t.delta);
    player->updateRotation(t.delta);

    if (pl->checkCollisions(player, t.delta, false) == 1) {
        if (player == t.fakePlayer1)
            t.deadP1 = true;
        else if (player == t.fakePlayer2)
            t.deadP2 = true;
    }

    xdbot::trajectory_physics::checkSpawnObjects(pl, player);

    if (pl->m_effectManager)
        pl->m_effectManager->postCollisionCheck();

    if (config.draw) {
        float width = t.width;
        if (pl->m_gameState.m_cameraZoom > 0.f)
            width /= pl->m_gameState.m_cameraZoom;
        t.trajectoryNode()->drawSegment(prevPos, player->getPosition(), width, color);
    }

    dead = (player == t.fakePlayer1 && t.deadP1) || (player == t.fakePlayer2 && t.deadP2);
    if (dead) {
        if (config.draw)
            drawPlayerHitbox(player, t.trajectoryNode());
        return true;
    }

    return false;
}

ShowTrajectory::PredictionResult ShowTrajectory::createTrajectory(
    PlayLayer* pl,
    bool player1,
    int mode,
    bool simulateBothPlayers,
    GJGameState const& baseGameState,
    EffectManagerState* baseEffectState,
    PredictionConfig config
) {
    PredictionResult result;
    result.player1 = player1;
    result.holding = (mode & Hold) != 0;

    if (!pl)
        return result;

    PlayerObject* fakePlayer = player1 ? t.fakePlayer1 : t.fakePlayer2;
    PlayerObject* realPlayer = player1 ? pl->m_player1 : pl->m_player2;
    PlayerObject* otherFake = player1 ? t.fakePlayer2 : t.fakePlayer1;
    PlayerObject* otherReal = player1 ? pl->m_player2 : pl->m_player1;
    if (!fakePlayer || !realPlayer)
        return result;

    PlayerPracticeFixes::transfer(realPlayer, fakePlayer, true);
    hideFakePlayer(fakePlayer);

    if (simulateBothPlayers && otherFake && otherReal) {
        PlayerPracticeFixes::transfer(otherReal, otherFake, true);
        hideFakePlayer(otherFake);
    }

    t.cancelTrajectory = false;
    t.deadP1 = false;
    t.deadP2 = false;
    t.activatedObjectsP1.clear();
    t.activatedObjectsP2.clear();
    t.objSnapshot.clear();
    t.snapshotObjects.clear();

    applyInitialInput(pl, fakePlayer, realPlayer, mode);
    if (simulateBothPlayers && otherFake && otherReal)
        applyInitialInput(pl, otherFake, otherReal, mode);

    cocos2d::ccColor4F color = (mode & Hold) ? t.color1 : (mode & Swift) ? t.color3 : t.color2;
    cocos2d::ccColor4F otherColor = ccc4f(1.f - color.r, 1.f - color.g, 1.f - color.b, color.a);

    bool stopMain = false;
    bool stopOther = !simulateBothPlayers || !otherFake;
    int predictionLength = std::min(std::max(t.length, 0), std::max(config.maxLength, 0));
    size_t replayInputIndex = Bot::get().currentAction;
    for (int i = 0; i < predictionLength; i++) {
        if (i >= predictionLength - 40)
            color.a = (predictionLength - i) / 40.f;
        otherColor.a = color.a;

        uint64_t frame = static_cast<uint64_t>(Bot::getCurrentFrame()) + static_cast<uint64_t>(i);
        applyReplayInputsForPrediction(
            pl,
            replayInputIndex,
            frame,
            simulateBothPlayers,
            config.applyReplayInputs
        );

        if (simulateBothPlayers && !pl->m_gameState.m_isDualMode)
            stopOther = true;

        if (!stopMain) {
            stopMain = iterate(pl, fakePlayer, mode, color, config);
            result.score++;
        }
        if (!stopOther)
            stopOther = iterate(pl, otherFake, mode, otherColor, config);

        if (stopMain && stopOther)
            break;
    }

    restoreSnapshots();
    t.activatedObjectsP1.clear();
    t.activatedObjectsP2.clear();
    t.snapshotObjects.clear();
    pl->m_gameState = baseGameState;
    if (pl->m_effectManager && baseEffectState)
        pl->m_effectManager->loadFromState(*baseEffectState);
    hideFakePlayer(fakePlayer);
    if (simulateBothPlayers)
        hideFakePlayer(otherFake);

    result.position = fakePlayer->getPosition();
    result.hitbox = fakePlayer->getObjectRect();
    result.innerHitbox = fakePlayer->getObjectRect(0.3, 0.3);
    result.rotation = fakePlayer->getRotation();
    return result;
}

void ShowTrajectory::drawPlayerHitbox(PlayerObject* player, CCDrawNode* drawNode) {
    cocos2d::CCRect bigRect = player->GameObject::getObjectRect();
    cocos2d::CCRect smallRect = player->GameObject::getObjectRect(0.3, 0.3);

    std::vector<cocos2d::CCPoint> vertices = ShowTrajectory::getVertices(player, bigRect, t.deathRotation);
    drawNode->drawPolygon(&vertices[0], 4, ccc4f(t.color2.r, t.color2.g, t.color2.b, 0.2f), 0.5, t.color2);

    vertices = ShowTrajectory::getVertices(player, smallRect, t.deathRotation);
    drawNode->drawPolygon(&vertices[0], 4, ccc4f(t.color3.r, t.color3.g, t.color3.b, 0.2f), 0.35, ccc4f(t.color3.r, t.color3.g, t.color3.b, 0.55f));
}

std::vector<cocos2d::CCPoint> ShowTrajectory::getVertices(PlayerObject* player, cocos2d::CCRect rect, float rotation) {
    std::vector<cocos2d::CCPoint> vertices = {
        ccp(rect.getMinX(), rect.getMaxY()),
        ccp(rect.getMaxX(), rect.getMaxY()),
        ccp(rect.getMaxX(), rect.getMinY()),
        ccp(rect.getMinX(), rect.getMinY())
    };

    cocos2d::CCPoint center = ccp(
        (rect.getMinX() + rect.getMaxX()) / 2.f,
        (rect.getMinY() + rect.getMaxY()) / 2.f
    );

    float size = static_cast<int>(rect.getMaxX() - rect.getMinX());

    if ((size == 18 || size == 5) && player->getScale() == 1) {
        for (auto& vertex : vertices) {
            vertex.x = center.x + (vertex.x - center.x) / 0.6f;
            vertex.y = center.y + (vertex.y - center.y) / 0.6f;
        }
    }

    if ((size == 7 || size == 30 || size == 29 || size == 9) && player->getScale() != 1) {
        for (auto& vertex : vertices) {
            vertex.x = center.x + (vertex.x - center.x) * 0.6;
            vertex.y = center.y + (vertex.y - center.y) * 0.6f;
        }
    }

    if (player->m_isDart) {
        for (auto& vertex : vertices) {
            vertex.x = center.x + (vertex.x - center.x) * 0.3f;
            vertex.y = center.y + (vertex.y - center.y) * 0.3f;
        }
    }

    float angle = CC_DEGREES_TO_RADIANS(rotation * -1.f);
    for (auto& vertex : vertices) {
        float x = vertex.x - center.x;
        float y = vertex.y - center.y;

        float xNew = center.x + (x * cos(angle)) - (y * sin(angle));
        float yNew = center.y + (x * sin(angle)) + (y * cos(angle));

        vertex.x = xNew;
        vertex.y = yNew;
    }

    return vertices;
}

void ShowTrajectory::updateMergedColor() {
    cocos2d::ccColor4F newColor = { 0.f, 0.f, 0.f, 1.f };

    newColor.r = (color1.r + color2.r) / 2;
    newColor.b = (color1.b + color2.b) / 2;
    newColor.g = (color1.g + color2.g) / 2;

    newColor.r = std::min(1.f, newColor.r + 0.45f);
    newColor.g = std::min(1.f, newColor.g + 0.45f);
    newColor.b = std::min(1.f, newColor.b + 0.45f);

    color3 = newColor;
}

void ShowTrajectory::handlePortal(PlayerObject* player, int id) {
    switch (id) {
    case 101:
        player->togglePlayerScale(true, true);
        player->updatePlayerScale();
        break;
    case 99:
        player->togglePlayerScale(false, true);
        player->updatePlayerScale();
        break;
        // case 11:
            // player->flipGravity(true, true); break;
        // case 10:
            // player->flipGravity(false, true); break;
    case 200: player->m_playerSpeed = 0.7f; break;
    case 201: player->m_playerSpeed = 0.9f; break;
    case 202: player->m_playerSpeed = 1.1f; break;
    case 203: player->m_playerSpeed = 1.3f; break;
    case 1334: player->m_playerSpeed = 1.6f; break;
    }
}

cocos2d::CCDrawNode* ShowTrajectory::trajectoryNode(bool create) {

    static TrajectoryNode* instance = nullptr;

    if (!create)
        return instance;

    if (!instance) {
        instance = TrajectoryNode::create();
        instance->retain();

        cocos2d::_ccBlendFunc  blendFunc;
        blendFunc.src = GL_SRC_ALPHA;
        blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;

        instance->setBlendFunc(blendFunc);
    }

    return instance;
}

void ShowTrajectory::detachTrajectoryNode() {
    auto* node = trajectoryNode(false);
    if (!node)
        return;

    node->clear();
    node->setVisible(false);
    if (node->getParent())
        node->removeFromParentAndCleanup(false);
}

class $modify(PlayLayer) {
    void playGravityEffect(bool flip) {
        if (!t.creatingTrajectory)
            PlayLayer::playGravityEffect(flip);
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        ShowTrajectory::releaseFakePlayers();
        t.cancelTrajectory = false;
        t.creatingTrajectory = false;
        t.deadP1 = false;
        t.deadP2 = false;

        auto* node = t.trajectoryNode();
        if (node->getParent())
            node->removeFromParentAndCleanup(false);
        m_objectLayer->addChild(node, 500);
        node->setVisible(false);
    }

    void destroyPlayer(PlayerObject * player, GameObject * gameObject) {
        if (t.creatingTrajectory || (player == t.fakePlayer1 || player == t.fakePlayer2)) {
            t.deathRotation = player->getRotation();
            if (player == t.fakePlayer1)
                t.deadP1 = true;
            else if (player == t.fakePlayer2)
                t.deadP2 = true;
            t.cancelTrajectory = true;
            return;
        }

        PlayLayer::destroyPlayer(player, gameObject);
    }

    void onQuit() {
        ShowTrajectory::detachTrajectoryNode();

        ShowTrajectory::releaseFakePlayers();
        t.cancelTrajectory = false;
        t.creatingTrajectory = false;
        t.deadP1 = false;
        t.deadP2 = false;

        PlayLayer::onQuit();
    }

    void playEndAnimationToPos(cocos2d::CCPoint p0) {
        if (!t.creatingTrajectory)
            PlayLayer::playEndAnimationToPos(p0);
    }

};

class $modify(PauseLayer) {
    void onQuit(CCObject* sender) {
        ShowTrajectory::detachTrajectoryNode();

        ShowTrajectory::releaseFakePlayers();
        t.cancelTrajectory = false;
        t.creatingTrajectory = false;
        t.deadP1 = false;
        t.deadP2 = false;

        PauseLayer::onQuit(sender);
    }

    void goEdit() {
        ShowTrajectory::detachTrajectoryNode();

        ShowTrajectory::releaseFakePlayers();
        t.cancelTrajectory = false;
        t.creatingTrajectory = false;
        t.deadP1 = false;
        t.deadP2 = false;

        PauseLayer::goEdit();
    }
};

class $modify(GJBaseGameLayer) {

    void collisionCheckObjects(PlayerObject * p0, gd::vector<GameObject*>*objects, int p2, float p3) {
        if (t.creatingTrajectory && ShowTrajectory::isFakePlayer(p0)) {
            xdbot::trajectory_physics::collisionCheckObjects(this, p0, objects, p2, p3);
            return;
        }

        GJBaseGameLayer::collisionCheckObjects(p0, objects, p2, p3);
    }

    bool canBeActivatedByPlayer(PlayerObject * p0, EffectGameObject * p1) {
        if (t.creatingTrajectory && ShowTrajectory::isFakePlayer(p0)) {
            if (ShowTrajectory::hasActivated(p0, p1) ||
                ShowTrajectory::realPlayerHasActivated(p0, p1))
                return false;

            ShowTrajectory::snapshotObject(p1);
            return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
        }

        return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
    }

    void playerTouchedRing(PlayerObject * p0, RingObject * p1) {
        if (t.creatingTrajectory && ShowTrajectory::isFakePlayer(p0)) {
            ShowTrajectory::snapshotObject(p1);
            ShowTrajectory::rememberActivated(p0, p1);
            xdbot::trajectory_physics::ringJump(p0, p1);
        } else if (!t.creatingTrajectory)
            GJBaseGameLayer::playerTouchedRing(p0, p1);
    }

    void playerTouchedTrigger(PlayerObject * p0, EffectGameObject * p1) {
        if (t.creatingTrajectory && ShowTrajectory::isFakePlayer(p0)) {
            xdbot::trajectory_physics::triggerObject(p1, this, p0);
        } else if (!t.creatingTrajectory)
            GJBaseGameLayer::playerTouchedTrigger(p0, p1);
    }

    void activateSFXTrigger(SFXTriggerGameObject * p0) {
        if (!t.creatingTrajectory)
            GJBaseGameLayer::activateSFXTrigger(p0);

    }
    void activateSongEditTrigger(SongTriggerGameObject * p0) {
        if (!t.creatingTrajectory)
            GJBaseGameLayer::activateSongEditTrigger(p0);

    }
    // void activateSongTrigger(SongTriggerGameObject * p0) {
    //     if (!t.creatingTrajectory)
    //         GJBaseGameLayer::activateSongTrigger(p0);
    // }

    void gameEventTriggered(GJGameEvent p0, int p1, int p2) {
        if (!t.creatingTrajectory)
            GJBaseGameLayer::gameEventTriggered(p0, p1, p2);
    }

    void teleportPlayer(TeleportPortalObject* object, PlayerObject* player) {
        if (ShowTrajectory::isFakePlayer(player)) {
            xdbot::trajectory_physics::teleportPlayer(this, object, player);
        } else {
            GJBaseGameLayer::teleportPlayer(object, player);
        }
    }

    void flipGravity(PlayerObject* player, bool flip, bool noEffects) {
        if (ShowTrajectory::isFakePlayer(player)) {
            xdbot::trajectory_physics::flipGravity(player, flip);
        } else {
            GJBaseGameLayer::flipGravity(player, flip, noEffects);
        }
    }

};

class $modify(PlayerObject) {

    void update(float dt) {
        PlayerObject::update(dt);
        t.delta = dt;
    }

    void playSpiderDashEffect(cocos2d::CCPoint p0, cocos2d::CCPoint p1) {
        if (!t.creatingTrajectory)
            PlayerObject::playSpiderDashEffect(p0, p1);
    }

    void incrementJumps() {
        if (ShowTrajectory::isFakePlayer(this))
            return;
        if (!t.creatingTrajectory)
            PlayerObject::incrementJumps();
    }

    void ringJump(RingObject * p0, bool p1) {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::ringJump(this, p0);
        } else if (!t.creatingTrajectory)
            PlayerObject::ringJump(p0, p1);
    }

    void bumpPlayer(float force, int objectType, bool noEffects, GameObject* object) {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::bumpPlayer(this, force, objectType, noEffects, object);
        } else {
            PlayerObject::bumpPlayer(force, objectType, noEffects, object);
        }
    }

    void propellPlayer(float force, bool noEffects, int objectType) {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::propellPlayer(this, force, noEffects, objectType);
        } else {
            PlayerObject::propellPlayer(force, noEffects, objectType);
        }
    }

    void startDashing(DashRingObject* object) {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::startDashing(this, object);
        } else {
            PlayerObject::startDashing(object);
        }
    }

    void togglePlayerScale(bool enable, bool noEffects) {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::togglePlayerScale(this, enable);
        } else {
            PlayerObject::togglePlayerScale(enable, noEffects);
        }
    }

    void flipGravity(bool flip, bool noEffects) {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::flipGravity(this, flip);
        } else {
            PlayerObject::flipGravity(flip, noEffects);
        }
    }

    void stopDashing() {
        if (ShowTrajectory::isFakePlayer(this)) {
            xdbot::trajectory_physics::stopDashing(this);
        } else {
            PlayerObject::stopDashing();
        }
    }

};

class $modify(HardStreak) {

    void addPoint(cocos2d::CCPoint p0) {
        if (!t.creatingTrajectory)
            HardStreak::addPoint(p0);
    }
};

class $modify(GameObject) {

    void playShineEffect() {
        if (!t.creatingTrajectory)
            GameObject::playShineEffect();
    }
};

class $modify(EffectGameObject) {

    void triggerObject(GJBaseGameLayer * p0, int p1, const gd::vector<int>*p2) {
        if (!t.creatingTrajectory)
            EffectGameObject::triggerObject(p0, p1, p2);
    }
};
