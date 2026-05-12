#pragma once

#include "../includes.hpp"

namespace xdbot::trajectory_physics {
void flipGravity(PlayerObject* player, bool gravity);
void propellPlayer(PlayerObject* player, float force, bool noEffects, int objectType);
void bumpPlayer(PlayerObject* player, float force, int objectType, bool noEffects, GameObject* object);
void startDashing(PlayerObject* player, DashRingObject* ring);
void stopDashing(PlayerObject* player);
void ringJump(PlayerObject* player, RingObject* ring);
void togglePlayerScale(PlayerObject* player, bool smallSize);
void teleportPlayer(GJBaseGameLayer* layer, TeleportPortalObject* object, PlayerObject* player);
}
