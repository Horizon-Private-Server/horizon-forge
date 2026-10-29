#include <libdl/collision.h>
#include <libdl/game.h>
#include <libdl/gamesettings.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/net.h>
#include <libdl/pad.h>
#include <libdl/string.h>
#include <libdl/time.h>
#include <libdl/utils.h>

#include "wallwalk.h"
#include "skills.h"
#include "game.h"
#include "utils.h"
#include "common.h"

#define WALLWALK_JUMP_BUFFER_TICKS       (8)
#define WALLWALK_REFRESH_MS              (TIME_SECOND / 10)
#define WALLWALK_TIMEOUT_MS              (TIME_SECOND / 2)
#define WALLWALK_PROBE_LENGTH            (1.5f)
#define WALLWALK_CONTACT_GRACE_MS        (TIME_SECOND / 10)

// These fields are absent from libdl's Player view. Ghidra confirms the surface
// classifier owns 0x430/0x437/0x438 and movement owns the gravity-mode byte.
#define WALLWALK_SURFACE(p)              (*(short*)((u8*)(p) + 0x430))
#define WALLWALK_MAGNET_FLAG(p)          (*(u8*)((u8*)(p) + 0x437))
#define WALLWALK_NOWALK_FLAG(p)          (*(u8*)((u8*)(p) + 0x438))
#define WALLWALK_GRAVITY_MODE(p)         (*(u8*)((u8*)(p) + 0x2659))

struct WallwalkState
{
  Player* Player;
  int ClassType;
  int Active;
  int Applied;
  int CanReattach;
  int BudgetTicks;
  int UsedTicks;
  int JumpHeldTicks;
  int LastUpdateTime;
  int LastSendTime;
  int LastReceiveTime;
  int LastContactTime;
  int RemoteTicks;
  int HasSequence;
  u32 Sequence;
  short NativeMagnetic;
  short NativeSurface;
  short Surface;
  u8 NativeMagnetFlag;
  u8 NativeNowalkFlag;
  HeroGround NativeGround;
  VECTOR Normal;
  VECTOR Point;
  VECTOR LastPosition;
} wallwalkStates[GAME_MAX_PLAYERS];

// Map-owned protocol; native player messages continue to own position updates.
struct WallwalkMessage
{
  int ClientId;
  int PlayerId;
  int AccountId;
  int ClassType;
  int MapLoadTime;
  u32 Sequence;
  int Active;
  int RemainingTicks;
  float Normal[3];
};

typedef char WallwalkMessageSizeCheck[sizeof(struct WallwalkMessage) == 44 ? 1 : -1];

u32 wallwalkSequence;

//--------------------------------------------------------------------------
struct WallwalkState* wallwalkGetState(Player* player)
{
  if (!player || player->PlayerId < 0 || player->PlayerId >= GAME_MAX_PLAYERS) return NULL;

  return &wallwalkStates[player->PlayerId];
}

//--------------------------------------------------------------------------
int wallwalkCanMove(Player* player)
{
  if (!skillsPlayerIsReady(player) || playerIsDead(player) || player->Health <= 0
      || player->InVehicle || player->timers.noInput || player->timers.postTeleportTimer)
    return 0;

  // Remote players use the owner's attachment, never this client's menu/input.
  if (player->IsLocal && (MapConfig.State->MenuOpen != RAIDS_CUSTOM_MENU_NONE || shouldCloseMenu()))
    return 0;

  switch (player->PlayerState)
  {
    case PLAYER_STATE_IDLE:
    case PLAYER_STATE_LOOK:
    case PLAYER_STATE_WALK:
    case PLAYER_STATE_SKID:
    case PLAYER_STATE_CROUCH:
    case PLAYER_STATE_QUICK_TURN:
    case PLAYER_STATE_FALL:
    case PLAYER_STATE_JUMP:
    case PLAYER_STATE_RUN_JUMP:
    case PLAYER_STATE_DOUBLE_JUMP:
    case PLAYER_STATE_FLIP_JUMP:
    case PLAYER_STATE_COMBO_ATTACK:
    case PLAYER_STATE_THROW_ATTACK:
    case PLAYER_STATE_GET_HIT:
    case PLAYER_STATE_RECOIL:
    case PLAYER_STATE_SLIDE:
    case PLAYER_STATE_TARGETING:
    case PLAYER_STATE_GUN_WAITING:
      return 1;
    default:
      return 0;
  }
}

