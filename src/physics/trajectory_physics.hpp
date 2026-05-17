#pragma once

#include <Geode/Geode.hpp>

namespace xdbot::trajectory_physics {
struct PassStats {
    int objectsVisited = 0;
    int solidQueued = 0;
    int hazardQueued = 0;
    int effectCandidates = 0;
    int obbChecks = 0;
    int spawnVisited = 0;
    int spawnCandidates = 0;
    int spawnTriggered = 0;
};

struct Context {
    PlayerObject* (*fakePlayer1)();
    PlayerObject* (*fakePlayer2)();
    bool (*isFakePlayer)(PlayerObject*);
    bool (*hasActivated)(PlayerObject*, EnhancedGameObject*);
    bool (*realPlayerHasActivated)(PlayerObject*, EnhancedGameObject*);
    void (*snapshotObject)(EffectGameObject*);
    void (*rememberActivated)(PlayerObject*, EnhancedGameObject*);
};

void setContext(Context const* context);
void beginTrajectoryPass();
void endTrajectoryPass();
PassStats currentPassStats();

void flipGravity(PlayerObject* player, bool gravity);
void propellPlayer(PlayerObject* player, float force, bool noEffects, int objectType);
void bumpPlayer(PlayerObject* player, float force, int objectType, bool noEffects, GameObject* object);
void startDashing(PlayerObject* player, DashRingObject* ring);
void stopDashing(PlayerObject* player);
void ringJump(PlayerObject* player, RingObject* ring);
void togglePlayerScale(PlayerObject* player, bool smallSize);
void teleportPlayer(GJBaseGameLayer* layer, TeleportPortalObject* object, PlayerObject* player);
bool activatePortal(GJBaseGameLayer* layer, PlayerObject* player, EffectGameObject* portal);
void triggerObject(EffectGameObject* object, GJBaseGameLayer* layer, PlayerObject* player);
void checkSpawnObjects(GJBaseGameLayer* layer, PlayerObject* player);
void collisionCheckObjects(GJBaseGameLayer* layer, PlayerObject* player, gd::vector<GameObject*>* objects, int objectCount, float dt);
}
