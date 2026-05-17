#include "trajectory_physics.hpp"

#include <cstdint>
#include <unordered_map>

using namespace geode::prelude;

namespace xdbot::trajectory_physics {
namespace {
Context const* activeContext = nullptr;
thread_local std::unordered_map<uintptr_t, CCRect> s_objectRectCache;
thread_local std::unordered_map<uintptr_t, OBB2D*> s_objectObbCache;
thread_local std::unordered_map<int, bool> s_spawnChannelHasTrajectoryTriggers;
thread_local PassStats s_passStats;

PlayerObject* fakePlayer1() {
    return activeContext && activeContext->fakePlayer1 ? activeContext->fakePlayer1() : nullptr;
}

PlayerObject* fakePlayer2() {
    return activeContext && activeContext->fakePlayer2 ? activeContext->fakePlayer2() : nullptr;
}

bool isFakePlayer(PlayerObject* player) {
    return activeContext && activeContext->isFakePlayer && activeContext->isFakePlayer(player);
}

bool hasActivated(PlayerObject* player, EnhancedGameObject* object) {
    return activeContext && activeContext->hasActivated && activeContext->hasActivated(player, object);
}

bool realPlayerHasActivated(PlayerObject* player, EnhancedGameObject* object) {
    return activeContext &&
        activeContext->realPlayerHasActivated &&
        activeContext->realPlayerHasActivated(player, object);
}

void snapshotObject(EffectGameObject* object) {
    if (activeContext && activeContext->snapshotObject)
        activeContext->snapshotObject(object);
}

void rememberActivated(PlayerObject* player, EnhancedGameObject* object) {
    if (activeContext && activeContext->rememberActivated)
        activeContext->rememberActivated(player, object);
}

CCRect getCachedObjectRect(GameObject* object) {
    auto key = reinterpret_cast<uintptr_t>(object);
    if (auto it = s_objectRectCache.find(key); it != s_objectRectCache.end())
        return it->second;

    auto rect = object->m_objectType == GameObjectType::Slope
        ? object->getObjectRect(2.0, 2.0)
        : object->getObjectRect();
    s_objectRectCache.insert_or_assign(key, rect);
    return rect;
}

OBB2D* getCachedObjectObb(GameObject* object) {
    auto key = reinterpret_cast<uintptr_t>(object);
    if (auto it = s_objectObbCache.find(key); it != s_objectObbCache.end())
        return it->second;

    auto* box = object->getOrientedBox();
    s_objectObbCache.insert_or_assign(key, box);
    return box;
}

float gravitySign(PlayerObject* player) {
    return player && player->m_isUpsideDown ? -1.f : 1.f;
}

void activateForTrajectory(EffectGameObject* object, PlayerObject* player) {
    snapshotObject(object);
    rememberActivated(player, object);
}

bool isTrajectorySpawnTrigger(SpawnTriggerGameObject* object) {
    if (!object)
        return false;

    return object->m_objectID == 2066 ||
        object->m_objectID == 2900 ||
        object->m_objectID == 3022 ||
        object->m_objectID == 901;
}

bool channelHasTrajectorySpawnTriggers(int channel, cocos2d::CCArray* objects) {
    if (auto it = s_spawnChannelHasTrajectoryTriggers.find(channel);
        it != s_spawnChannelHasTrajectoryTriggers.end()) {
        return it->second;
    }

    bool found = false;
    unsigned int count = objects ? objects->count() : 0;
    for (unsigned int i = 0; i < count; i++) {
        auto* object = static_cast<SpawnTriggerGameObject*>(objects->objectAtIndex(i));
        if (isTrajectorySpawnTrigger(object)) {
            found = true;
            break;
        }
    }

    s_spawnChannelHasTrajectoryTriggers.insert_or_assign(channel, found);
    return found;
}

void bumpPlayerFromLayer(GJBaseGameLayer* layer, PlayerObject* player, EffectGameObject* object) {
    if (!layer || !player || !object)
        return;
    if (!layer->canBeActivatedByPlayer(player, object))
        return;

    player->m_lastPortalPos = object->getPosition();
    activateForTrajectory(object, player);

    float force = 1.f;
    if (object->m_objectType == GameObjectType::PinkJumpPad) {
        if (player->m_isShip)
            force = 0.35f;
        else if (player->m_isBird)
            force = 0.4f;
        else if (player->m_isBall || player->m_isSpider)
            force = 0.7f;
        else
            force = 0.65f;
    } else if (object->m_objectType == GameObjectType::RedJumpPad) {
        if (player->m_isShip)
            force = player->m_vehicleSize >= 1.f ? 0.63f : 0.95f;
        else if (player->m_isBird)
            force = player->m_vehicleSize >= 1.f ? 0.6f : 0.98f;
        else
            force = 1.25f;
    }

    player->m_lastActivatedPortal = object;
    bumpPlayer(player, force, static_cast<int>(object->m_objectType), false, object);
}

void clearCollisionState(PlayerObject* player) {
    if (!player)
        return;

    player->m_collidedBottomMaxY = 0.0;
    player->m_collidedTopMinY = 0.0;
    player->m_collidedLeftMaxX = 0.0;
    player->m_collidedRightMinX = 0.0;
    player->m_unkA29 = false;
    player->m_collisionLogTop->removeAllObjects();
    player->m_collisionLogBottom->removeAllObjects();
    player->m_collisionLogLeft->removeAllObjects();
    player->m_collisionLogRight->removeAllObjects();
    player->m_unk50C = -1;
    player->m_unk510 = -1;
    player->m_lastCollisionBottom = -1;
    player->m_lastCollisionTop = -1;
    player->m_lastCollisionLeft = -1;
    player->m_lastCollisionRight = -1;
}

CCPoint pointForAngle(float angle) {
#ifdef GEODE_IS_WINDOWS
    return ccpForAngle(angle);
#else
    return {std::cos(angle), std::sin(angle)};
#endif
}

cocos2d::CCArray* getGroup(GJBaseGameLayer* layer, int groupID) {
    if (!layer)
        return nullptr;

    groupID = std::clamp(groupID, 0, 9999);
    cocos2d::CCArray* group = layer->m_groups.at(groupID);
    if (!group) {
        group = cocos2d::CCArray::create();
        layer->m_groupDict->setObject(group, groupID);
        layer->m_groups.at(groupID) = group;
    }

    return group;
}

float redirectPlayerForce(PlayerObject* player, float force, float, float, float) {
    if (!player)
        return 0.f;

    CCPoint velocity = {
        static_cast<float>(player->m_platformerXVelocity),
        static_cast<float>(player->m_yVelocity),
    };

    float angle = force * 0.017453292f - std::atan2(velocity.y, velocity.x);
    if (angle != 0.f) {
        float s = std::sin(angle);
        float c = std::cos(angle);
        velocity = CCPoint{
            velocity.x * s - velocity.y * c,
            velocity.x * c + velocity.y * s,
        };
    }

    if (player->m_isSideways)
        std::swap(velocity.x, velocity.y);

    player->m_yVelocity = velocity.y;
    player->m_isAccelerating = true;
    if (player->m_isPlatformer) {
        player->m_platformerXVelocity = velocity.x;
        player->m_affectedByForces = true;
    }

    return player->m_yVelocity;
}
}

void setContext(Context const* context) {
    activeContext = context;
}

void beginTrajectoryPass() {
    s_objectRectCache.clear();
    s_objectObbCache.clear();
    s_spawnChannelHasTrajectoryTriggers.clear();
    s_passStats = {};
}

void endTrajectoryPass() {
    s_objectRectCache.clear();
    s_objectObbCache.clear();
    s_spawnChannelHasTrajectoryTriggers.clear();
}

PassStats currentPassStats() {
    return s_passStats;
}

void flipGravity(PlayerObject* player, bool gravity) {
    if (!player || player->m_isUpsideDown == gravity)
        return;

    auto flipSingle = [](PlayerObject* target, bool targetGravity) {
        target->m_isUpsideDown = targetGravity;
        target->m_lastFlipTime = target->m_totalTime;
        if (target->m_wasOnSlope || target->m_isOnSlope)
            target->m_slopeFlipGravityRelated = !target->m_slopeFlipGravityRelated;

        clearCollisionState(target);
        target->m_yVelocity *= 0.5;
        target->m_isOnGround = false;

        if (target->m_isBall) {
            target->m_isRotating = false;
            target->m_isBallRotating = false;
            target->m_isBallRotating2 = false;
            target->m_rotationSpeed = 0.0;
            target->runBallRotation2();
        }
    };

    flipSingle(player, gravity);

    auto* layer = GJBaseGameLayer::get();
    if (!layer || !isFakePlayer(player) || layer->m_gameState.m_unkBool31 ||
        !layer->m_gameState.m_isDualMode || layer->m_levelSettings->m_twoPlayerMode) {
        return;
    }

    PlayerObject* other = player == fakePlayer1() ? fakePlayer2() : fakePlayer1();
    if (!other)
        return;

    if (!(player->m_isShip == other->m_isShip &&
          player->m_isBall == other->m_isBall &&
          player->m_isBird == other->m_isBird &&
          player->m_isSpider == other->m_isSpider &&
          player->m_isRobot == other->m_isRobot &&
          player->m_isSwing == other->m_isSwing)) {
        return;
    }

    flipSingle(other, !gravity);
}

void propellPlayer(PlayerObject* player, float force, bool, int) {
    if (!player)
        return;

    player->m_maybeIsBoosted = true;
    player->m_isOnGround2 = false;
    player->m_isOnGround = false;
    player->m_isOnSlope = false;
    player->m_wasOnSlope = false;

    float sizeGravity = player->m_vehicleSize != 1.f ? 0.8f : 1.f;
    player->setYVelocity(force * sizeGravity * gravitySign(player) * 16.f, 0);

    if (player->m_isBall || player->m_isSpider || player->m_isSwing)
        player->m_yVelocity *= 0.6;

    if (!player->m_isLocked && !player->m_isDashing) {
        player->m_isRotating = false;
        player->m_isBallRotating = false;
        player->m_isBallRotating2 = false;
        player->m_rotationSpeed = 0.0;
        if (!player->m_isBall)
            player->runBallRotation(player->m_yVelocity);
        else
            player->runNormalRotation(0, 1.0);
    }

    player->m_lastGroundedPos = player->getPosition();
}

void bumpPlayer(PlayerObject* player, float force, int objectType, bool noEffects, GameObject* object) {
    if (!player)
        return;

    if (player->m_isPlatformer || !player->m_fixRobotJump)
        player->m_touchedPad = true;

    if (objectType != static_cast<int>(GameObjectType::SpiderPad)) {
        propellPlayer(player, force, noEffects, objectType);
        player->m_isAccelerating = objectType == static_cast<int>(GameObjectType::RedJumpPad);
        if (player->m_isAccelerating)
            player->m_lastGroundedPos = CCPoint{0.f, 0.f};
        return;
    }

    if (object) {
        bool facing = player->m_isSideways ? object->isFacingLeft() : object->isFacingDown();
        if (player->m_isUpsideDown != facing)
            flipGravity(player, !player->m_isUpsideDown);
    }

    player->spiderTestJumpInternal(false);
}

void startDashing(PlayerObject* player, DashRingObject* ring) {
    if (!player || !ring || player->m_isDashing)
        return;

    player->m_isDashing = true;
    player->m_lastLandTime = 0.0;
    player->m_isRotating = false;
    player->m_isBallRotating2 = false;
    player->m_isBallRotating = false;
    player->m_rotationSpeed = 0.0;
    player->m_dashRing = ring;
    player->m_dashStartTime = player->m_totalTime;

    float dashAngle = ring->getObjectRotation();
    if (ring->isFlipX())
        dashAngle += 180.f;
    if (std::fabs(ring->getRotationX() - ring->getRotationY()) > 179.f)
        dashAngle += 180.f;

    dashAngle = std::fmod(-dashAngle, 360.f);
    if (dashAngle < -180.f)
        dashAngle += 360.f;
    else if (dashAngle > 180.f)
        dashAngle -= 360.f;

    float sidewaysAngle = 0.f;
    if (player->m_isPlatformer) {
        sidewaysAngle = ring->m_dashSpeed * 5.77f;
    } else {
        if (player->m_isSideways)
            sidewaysAngle = -90.f;
        if (player->m_isGoingLeft)
            sidewaysAngle += 180.f;

        float angle = dashAngle + sidewaysAngle;
        if (angle < -180.f)
            angle += 360.f;
        else if (angle > 180.f)
            angle -= 360.f;

        if (std::fabs(angle) > 90.f) {
            float side = angle <= 0.f ? -180.f : 180.f;
            angle -= side;
        }

        dashAngle = std::clamp(angle, -70.f, 70.f) - sidewaysAngle;
        sidewaysAngle = 1.f;
    }

    CCPoint dir = pointForAngle(dashAngle * 0.01745329f) * sidewaysAngle;
    if (player->m_isSideways)
        std::swap(dir.x, dir.y);

    player->m_dashX = dir.x;
    player->m_dashY = dir.y;
    if (!player->m_isPlatformer) {
        double value = dir.y / std::fabs(dir.x == 0.f ? 1.f : dir.x);
        player->m_dashY = value;
        player->m_dashX = std::fabs(value);
    } else if (dir.x != 0.f) {
        player->doReversePlayer(dir.x < 0.f);
    }

    player->m_dashAngle = dashAngle;
    player->m_lastGroundedPos = CCPoint{0.f, 0.f};
}

void stopDashing(PlayerObject* player) {
    if (!player || !player->m_isDashing)
        return;

    player->m_isDashing = false;
    player->m_lastLandTime = 0.0;
    if (player->m_isPlatformer && player->m_dashRing) {
        CCPoint boosted = {
            static_cast<float>(player->m_dashX * player->m_dashRing->m_endBoost),
            static_cast<float>(player->m_dashY * player->m_dashRing->m_endBoost),
        };
        player->m_isAccelerating = true;
        player->m_yVelocity = boosted.y;
        player->m_platformerXVelocity = boosted.x;
        player->m_affectedByForces = true;

        if (player->m_dashRing->m_stopSlide) {
            player->m_isAccelerating = false;
            player->m_affectedByForces = false;
        }
    }
    player->m_dashRing = nullptr;

    if (player->m_isPlatformer) {
        float normal = std::sqrt(player->m_dashX * player->m_dashX + player->m_dashY * player->m_dashY);
        if (normal <= 17.3f)
            normal = (normal / 17.3f) * 1.5f + 0.5f;
        else
            normal = 2.f;

        float sizeMod = player->m_vehicleSize == 1.f ? 0.433333f : 0.333333f;
        float sidewaysMod = player->m_isSideways ? -1.f : 1.f;
        int gravityMod = player->m_isUpsideDown ? -0xb4 : 0xb4;
        int leftMod = player->m_isGoingLeft ? -1 : 1;
        player->m_rotationSpeed = (gravityMod * leftMod * sidewaysMod * player->m_gravityMod * normal) / sizeMod;
        player->m_isRotating = true;
    }

    if (player->m_isBall) {
        if (player->m_isPlatformer) {
            player->m_isRotating = false;
            player->m_isBallRotating = false;
            player->m_rotationSpeed = 0.0;
        }
        player->runBallRotation(1.0);
    }
}

void teleportPlayer(GJBaseGameLayer* layer, TeleportPortalObject* object, PlayerObject* player) {
    if (!layer || !object || !player)
        return;

    player->m_wasTeleported = true;
    CCPoint playerPos = player->getPosition();
    TeleportPortalObject* destinationPortal = object->m_orangePortal;
    if (object->m_orangePortal) {
        object->m_teleportYOffset = object->m_orangePortal->getStartPos().y - object->getStartPos().y;
    } else if (object->m_targetGroupID > 0) {
        if (auto* group = getGroup(layer, object->m_targetGroupID); group && group->count() > 0) {
            float pick = group->count() == 1 ? 0.f : static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
            if (pick == 1.f)
                pick = 0.f;
            destinationPortal = typeinfo_cast<TeleportPortalObject*>(group->objectAtIndex(static_cast<int>(pick * group->count())));
        }
    }

    if (destinationPortal) {
        CCPoint destination = destinationPortal->getPosition();
        CCPoint portalPos = object->getPosition();
        player->m_lastPortalPos = object->getPosition();
        player->m_lastActivatedPortal = object;
        player->m_fallStartY = 0.f;
        if (object->m_objectID == 0x2eb) {
            destination = CCPoint{playerPos.x, object->getRealPosition().y + object->m_teleportYOffset};
        }

        if (object->m_saveOffset) {
            portalPos = object->getRealPosition();
            portalPos -= playerPos;
            destination -= portalPos;
        }
        if (object->m_ignoreX)
            destination.x = playerPos.x;
        if (object->m_ignoreY)
            destination.y = playerPos.y;

        player->setPosition(destination);
    }

    if (object->m_gravityMode > 0) {
        bool gravity = object->m_gravityMode == 2;
        if (object->m_gravityMode == 3)
            gravity = !player->m_isUpsideDown;
        flipGravity(player, gravity);
    }

    float forceAngle;
    if (destinationPortal) {
        float gravityForceMod = 90.f;
        int id = object->m_objectID;
        if (id == 0x26 || id == 0x2eb || id == 0x2ed || id == 0x810 || id == 0xb56)
            gravityForceMod = 180.f;

        forceAngle = (object->isFlipX() ? 180.f : 1.f) + (gravityForceMod - destinationPortal->getRotationX());
    } else {
        forceAngle = (object->isFlipX() ? 180.f : 1.f) - object->getRotationX();
    }

    if (object->m_redirectForceEnabled) {
        redirectPlayerForce(player, forceAngle, object->m_redirectForceMod, object->m_redirectForceMin, object->m_redirectForceMax);
    } else if (object->m_staticForceEnabled) {
        float force = object->m_staticForce;
        if (force == 0.f && !object->m_staticForceAdditive) {
            player->m_isAccelerating = false;
            player->m_yVelocity = 0.0;
            if (player->m_isPlatformer) {
                player->m_platformerXVelocity = 0.0;
                player->m_affectedByForces = false;
            }
        } else {
            CCPoint dir = pointForAngle(forceAngle * 0.01745329f);
            float length = dir.getLength();
            if (length > 0.f) {
                CCPoint forceVector = dir * (force / length);
                if (player->m_isSideways)
                    std::swap(forceVector.x, forceVector.y);

                player->m_isAccelerating = true;
                player->m_yVelocity = forceVector.y + (object->m_staticForceAdditive ? player->m_yVelocity : 0.0);
                if (player->m_isPlatformer) {
                    player->m_platformerXVelocity =
                        forceVector.x + (object->m_staticForceAdditive ? player->m_platformerXVelocity : 0.0);
                    player->m_affectedByForces = true;
                }
            }
        }
    }

    if (object->m_snapGround)
        player->m_lastGroundedPos = player->getPosition();
}

bool activatePortal(GJBaseGameLayer* layer, PlayerObject* player, EffectGameObject* portal) {
    if (!layer || !player || !portal)
        return false;
    if (!layer->canBeActivatedByPlayer(player, portal))
        return false;
    if (hasActivated(player, portal))
        return false;

    layer->playerWillSwitchMode(player, portal);

    bool isSameMode =
        (portal->m_objectType == GameObjectType::ShipPortal && player->m_isShip) ||
        (portal->m_objectType == GameObjectType::BallPortal && player->m_isBall) ||
        (portal->m_objectType == GameObjectType::UfoPortal && player->m_isBird) ||
        (portal->m_objectType == GameObjectType::WavePortal && player->m_isDart) ||
        (portal->m_objectType == GameObjectType::SpiderPortal && player->m_isSpider) ||
        (portal->m_objectType == GameObjectType::SwingPortal && player->m_isSwing) ||
        (portal->m_objectType == GameObjectType::RobotPortal && player->m_isRobot) ||
        (portal->m_objectType == GameObjectType::CubePortal &&
            !(player->m_isShip || player->m_isBall || player->m_isBird ||
              player->m_isDart || player->m_isSpider || player->m_isSwing ||
              player->m_isRobot));

    CCPoint position = player->getPosition();
    player->switchedToMode(portal->m_objectType);

    switch (portal->m_objectType) {
    case GameObjectType::ShipPortal:
        player->toggleFlyMode(true, true);
        break;
    case GameObjectType::BallPortal:
        player->toggleRollMode(true, true);
        break;
    case GameObjectType::UfoPortal:
        player->toggleBirdMode(true, true);
        break;
    case GameObjectType::WavePortal:
        player->toggleDartMode(true, true);
        break;
    case GameObjectType::SpiderPortal:
        player->toggleSpiderMode(true, true);
        break;
    case GameObjectType::SwingPortal:
        player->toggleSwingMode(true, true);
        break;
    case GameObjectType::RobotPortal:
        player->toggleRobotMode(true, true);
        break;
    default:
        break;
    }

    player->setPosition(position);
    if (player->m_iconSprite)
        player->m_iconSprite->setPosition(position);
    if (player->m_vehicleSprite)
        player->m_vehicleSprite->setPosition(position);

    player->m_lastPortalPos = portal->getPosition();
    player->m_lastActivatedPortal = portal;
    snapshotObject(portal);
    rememberActivated(player, portal);

    return !isSameMode;
}

void triggerObject(EffectGameObject* object, GJBaseGameLayer* layer, PlayerObject* player) {
    if (!object || !layer || !player)
        return;

    snapshotObject(object);
    rememberActivated(player, object);

    switch (object->m_objectID) {
    case 200:
        layer->m_gameState.m_timeModRelated = 0.7f;
        break;
    case 201:
        layer->m_gameState.m_timeModRelated = 0.9f;
        break;
    case 202:
        layer->m_gameState.m_timeModRelated = 1.1f;
        break;
    case 203:
        layer->m_gameState.m_timeModRelated = 1.3f;
        break;
    case 1334:
        layer->m_gameState.m_timeModRelated = 1.6f;
        break;
    case 2066: {
        if (object->m_followCPP)
            break;

        bool isP2 = player == fakePlayer2();
        if ((!object->m_targetPlayer2 && !isP2) || (object->m_targetPlayer2 && isP2))
            player->m_gravityMod = object->m_gravityValue;
        break;
    }
    case 2900: {
        auto* rotate = typeinfo_cast<RotateGameplayGameObject*>(object);
        if (!rotate)
            break;

        player->rotateGameplay(
            rotate->m_moveDirection,
            rotate->m_groundDirection,
            rotate->m_editVelocity,
            rotate->m_velocityModX,
            rotate->m_velocityModY,
            rotate->m_overrideVelocity,
            rotate->m_dontSlide
        );
        break;
    }
    case 3022:
        if (auto* teleport = typeinfo_cast<TeleportPortalObject*>(object))
            teleportPlayer(layer, teleport, player);
        break;
    default:
        break;
    }
}

void checkSpawnObjects(GJBaseGameLayer* layer, PlayerObject* player) {
    if (!layer || !player || !layer->m_spawnObjects)
        return;

    int channel = layer->m_gameState.m_currentChannel;
    auto* objects = typeinfo_cast<cocos2d::CCArray*>(
        layer->m_spawnObjects->objectForKey(channel)
    );
    if (!objects)
        return;
    if (!channelHasTrajectorySpawnTriggers(channel, objects))
        return;

    int startingIndex = layer->m_gameState.m_spawnChannelRelated0[channel];
    bool goingBack = layer->m_gameState.m_spawnChannelRelated1[channel];
    CCPoint position = player->getPosition();

    unsigned int count = objects->count();
    for (int i = startingIndex; static_cast<unsigned int>(i) < count; i++) {
        ++s_passStats.spawnVisited;

        auto* object = static_cast<SpawnTriggerGameObject*>(objects->objectAtIndex(i));
        if (!object)
            continue;

        CCPoint objectPos = object->m_speedStart;
        if (player->m_isSideways) {
            if (goingBack) {
                if (objectPos.y < position.y)
                    break;
            } else if (objectPos.y > position.y) {
                break;
            }
        } else {
            if (goingBack) {
                if (objectPos.x < position.x)
                    break;
            } else if (objectPos.x > position.x) {
                break;
            }
        }

        if (!isTrajectorySpawnTrigger(object))
            continue;
        ++s_passStats.spawnCandidates;

        if (object->m_isGroupDisabled ||
            hasActivated(player, object) ||
            realPlayerHasActivated(player, object)) {
            continue;
        }

        if (!object->m_isTouchTriggered) {
            ++s_passStats.spawnTriggered;
            triggerObject(object, layer, player);
        }
    }
}

void collisionCheckObjects(GJBaseGameLayer* layer, PlayerObject* player, gd::vector<GameObject*>* objects, int objectCount, float dt) {
    if (!layer || !player || !objects || objectCount <= 0)
        return;

    CCRect playerRect = player->getObjectRect();
    OBB2D* playerBox = nullptr;
    bool playerBoxValid = false;

    auto refreshPlayerRect = [&] {
        playerRect = player->getObjectRect();
        playerBoxValid = false;
    };

    auto getPlayerBox = [&]() -> OBB2D* {
        if (!playerBoxValid) {
            player->updateOrientedBox();
            playerBox = player->getOrientedBox();
            playerBoxValid = true;
        }
        return playerBox;
    };

    int checkedObjects = std::min(objectCount, static_cast<int>(objects->size()));
    for (int i = 0; i < checkedObjects; i++) {
        GameObject* object = objects->at(i);
        if (!object)
            continue;

        ++s_passStats.objectsVisited;

        if (object->m_objectType == GameObjectType::Decoration ||
            object->m_objectType == GameObjectType::CollisionObject ||
            object->m_objectType == GameObjectType::SecretCoin ||
            object->m_objectType == GameObjectType::UserCoin ||
            object->m_objectType == GameObjectType::Collectible ||
            object->m_objectType == GameObjectType::EnterEffectObject ||
            object->m_objectID == 286 || object->m_objectID == 287 ||
            object->m_isGroupDisabled || object->m_isDisabled) {
            continue;
        }

        if (object->m_objectType == GameObjectType::Solid ||
            object->m_objectType == GameObjectType::Breakable) {
            ++s_passStats.solidQueued;
            if (layer->m_solidCollisionObjectsCount < layer->m_solidCollisionObjectsIndex) {
                layer->m_solidCollisionObjects.at(layer->m_solidCollisionObjectsCount) = object;
            } else {
                layer->m_solidCollisionObjects.push_back(object);
                layer->m_solidCollisionObjectsIndex++;
            }
            layer->m_solidCollisionObjectsCount++;
            continue;
        }

        if (object == layer->m_anticheatSpike)
            continue;

        if (object->m_objectType == GameObjectType::Hazard ||
            object->m_objectType == GameObjectType::AnimatedHazard) {
            ++s_passStats.hazardQueued;
            if (layer->m_hazardCollisionObjectsCount < layer->m_hazardCollisionObjectsIndex) {
                layer->m_hazardCollisionObjects.at(layer->m_hazardCollisionObjectsCount) = object;
            } else {
                layer->m_hazardCollisionObjects.push_back(object);
                layer->m_hazardCollisionObjectsIndex++;
            }
            layer->m_hazardCollisionObjectsCount++;
            continue;
        }

        CCRect objectRect = getCachedObjectRect(object);

        if (object->m_objectRadius <= 0.f) {
            if (!playerRect.intersectsRect(objectRect))
                continue;
        } else if (!layer->playerCircleCollision(player, object)) {
            continue;
        }

        bool overlaps = true;
        if (object->m_shouldUseOuterOb &&
            (!layer->m_levelSettings->m_fixRadiusCollision || object->m_objectRadius <= 0.f)) {
            ++s_passStats.obbChecks;
            OBB2D* box = getCachedObjectObb(object);
            OBB2D* boxPlayer = getPlayerBox();
            overlaps = box && boxPlayer && box->overlaps1Way(boxPlayer);
        }
        if (!overlaps)
            continue;

        if (object->m_objectType == GameObjectType::Slope) {
            if (!player->m_isSideways) {
                player->collidedWithSlopeInternal(dt, object, false);
            } else {
                CCRect emptyRect{0.f, 0.f, 0.f, 0.f};
                player->handleRotatedCollisionInternal(dt, object, emptyRect, false, false, true);
            }
            refreshPlayerRect();
            continue;
        }

        auto* effect = typeinfo_cast<EffectGameObject*>(object);
        if (!effect)
            continue;
        ++s_passStats.effectCandidates;

        if (hasActivated(player, effect) ||
            realPlayerHasActivated(player, effect)) {
            continue;
        }

        switch (object->m_objectType) {
        case GameObjectType::InverseGravityPortal:
            player->m_lastPortalPos = object->getPosition();
            player->m_lastActivatedPortal = object;
            activateForTrajectory(effect, player);
            flipGravity(player, true);
            refreshPlayerRect();
            break;
        case GameObjectType::NormalGravityPortal:
            player->m_lastPortalPos = object->getPosition();
            player->m_lastActivatedPortal = object;
            activateForTrajectory(effect, player);
            flipGravity(player, false);
            refreshPlayerRect();
            break;
        case GameObjectType::GravityTogglePortal:
            player->m_lastPortalPos = object->getPosition();
            player->m_lastActivatedPortal = object;
            activateForTrajectory(effect, player);
            flipGravity(player, !player->m_isUpsideDown);
            refreshPlayerRect();
            break;
        case GameObjectType::TeleportPortal:
            if (layer->canBeActivatedByPlayer(player, effect)) {
                teleportPlayer(layer, typeinfo_cast<TeleportPortalObject*>(object), player);
                activateForTrajectory(effect, player);
                refreshPlayerRect();
            }
            break;
        case GameObjectType::CustomRing:
        case GameObjectType::DashRing:
        case GameObjectType::DropRing:
        case GameObjectType::GravityDashRing:
        case GameObjectType::GravityRing:
        case GameObjectType::GreenRing:
        case GameObjectType::PinkJumpRing:
        case GameObjectType::RedJumpRing:
        case GameObjectType::SpiderOrb:
        case GameObjectType::YellowJumpRing:
        case GameObjectType::TeleportOrb: {
            auto* ring = typeinfo_cast<RingObject*>(object);
            if (!ring)
                break;
            if (!player->m_touchingRings->containsObject(object))
                player->m_touchingRings->addObject(object);
            player->m_touchedRings.insert(object->m_uniqueID);
            if (!player->m_isShip && !player->m_isBird && !player->m_isDart &&
                !player->m_isSwing && !ring->m_isSpawnOnly) {
                ringJump(player, ring);
                activateForTrajectory(effect, player);
            }
            break;
        }
        case GameObjectType::YellowJumpPad:
        case GameObjectType::PinkJumpPad:
        case GameObjectType::RedJumpPad:
        case GameObjectType::SpiderPad:
            bumpPlayerFromLayer(layer, player, effect);
            refreshPlayerRect();
            break;
        case GameObjectType::GravityPad: {
            bool facingDown = player->m_isSideways ? object->isFacingLeft() : object->isFacingDown();
            if (player->m_isUpsideDown == facingDown && layer->canBeActivatedByPlayer(player, effect)) {
                if (effect->m_isReverse)
                    player->reversePlayer(effect);
                player->m_lastPortalPos = object->getPosition();
                player->m_lastActivatedPortal = object;
                activateForTrajectory(effect, player);
                propellPlayer(player, 0.8f, false, static_cast<int>(GameObjectType::GravityPad));
                flipGravity(player, !facingDown);
                player->m_padRingRelated = true;
                refreshPlayerRect();
            }
            break;
        }
        case GameObjectType::MiniSizePortal:
            if (layer->canBeActivatedByPlayer(player, effect)) {
                player->m_lastPortalPos = object->getPosition();
                player->m_lastActivatedPortal = object;
                activateForTrajectory(effect, player);
                togglePlayerScale(player, true);
                refreshPlayerRect();
            }
            break;
        case GameObjectType::RegularSizePortal:
            if (layer->canBeActivatedByPlayer(player, effect)) {
                player->m_lastPortalPos = object->getPosition();
                player->m_lastActivatedPortal = object;
                activateForTrajectory(effect, player);
                togglePlayerScale(player, false);
                refreshPlayerRect();
            }
            break;
        case GameObjectType::CubePortal:
        case GameObjectType::ShipPortal:
        case GameObjectType::BallPortal:
        case GameObjectType::UfoPortal:
        case GameObjectType::WavePortal:
        case GameObjectType::SpiderPortal:
        case GameObjectType::SwingPortal:
        case GameObjectType::RobotPortal:
            activatePortal(layer, player, effect);
            refreshPlayerRect();
            break;
        case GameObjectType::Special:
            if (object->m_objectID == 0x743)
                player->m_stateHitHead = 2;
            else if (object->m_objectID == 0x6db)
                player->m_stateDartSlide = 2;
            else if (object->m_objectID == 0x715)
                player->m_stateNoAutoJump = 2;
            else if (object->m_objectID == 0x725 && player->m_isDashing) {
                stopDashing(player);
                player->m_jumpBuffered = false;
            } else if (object->m_objectID == 0xb32)
                player->m_stateFlipGravity = 2;
            else if (object->m_objectID == 2069 || object->m_objectID == 3645) {
                player->m_stateForce = 2;
                auto* forceBlock = typeinfo_cast<ForceBlockGameObject*>(object);
                if (!forceBlock)
                    break;

                int forceID = forceBlock->m_forceID;
                if (forceID > 0) {
                    if (player->m_jumpPadRelated.contains(forceID) &&
                        player->m_jumpPadRelated.at(forceID)) {
                        break;
                    }
                    player->m_jumpPadRelated.insert({forceID, true});
                }

                player->m_stateForceVector += forceBlock->calculateForceToTarget(player);
            }
            break;
        case GameObjectType::EnterEffectObject:
        case GameObjectType::Modifier:
            effect->activatedByPlayer(player);
            if (effect->m_isTouchTriggered)
                triggerObject(effect, layer, player);
            break;
        default:
            break;
        }
    }
}

void ringJump(PlayerObject* player, RingObject* ring) {
    if (!player || !ring || player->m_isDead)
        return;
    if (player->m_ringRelatedSet.find(ring->m_uniqueID) != player->m_ringRelatedSet.end())
        return;
    if (!player->m_stateRingJump2 || player->m_isDashing || !player->m_stateJumpBuffered)
        return;

    bool custom = ring->m_objectType == GameObjectType::CustomRing;
    bool teleport = ring->m_objectType == GameObjectType::TeleportOrb;
    if ((player->m_touchedRing || custom || teleport) &&
        (player->m_touchedCustomRing || ring->m_objectType != GameObjectType::CustomRing)) {
        if (player->m_touchedGravityPortal || ring->m_objectType != GameObjectType::TeleportOrb)
            return;
    }

    snapshotObject(ring);
    rememberActivated(player, ring);

    if (ring->m_isReverse)
        player->reversePlayer(ring);

    player->m_ringJumpRelated = true;
    player->m_ringRelatedSet.insert(ring->m_uniqueID);
    if (custom)
        player->m_touchedCustomRing = true;
    else if (teleport)
        player->m_touchedGravityPortal = true;
    else
        player->m_touchedRing = true;

    player->m_touchingRings->removeObject(ring, true);
    if (!custom && !teleport)
        player->m_padRingRelated = true;

    if (custom)
        return;

    if (teleport) {
        if (auto* teleportPortal = typeinfo_cast<TeleportPortalObject*>(ring))
            teleportPlayer(player->m_gameLayer, teleportPortal, player);
        return;
    }

    if (ring->m_objectType == GameObjectType::SpiderOrb) {
        bool facing = player->m_isSideways ? ring->isFacingLeft() : ring->isFacingDown();
        if (facing != player->m_isUpsideDown)
            flipGravity(player, !player->m_isUpsideDown);
        player->spiderTestJumpInternal(false);
        return;
    }

    if (ring->m_objectType == GameObjectType::DashRing) {
        if (auto* dashRing = typeinfo_cast<DashRingObject*>(ring))
            startDashing(player, dashRing);
        return;
    }

    if (ring->m_objectType == GameObjectType::GravityDashRing) {
        flipGravity(player, !player->m_isUpsideDown);
        if (auto* dashRing = typeinfo_cast<DashRingObject*>(ring))
            startDashing(player, dashRing);
        return;
    }

    player->m_stateRingJump = false;
    if (ring->m_objectType == GameObjectType::DropRing) {
        float velocity = gravitySign(player) * -15.f;
        if (player->m_isShip || player->m_isBird || player->m_isDart || player->m_isSwing)
            velocity = gravitySign(player) * -14.f;
        if (player->m_isBird)
            velocity *= 0.8f;
        else if (!player->m_isRobot && player->m_isSpider)
            velocity *= 1.1f;
        player->setYVelocity(velocity, 1);
        if (!player->m_isBall && !player->m_isLocked && !player->m_isDashing) {
            player->m_isRotating = false;
            player->m_isBallRotating2 = false;
            player->m_isBallRotating = false;
            player->m_rotationSpeed = 0.0;
            player->runNormalRotation(0, 1.0);
        } else {
            player->runBallRotation2();
        }
        player->m_hasEverHitRing = true;
        player->m_isAccelerating = true;
        if (player->m_isBall || player->m_isSwing)
            player->m_jumpBuffered = false;
        return;
    }

    float yStart = player->m_yStart;
    if (ring->m_objectType == GameObjectType::GravityRing)
        yStart *= 0.8f;
    else if (ring->m_objectType == GameObjectType::GreenRing && player->m_isShip)
        yStart *= 0.7f;
    else if (ring->m_objectType == GameObjectType::PinkJumpRing)
        yStart *= player->m_isShip ? 0.37f : player->m_isBird ? 0.42f : player->m_isBall ? 0.77f : 0.72f;
    else if (ring->m_objectType == GameObjectType::RedJumpRing) {
        player->m_isAccelerating = true;
        yStart *= player->m_isShip ? 1.4f : player->m_isBird ? (player->m_vehicleSize == 1.f ? 1.02f : 1.36f) : player->m_isBall ? 1.34f : player->m_isRobot ? 1.28f : player->m_isSpider ? 1.34f : 1.38f;
    } else if (player->m_isRobot) {
        yStart *= 0.9f;
    }

    if (ring->m_objectType == GameObjectType::GreenRing)
        flipGravity(player, !player->m_isUpsideDown);

    float sizeGravity = player->m_vehicleSize != 1.f ? 0.8f : 1.f;
    player->setYVelocity(gravitySign(player) * yStart * sizeGravity, gravitySign(player));
    player->m_maybeIsBoosted = true;
    player->m_isOnGround2 = false;
    player->m_isOnGround = false;
    player->m_lastGroundedPos = player->getPosition();
    player->m_hasEverHitRing = true;

    if (!player->m_isBall && !player->m_isLocked && !player->m_isDashing) {
        player->m_isRotating = false;
        player->m_isBallRotating2 = false;
        player->m_isBallRotating = false;
        player->m_rotationSpeed = 0.0;
        player->runNormalRotation(0, 1.0);
    } else {
        player->runBallRotation2();
    }

    if (player->m_isSwing)
        player->m_yVelocity *= 0.6;
    else if (player->m_isBall || player->m_isSpider)
        player->m_yVelocity *= 0.7;

    if (ring->m_objectType == GameObjectType::GravityRing)
        flipGravity(player, !player->m_isUpsideDown);
    if (ring->m_objectType == GameObjectType::RedJumpRing)
        player->m_isAccelerating = true;
}

void togglePlayerScale(PlayerObject* player, bool smallSize) {
    if (!player)
        return;
    if (player->m_vehicleSize == 1.f && !smallSize)
        return;
    if (smallSize && player->m_vehicleSize != 1.f)
        return;

    if (smallSize) {
        player->m_vehicleSize = 0.6f;
    } else {
        if (player->m_isPlatformer)
            player->m_stateScale = 2;
        player->m_vehicleSize = 1.f;
    }

    player->m_spriteWidthScale = player->m_vehicleSize;
    player->m_spriteHeightScale = player->m_vehicleSize;
    player->setScaleX(player->m_vehicleSize);
    player->setScaleY(player->m_vehicleSize);
    player->updatePlayerScale();
}
}