//--------------------------------------------------------------------------
int wallwalkIsActive(Player* player)
{
  struct WallwalkState* state = wallwalkGetState(player);
  if (!state || state->Player != player || !state->Active || !wallwalkCanMove(player)) return 0;

  RaidsPlayerClassProgress_t* playerClass = bankGetPlayerEquippedClass(player->PlayerId);
  return playerClass && playerClass->ClassType == state->ClassType;
}

//--------------------------------------------------------------------------
int wallwalkIsSolidSurface(int surface)
{
  switch (surface)
  {
    case COLLISION_SURFACE_WALKABLE_1:
    case COLLISION_SURFACE_WALKABLE_2:
    case COLLISION_SURFACE_WALKABLE_3:
    case COLLISION_SURFACE_WALKABLE_4:
    case -1: // CollHotspot caches the default 0xF material as -1.
      return 1;
    default:
      return 0; // Native magnet walls, hazards, liquids and no-walk surfaces.
  }
}

//--------------------------------------------------------------------------
int wallwalkValidNormal(VECTOR normal)
{
  float lengthSq = vector_sqrmag(normal);
  return lengthSq > 0.99f && lengthSq < 1.01f;
}

//--------------------------------------------------------------------------
int wallwalkDurationTicks(float seconds)
{
  // All configured ranks are much smaller; bound before converting network or
  // bank-derived floats to integers, including NaN/infinity.
  return seconds > 0 && seconds < 3600 ? (int)(seconds * TPS) : 0;
}

//--------------------------------------------------------------------------
void wallwalkSend(struct WallwalkState* state)
{
  Player* player = state->Player;
  GameSettings* gs = gameGetSettings();
  void* connection = netGetDmeServerConnection();
  if (!player || !player->IsLocal || !gs || !connection) return;

  struct WallwalkMessage msg = {
    .ClientId = gameGetMyClientId(),
    .PlayerId = player->PlayerId,
    .AccountId = gs->PlayerAccountIds[player->PlayerId],
    .ClassType = state->ClassType,
    .MapLoadTime = gs->GameLoadStartTime,
    .Sequence = ++wallwalkSequence,
    .Active = state->Active,
    .RemainingTicks = state->BudgetTicks - state->UsedTicks
  };
  if (msg.RemainingTicks <= 0) msg.Active = 0;
  memcpy(msg.Normal, state->Normal, sizeof(msg.Normal));

  // Repeat active snapshots for late joiners; sequence rejects reordered packets.
  netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, connection,
    CUSTOM_MSG_WALLWALK_STATE, sizeof(msg), &msg);
  state->LastSendTime = gameGetTime();
}

//--------------------------------------------------------------------------
void wallwalkStop(struct WallwalkState* state)
{
  Player* player = state->Player;
  int wasActive = state->Active;
  state->Active = 0;

  if (state->Applied && player) {
    memcpy(&player->Ground, &state->NativeGround, sizeof(player->Ground));
    WALLWALK_SURFACE(player) = state->NativeSurface;
    player->Ground.magnetic = state->NativeMagnetic;
    WALLWALK_MAGNET_FLAG(player) = state->NativeMagnetFlag;
    WALLWALK_NOWALK_FLAG(player) = state->NativeNowalkFlag;
    WALLWALK_GRAVITY_MODE(player) = ((short (*)(Player*))0x005F1D20)(player);
    if (!WALLWALK_GRAVITY_MODE(player)) {
      vector_copy(player->Ground.normal, (VECTOR){0, 0, 1, 0});
      vector_copy(player->Ground.gravity, (VECTOR){0, 0, -1, 0});
    }
    player->Ground.onGood = 0;
    player->Ground.offGood = 1;
    state->Applied = 0;
  }
  if (wasActive) wallwalkSend(state);
}

//--------------------------------------------------------------------------
void wallwalkResetPlayer(Player* player)
{
  struct WallwalkState* state = wallwalkGetState(player);
  if (!state) return;

  if (state->Player == player) wallwalkStop(state);
  memset(state, 0, sizeof(*state));
  state->ClassType = -1;
  state->LastUpdateTime = -1;
}

//--------------------------------------------------------------------------
// Acquisition accepts feet or torso contact without requiring matching planes.
// Once attached, probe at the feet along the wall normal: the body's up axis
// rotates away from the wall during alignment. Native movement handles angles.
// Return 0 for a gap, -1 for an unsuitable hit, and 1 for solid support.
int wallwalkProbe(Player* player, VECTOR direction, int acquiring, struct WallwalkState* state)
{
  VECTOR up = {player->WorldMatrix[8], player->WorldMatrix[9], player->WorldMatrix[10], 0};
  if (!acquiring) vector_scale(up, direction, -1);
  int probeCount = acquiring ? 2 : 1;
  int i;
  for (i = 0; i < probeCount; ++i) {
    VECTOR from, to, offset;
    vector_scale(offset, up, i ? 1.0f : 0.15f);
    vector_add(from, player->PlayerPosition, offset);
    vector_scale(offset, direction, WALLWALK_PROBE_LENGTH);
    vector_add(to, from, offset);
    if (!CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, player->PlayerMoby, NULL))
      continue;
    if (CollLine_Fix_GetHitMoby()
        || !wallwalkIsSolidSurface(CollLine_Fix_GetHitCollisionId() & 0x0F))
      return -1;

    VECTOR normal, delta;
    vector_normalize(normal, CollLine_Fix_GetHitNormal());
    if (!wallwalkValidNormal(normal)) return -1;
    vector_subtract(delta, player->PlayerPosition, CollLine_Fix_GetHitPosition());
    float distance = vector_innerproduct_unscaled(delta, normal);
    if (!(fabsf(distance) <= WALLWALK_PROBE_LENGTH)) return -1;
    vector_scale(delta, normal, distance);
    vector_subtract(state->Point, player->PlayerPosition, delta);
    vector_copy(state->Normal, normal);
    state->Surface = CollLine_Fix_GetHitCollisionId() & 0x0F;
    return 1;
  }
  return 0;
}

//--------------------------------------------------------------------------
int wallwalkRefreshContact(Player* player, VECTOR direction, struct WallwalkState* state)
{
  int result = wallwalkProbe(player, direction, 0, state);
  int now = gameGetTime();
  if (result > 0) {
    state->LastContactTime = now;
    return 1;
  }
  if (result < 0 || !state->Active
      || (int)((u32)now - (u32)state->LastContactTime) >= WALLWALK_CONTACT_GRACE_MS)
    return 0;

  // A brief missed cast at a seam must not toggle gravity/camera mode. Keep the
  // previous plane only while the feet remain nearby; the normal budget runs.
  VECTOR delta;
  vector_subtract(delta, player->PlayerPosition, state->Point);
  float distance = vector_innerproduct_unscaled(delta, state->Normal);
  if (!(fabsf(distance) <= WALLWALK_PROBE_LENGTH)) return 0;
  vector_scale(delta, state->Normal, distance);
  vector_subtract(state->Point, player->PlayerPosition, delta);
  return 1;
}

//--------------------------------------------------------------------------
void wallwalkApplyGround(Player* player, struct WallwalkState* state)
{
  if (!state->Applied) {
    memcpy(&state->NativeGround, &player->Ground, sizeof(state->NativeGround));
    state->NativeSurface = WALLWALK_SURFACE(player);
    state->NativeMagnetic = player->Ground.magnetic;
    state->NativeMagnetFlag = WALLWALK_MAGNET_FLAG(player);
    state->NativeNowalkFlag = WALLWALK_NOWALK_FLAG(player);
    state->Applied = 1;
  }

  vector_copy(player->Ground.normal, state->Normal);
  vector_scale(player->Ground.gravity, state->Normal, -1);
  vector_copy(player->Ground.point, state->Point);
  VECTOR delta;
  vector_subtract(delta, player->PlayerPosition, state->Point);
  player->Ground.dist = vector_innerproduct_unscaled(delta, state->Normal);
  player->Ground.height = state->Point[2];
  player->Ground.slope = 0;
  player->Ground.pMoby = NULL;
  player->Ground.magnetic = 1;
  // This is the player's contact cache, not the collision mesh/material. Native
  // casts can still point at a distant floor while alignment turns toward a wall.
  WALLWALK_SURFACE(player) = state->Surface;
  player->Ground.offAny = 0;
  player->Ground.offGood = 0;
  if (player->Ground.onGood <= 0) player->Ground.onGood = 1;
  WALLWALK_MAGNET_FLAG(player) = 1;
  WALLWALK_NOWALK_FLAG(player) = 0;
}

//--------------------------------------------------------------------------
short wallwalkGetGravityMode(Player* player)
{
  if (wallwalkIsActive(player)) {
    wallwalkApplyGround(player, wallwalkGetState(player));
    return 1;
  }
  struct WallwalkState* state = wallwalkGetState(player);
  if (state && state->Player == player && state->Active) wallwalkStop(state);

  return ((short (*)(Player*))0x005F1D20)(player);
}

//--------------------------------------------------------------------------
void wallwalkGetGroundInfo(Player* player, int force)
{
  ((void (*)(Player*, int))0x005E5A60)(player, force);
  if (!wallwalkIsActive(player)) return;

  struct WallwalkState* state = wallwalkGetState(player);
  memcpy(&state->NativeGround, &player->Ground, sizeof(state->NativeGround));
  state->NativeGround.magnetic = state->NativeMagnetic;
  state->NativeSurface = WALLWALK_SURFACE(player);
  // Prefer the game's own supported contact after alignment. Surface angle is
  // the native controller's responsibility once the player is attached.
  if (player->Ground.offGood == 0 && player->Ground.onGood > 0 && !player->Ground.pMoby
      && wallwalkIsSolidSurface(state->NativeSurface) && wallwalkValidNormal(player->Ground.normal)
      && vector_sqrdistance(player->PlayerPosition, player->Ground.point) <= 0.0625f) {
    vector_copy(state->Normal, player->Ground.normal);
    vector_copy(state->Point, player->Ground.point);
    state->Surface = state->NativeSurface;
    state->LastContactTime = gameGetTime();
    wallwalkApplyGround(player, state);
    return;
  }

  VECTOR direction;
  vector_scale(direction, state->Normal, -1);
  if (!wallwalkRefreshContact(player, direction, state)) {
    wallwalkStop(state);
    return;
  }
  wallwalkApplyGround(player, state);
}

//--------------------------------------------------------------------------
void wallwalkClassifySurface(Player* player)
{
  ((void (*)(Player*))0x005D9F28)(player);
  struct WallwalkState* state = wallwalkGetState(player);
  if (!state || state->Player != player || !state->Active) return;

  // Preserve native flags for detach; the surface cache already describes the
  // verified wall, so the original classifier still handles its normal work.
  state->NativeMagnetic = player->Ground.magnetic;
  state->NativeMagnetFlag = WALLWALK_MAGNET_FLAG(player);
  state->NativeNowalkFlag = WALLWALK_NOWALK_FLAG(player);
  if (!wallwalkIsActive(player) || WALLWALK_NOWALK_FLAG(player)
      || !wallwalkIsSolidSurface(WALLWALK_SURFACE(player))) {
    wallwalkStop(state);
    return;
  }
  wallwalkApplyGround(player, state);
}

//--------------------------------------------------------------------------
int wallwalkOnState(void* connection, void* data)
{
  struct WallwalkMessage msg;
  memcpy(&msg, data, sizeof(msg));
  GameSettings* gs = gameGetSettings();
  if (!gs || msg.PlayerId < 0 || msg.PlayerId >= GAME_MAX_PLAYERS || msg.AccountId <= 0
      || (msg.Active != 0 && msg.Active != 1)
      || msg.ClientId == gameGetMyClientId() || msg.MapLoadTime != gs->GameLoadStartTime
      || (s8)gs->PlayerClients[msg.PlayerId] != msg.ClientId
      || gs->PlayerAccountIds[msg.PlayerId] != msg.AccountId)
    return sizeof(msg);

  Player* player = playerGetFromIndex(msg.PlayerId);
  if (!player || player->IsLocal || !skillsPlayerIsReady(player)) return sizeof(msg);

  RaidsPlayerClassProgress_t* playerClass = bankGetPlayerEquippedClass(player->PlayerId);
  struct WallwalkState* state = wallwalkGetState(player);
  if (playerClass->ClassType != msg.ClassType) return sizeof(msg);
  if (state->Player != player || state->ClassType != msg.ClassType) {
    wallwalkResetPlayer(player);
    state->Player = player;
    state->ClassType = msg.ClassType;
    vector_copy(state->LastPosition, player->PlayerPosition);
  }
  if (state->HasSequence && (s32)(msg.Sequence - state->Sequence) <= 0) return sizeof(msg);

  VECTOR normal = {msg.Normal[0], msg.Normal[1], msg.Normal[2], 0};
  int limit = wallwalkDurationTicks(skillsGetPlayerBonus(player, RAIDS_CLASS_SKILL_WALL_WALKING));
  if (msg.Active && (!wallwalkValidNormal(normal) || msg.RemainingTicks <= 0 || msg.RemainingTicks > limit))
    return sizeof(msg);

  state->Sequence = msg.Sequence;
  state->HasSequence = 1;
  state->LastReceiveTime = gameGetTime();
  state->RemoteTicks = msg.Active ? msg.RemainingTicks : 0;
  if (!msg.Active || !wallwalkCanMove(player)) {
    wallwalkStop(state);
  } else {
    VECTOR direction;
    vector_scale(direction, normal, -1);
    // Contact is reconstructed at the replicated position; never move a remote
    // player to the owner's previous position or grant it a local input budget.
    if (wallwalkRefreshContact(player, direction, state)) {
      state->Active = 1;
    } else {
      wallwalkStop(state);
    }
  }
  return sizeof(msg);
}

//--------------------------------------------------------------------------
void wallwalkUpdate(Player* player, int classType, float durationSeconds, int canControl)
{
  struct WallwalkState* state = wallwalkGetState(player);
  if (!state) return;

  if (state->Player != player || state->ClassType != classType) {
    wallwalkResetPlayer(player);
    state->Player = player;
    state->ClassType = classType;
    vector_copy(state->LastPosition, player->PlayerPosition);
  }

  int now = gameGetTime();
  if (state->LastUpdateTime == now) return;
  state->LastUpdateTime = now;

  int duration = wallwalkDurationTicks(durationSeconds);
  int teleported = vector_sqrdistance(state->LastPosition, player->PlayerPosition) > 64;
  vector_copy(state->LastPosition, player->PlayerPosition);
  if (!wallwalkCanMove(player) || duration <= 0 || teleported) {
    wallwalkStop(state);
    state->JumpHeldTicks = 0;
    state->CanReattach = 0;
    if (teleported) state->BudgetTicks = 0;
    return;
  }
  if (!player->IsLocal) {
    int age = (int)((u32)now - (u32)state->LastReceiveTime);
    if (age >= WALLWALK_TIMEOUT_MS || age * TPS >= state->RemoteTicks * TIME_SECOND)
      wallwalkStop(state);
    return;
  }

  if (!canControl) {
    wallwalkStop(state);
    state->JumpHeldTicks = 0;
    state->CanReattach = 0;
    return;
  }
  if (state->BudgetTicks > duration) state->BudgetTicks = duration;

  // Synthetic ground is never a recharge. A genuine world-up floor landing is.
  int onFloor = !state->Active && !state->Applied && player->Ground.onGood > 0
    && player->Ground.offGood == 0 && player->Ground.normal[2] >= 0.6427876f
    && wallwalkIsSolidSurface(WALLWALK_SURFACE(player)) && !WALLWALK_NOWALK_FLAG(player);
  if (onFloor) {
    state->BudgetTicks = duration;
    state->UsedTicks = 0;
    state->CanReattach = 0;
  }

  int mappedJump = padGetMappedPad(0x40, player->PlayerId);
  int held = mappedJump > 0 && playerPadGetAnyButton(player, mappedJump << 8) > 0;
  if (!held) {
    state->JumpHeldTicks = 0;
    if (state->Active) state->CanReattach = state->UsedTicks < state->BudgetTicks;
    wallwalkStop(state);
    return;
  }
  if (state->JumpHeldTicks < WALLWALK_JUMP_BUFFER_TICKS) ++state->JumpHeldTicks;
  if (state->UsedTicks >= state->BudgetTicks) {
    wallwalkStop(state);
    state->CanReattach = 0;
    return;
  }

  VECTOR direction;
  if (state->Active) {
    vector_scale(direction, state->Normal, -1);
    if (!wallwalkRefreshContact(player, direction, state)) {
      wallwalkStop(state);
      return;
    }
  } else {
    if (onFloor || player->Ground.magnetic || state->JumpHeldTicks < WALLWALK_JUMP_BUFFER_TICKS) return;

    // A released wall can be resumed without pushing into it again. Require a
    // fresh hit; the attached seam grace must never acquire missing terrain.
    int hasContact = 0;
    if (state->CanReattach) {
      vector_scale(direction, state->Normal, -1);
      hasContact = wallwalkProbe(player, direction, 0, state) == 1;
    }
    if (!hasContact) {
      // GetIdealThrustFromPad reads these stick axes and publishes the world-space
      // direction at 0x1790. Its 0x1780 thrust is scaled by native movement speed.
      float* input = (float*)((u8*)player + 0x120);
      if (!(input[0] * input[0] + input[1] * input[1] > 0.01f)) return;
      memcpy(direction, (u8*)player + 0x1790, sizeof(direction));
      direction[2] = direction[3] = 0;
      if (!(vector_sqrmag(direction) > 0.01f)) return;
      vector_normalize(direction, direction);
      if (wallwalkProbe(player, direction, 1, state) != 1) return;
    }

    state->Active = 1;
    state->CanReattach = 0;
    state->LastContactTime = now;
    state->LastSendTime = now - WALLWALK_REFRESH_MS;
    wallwalkApplyGround(player, state);
    WALLWALK_GRAVITY_MODE(player) = 1;
    // End the airborne animation once on attachment; subsequent transitions,
    // movement, shooting and alignment remain the native player's responsibility.
    PlayerVTable* vtable = playerGetVTable(player);
    if (vtable && vtable->UpdateState)
      vtable->UpdateState(player, PLAYER_STATE_WALK, 1, 0, 1);
  }

  wallwalkApplyGround(player, state);
  WALLWALK_GRAVITY_MODE(player) = 1;
  ++state->UsedTicks;
  if ((int)((u32)now - (u32)state->LastSendTime) >= WALLWALK_REFRESH_MS) wallwalkSend(state);
}

//--------------------------------------------------------------------------
float wallwalkGetCooldown(Player* player)
{
  struct WallwalkState* state = wallwalkGetState(player);
  if (!state || state->Player != player || state->BudgetTicks <= 0) return 1;

  return minf(1, state->UsedTicks / (float)state->BudgetTicks);
}

//--------------------------------------------------------------------------
void wallwalkInit(void)
{
  memset(wallwalkStates, 0, sizeof(wallwalkStates));
  wallwalkSequence = gameGetTime();
  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    wallwalkStates[i].ClassType = -1;
    wallwalkStates[i].LastUpdateTime = -1;
  }

  netInstallCustomMsgHandler(CUSTOM_MSG_WALLWALK_STATE, &wallwalkOnState);

  // All sites are in code.0006 (0x003A1580..0x0062F028), reloaded per map.
  HOOK_JAL(0x005E3D28, &wallwalkGetGravityMode);
  HOOK_JAL(0x005E7640, &wallwalkGetGroundInfo);
  HOOK_JAL(0x005E7668, &wallwalkGetGroundInfo);
  HOOK_JAL(0x005E7A48, &wallwalkGetGroundInfo);
  HOOK_JAL(0x0060E4D4, &wallwalkClassifySurface);
  HOOK_JAL(0x00612C50, &wallwalkClassifySurface);
}

//--------------------------------------------------------------------------
void wallwalkCleanup(void)
{
  netUninstallCustomMsgHandler(CUSTOM_MSG_WALLWALK_STATE, &wallwalkOnState);
}
