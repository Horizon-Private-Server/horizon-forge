#include <float.h>

#include <libdl/game.h>
#include <libdl/gamesettings.h>
#include <libdl/guber.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/moby.h>
#include <libdl/net.h>
#include <libdl/player.h>
#include <libdl/string.h>
#include <libdl/time.h>
#include <libdl/utils.h>
#include <libdl/weapon.h>

#include "powerups.h"
#include "skills.h"
#include "bank.h"
#include "game.h"
#include "utils.h"
#include "mobs/mob.h"

#define POWERUP_FRENZY_WINDOW_MS           (6 * TIME_SECOND)
#define POWERUP_IRONCLAD_WINDOW_MS         (3 * TIME_SECOND)
#define POWERUP_WINDOW_CAPACITY            (6 * TPS + 1)
#define POWERUP_FRENZY_THRESHOLD           (10.0f)
#define POWERUP_IRONCLAD_THRESHOLD         (0.30f)
#define POWERUP_FRENZY_COOLDOWN_MS         (20 * TIME_SECOND)
#define POWERUP_IRONCLAD_COOLDOWN_MS       (30 * TIME_SECOND)
#define POWERUP_REFRESH_MS                 (250)
#define POWERUP_CLOSE_RANGE_SQR            (15.0f * 15.0f)

enum SkillPowerupType
{
  SKILL_POWERUP_QUAD,
  SKILL_POWERUP_SHIELD,
  SKILL_POWERUP_COUNT
};

struct PowerupWindowEntry
{
  int Time;
  float Amount;
};

// Damage events in the same frame share a bucket. Overflow drops old progress,
// never awards extra progress; all memory is bounded even under extreme load.
struct PowerupWindow
{
  struct PowerupWindowEntry Entries[POWERUP_WINDOW_CAPACITY];
  int Head;
  int Count;
  float Total;
} powerupWindows[SKILL_POWERUP_COUNT];

struct SkillPowerupEffect
{
  int Active;
  int EndsAt;
  int DurationTicks;
  int HasCooldown;
  int CooldownUntil;
  int Applied;
};

struct SkillPowerupState
{
  Player* Player;
  int AccountId;
  int ClassType;
  int HasSequence;
  u32 Sequence;
  int LastSendTime;
  struct SkillPowerupEffect Effects[SKILL_POWERUP_COUNT];
} powerupStates[GAME_MAX_PLAYERS];

// Absolute deadlines prevent repeated snapshots from extending a powerup.
struct SkillPowerupMessage
{
  int ClientId;
  int PlayerId;
  int AccountId;
  int ClassType;
  int MapLoadTime;
  u32 Sequence;
  int ActiveMask;
  int EndsAt[SKILL_POWERUP_COUNT];
  int DurationTicks[SKILL_POWERUP_COUNT];
};

typedef char SkillPowerupMessageSizeCheck[sizeof(struct SkillPowerupMessage) == 44 ? 1 : -1];

u32 powerupSequence;

//--------------------------------------------------------------------------
int powerupsRemaining(int deadline, int now)
{
  return (s32)((u32)deadline - (u32)now);
}

//--------------------------------------------------------------------------
float powerupsWindowPrune(struct PowerupWindow* window, int now, int duration)
{
  while (window->Count > 0) {
    struct PowerupWindowEntry* entry = &window->Entries[window->Head];
    if ((u32)now - (u32)entry->Time < (u32)duration)
      break;

    window->Total -= entry->Amount;
    window->Head = (window->Head + 1) % POWERUP_WINDOW_CAPACITY;
    --window->Count;
  }
  if (!window->Count || window->Total < 0)
    window->Total = 0;

  return window->Total;
}

//--------------------------------------------------------------------------
float powerupsWindowAdd(struct PowerupWindow* window, int now, int duration, float amount)
{
  powerupsWindowPrune(window, now, duration);
  if (!(amount > 0 && amount <= FLT_MAX))
    return window->Total;

  int tail = (window->Head + window->Count - 1 + POWERUP_WINDOW_CAPACITY) % POWERUP_WINDOW_CAPACITY;
  if (window->Count > 0 && window->Entries[tail].Time == now) {
    window->Entries[tail].Amount += amount;
  } else {
    if (window->Count == POWERUP_WINDOW_CAPACITY) {
      window->Total -= window->Entries[window->Head].Amount;
      window->Head = (window->Head + 1) % POWERUP_WINDOW_CAPACITY;
      --window->Count;
    }
    tail = (window->Head + window->Count) % POWERUP_WINDOW_CAPACITY;
    window->Entries[tail].Time = now;
    window->Entries[tail].Amount = amount;
    ++window->Count;
  }

  window->Total += amount;
  return window->Total;
}

//--------------------------------------------------------------------------
enum RaidsClassSkillId powerupsSkillId(int type)
{
  return type == SKILL_POWERUP_QUAD
    ? RAIDS_CLASS_SKILL_BLOOD_FRENZY : RAIDS_CLASS_SKILL_IRONCLAD;
}

//--------------------------------------------------------------------------
int powerupsDurationTicks(Player* player, int type)
{
  float seconds = skillsGetPlayerBonus(player, powerupsSkillId(type));
  return seconds > 0 && seconds <= 60 ? (int)(seconds * TPS) : 0;
}

//--------------------------------------------------------------------------
int powerupsNativeTimer(Player* player, int type)
{
  return type == SKILL_POWERUP_QUAD
    ? player->timers.damageMuliplierTimer : player->timers.armorLevelTimer;
}

//--------------------------------------------------------------------------
int powerupsNativeActive(Player* player, int type)
{
  return powerupsNativeTimer(player, type) > 0
    && (type == SKILL_POWERUP_QUAD ? player->DamageMultiplier > 1 : player->ArmorLevel > 0);
}

//--------------------------------------------------------------------------
int powerupsEffectTicks(struct SkillPowerupEffect* effect, int now)
{
  int ms = powerupsRemaining(effect->EndsAt, now);
  return effect->Active && ms > 0 ? (ms * TPS + TIME_SECOND - 1) / TIME_SECOND : 0;
}

//--------------------------------------------------------------------------
void powerupsStopEffect(Player* player, struct SkillPowerupEffect* effect, int type)
{
  // Do not remove a native pickup that has extended/replaced our grant.
  int expected = powerupsEffectTicks(effect, gameGetTime());
  if (player && effect->Applied && powerupsNativeTimer(player, type) <= expected + 2) {
    if (type == SKILL_POWERUP_QUAD && player->DamageMultiplier == 4) {
      player->timers.damageMuliplierTimer = 0;
      player->DamageMultiplier = 1;
    } else if (type == SKILL_POWERUP_SHIELD && player->ArmorLevel == 3) {
      player->timers.armorLevelTimer = 0;
      player->ArmorLevel = 0;
    }
  }

  effect->Active = 0;
  effect->Applied = 0;
}

//--------------------------------------------------------------------------
void powerupsApplyEffect(Player* player, struct SkillPowerupEffect* effect, int type)
{
  int ticks = powerupsEffectTicks(effect, gameGetTime());
  if (ticks <= 0 || powerupsNativeTimer(player, type) > ticks)
    return;

  if (type == SKILL_POWERUP_QUAD) {
    // A stronger independent multiplier also belongs to its original source.
    if (player->DamageMultiplier > 4)
      return;

    player->DamageMultiplier = 4;
    player->timers.damageMuliplierTimer = ticks;
  } else {
    if (player->ArmorLevel > 3)
      return;

    player->ArmorLevel = 3;
    player->timers.armorLevelTimer = ticks;
  }

  effect->Applied = 1;
}

//--------------------------------------------------------------------------
void powerupsSend(struct SkillPowerupState* state)
{
  GameSettings* gs = gameGetSettings();
  void* connection = netGetDmeServerConnection();
  Player* player = state->Player;
  if (!player || !player->IsLocal || !gs || !connection)
    return;

  struct SkillPowerupMessage msg = {
    .ClientId = gameGetMyClientId(),
    .PlayerId = player->PlayerId,
    .AccountId = state->AccountId,
    .ClassType = state->ClassType,
    .MapLoadTime = gs->GameLoadStartTime,
    .Sequence = ++powerupSequence
  };
  int i;
  for (i = 0; i < SKILL_POWERUP_COUNT; ++i) {
    if (state->Effects[i].Active) {
      msg.ActiveMask |= 1 << i;
      msg.EndsAt[i] = state->Effects[i].EndsAt;
      msg.DurationTicks[i] = state->Effects[i].DurationTicks;
    }
  }

  netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL | NET_ORDER_CRITICAL, connection,
    CUSTOM_MSG_SKILL_POWERUP_STATE, sizeof(msg), &msg);
  state->LastSendTime = gameGetTime();
}

//--------------------------------------------------------------------------
struct SkillPowerupState* powerupsSyncPlayer(Player* player)
{
  GameSettings* gs = gameGetSettings();
  if (!gs || !skillsPlayerIsReady(player))
    return NULL;

  RaidsPlayerClassProgress_t* playerClass = bankGetPlayerEquippedClass(player->PlayerId);
  struct SkillPowerupState* state = &powerupStates[player->PlayerId];
  int accountId = gs->PlayerAccountIds[player->PlayerId];
  if (state->Player != player || state->AccountId != accountId || state->ClassType != playerClass->ClassType) {
    int i;
    for (i = 0; i < SKILL_POWERUP_COUNT; ++i)
      powerupsStopEffect(state->Player == player ? player : NULL, &state->Effects[i], i);

    memset(state, 0, sizeof(*state));
    state->Player = player;
    state->AccountId = accountId;
    state->ClassType = playerClass->ClassType;
    if (player->IsLocal)
      memset(powerupWindows, 0, sizeof(powerupWindows));
  }

  return state;
}

//--------------------------------------------------------------------------
int powerupsCanCharge(Player* player, struct SkillPowerupState* state, int type)
{
  struct SkillPowerupEffect* effect = &state->Effects[type];
  return player->IsLocal && !playerIsDead(player) && player->Health > 0 && !player->InVehicle
    && powerupsDurationTicks(player, type) > 0 && !effect->Active
    && (!effect->HasCooldown || powerupsRemaining(effect->CooldownUntil, gameGetTime()) <= 0)
    && !powerupsNativeActive(player, type);
}

//--------------------------------------------------------------------------
void powerupsActivate(Player* player, struct SkillPowerupState* state, int type)
{
  struct SkillPowerupEffect* effect = &state->Effects[type];
  int now = gameGetTime();
  effect->DurationTicks = powerupsDurationTicks(player, type);
  effect->EndsAt = (u32)now + effect->DurationTicks * TIME_SECOND / TPS;
  effect->CooldownUntil = (u32)effect->EndsAt + (type == SKILL_POWERUP_QUAD
    ? POWERUP_FRENZY_COOLDOWN_MS : POWERUP_IRONCLAD_COOLDOWN_MS);
  effect->HasCooldown = 1;
  effect->Active = 1;
  memset(&powerupWindows[type], 0, sizeof(powerupWindows[type]));
  powerupsApplyEffect(player, effect, type);
  powerupsSend(state);
}

//--------------------------------------------------------------------------
float powerupsReferenceDamage(Player* player, Moby* target, int weaponId)
{
  if (!player->GadgetBox || weaponId <= 0)
    return 0;

  float damage = 0;
  if (weaponId == WEAPON_ID_WRENCH) {
    damage = bankGetGadgetDamage(player->GadgetBox, weaponId, 0, 1);
  } else {
    RaidsInventoryItem_t* item = bankGetEquippedWeaponFromGadgetBox(player->GadgetBox, weaponId);
    if (item)
      damage = bankGetWeaponDamage(item);
  }

  // Use the same Hero/class calculation as a hit, without crits, falloff, or Quad.
  struct RaidsPlayerDamageContext context = {
    .Attacker = player,
    .Target = target,
    .WeaponId = weaponId,
    .Damage = damage * (1 + PLAYER_LEVEL_DAMAGE_FACTOR * MapConfig.State->PlayerStates[player->PlayerId].State.Level)
  };
  skillsFinalizePlayerDamage(&context);
  return context.Damage > 0 && context.Damage <= FLT_MAX ? context.Damage : 0;
}

//--------------------------------------------------------------------------
void powerupsOnMobDamaged(Moby* moby, struct MobDamageEventArgs* args, float appliedDamage)
{
  if (!args || !mobyIsMob(moby) || mobyIsNpc(moby))
    return;

  Player* player = playerGetFromUID(args->SourceUID);
  int weaponId = getWeaponIdFromDamageSource(getDamageSourceFromOClass(args->SourceOClass));
  powerupsOnTargetDamaged(player, moby, weaponId, appliedDamage);
}

//--------------------------------------------------------------------------
void powerupsOnTargetDamaged(Player* player, Moby* target, int weaponId, float appliedDamage)
{
  if (!target || !(appliedDamage > 0 && appliedDamage <= FLT_MAX)
      || !skillsPlayerIsReady(player) || !player->IsLocal)
    return;

  struct SkillPowerupState* state = powerupsSyncPlayer(player);
  if (!state)
    return;

  if (!powerupsCanCharge(player, state, SKILL_POWERUP_QUAD)) {
    memset(&powerupWindows[SKILL_POWERUP_QUAD], 0, sizeof(powerupWindows[0]));
    return;
  }
  if (!(vector_sqrdistance(player->PlayerPosition, target->Position) <= POWERUP_CLOSE_RANGE_SQR))
    return;

  float reference = powerupsReferenceDamage(player, target, weaponId);
  if (!(reference > 0))
    return;

  float total = powerupsWindowAdd(&powerupWindows[SKILL_POWERUP_QUAD], gameGetTime(),
    POWERUP_FRENZY_WINDOW_MS, appliedDamage / reference);

  // Small fractional contributions must not miss an exact threshold through float rounding.
  if (total >= POWERUP_FRENZY_THRESHOLD * (1 - 0.000001f))
    powerupsActivate(player, state, SKILL_POWERUP_QUAD);
}

//--------------------------------------------------------------------------
int powerupsIsEnemyDamage(Moby* source)
{
  // Native projectiles (including thrown heads and trailshots) retain their creator.
  int i;
  for (i = 0; source && i < 8; ++i, source = source->PParent) {
    if (mobyIsNpc(source))
      return 0;

    if (mobyIsMob(source))
      return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
void powerupsOnPlayerDamaged(Player* player, float healthLost, float maxHealth)
{
  if (!skillsPlayerIsReady(player) || !player->IsLocal
      || !(healthLost > 0 && healthLost <= FLT_MAX && maxHealth > 0 && maxHealth <= FLT_MAX))
    return;

  struct SkillPowerupState* state = powerupsSyncPlayer(player);
  if (!state || !powerupsCanCharge(player, state, SKILL_POWERUP_SHIELD))
    return;

  float total = powerupsWindowAdd(&powerupWindows[SKILL_POWERUP_SHIELD], gameGetTime(),
    POWERUP_IRONCLAD_WINDOW_MS, healthLost / maxHealth);

  if (total >= POWERUP_IRONCLAD_THRESHOLD * (1 - 0.000001f))
    powerupsActivate(player, state, SKILL_POWERUP_SHIELD);
}

//--------------------------------------------------------------------------
void powerupsResetPlayer(Player* player)
{
  if (!player || player->PlayerId < 0 || player->PlayerId >= GAME_MAX_PLAYERS)
    return;

  struct SkillPowerupState* state = &powerupStates[player->PlayerId];
  if (state->Player != player)
    return;

  int changed = 0, i;
  for (i = 0; i < SKILL_POWERUP_COUNT; ++i) {
    changed |= state->Effects[i].Active;
    powerupsStopEffect(player, &state->Effects[i], i);
  }
  if (player->IsLocal) {
    memset(powerupWindows, 0, sizeof(powerupWindows));
    if (changed)
      powerupsSend(state);
  }

  // Death does not reset cooldowns. Class/account changes are handled by SyncPlayer.
}

//--------------------------------------------------------------------------
int powerupsOnState(void* connection, void* data)
{
  struct SkillPowerupMessage msg;
  memcpy(&msg, data, sizeof(msg));

  GameSettings* gs = gameGetSettings();
  if (!gs || msg.PlayerId < 0 || msg.PlayerId >= GAME_MAX_PLAYERS || msg.AccountId <= 0
      || msg.ClientId == gameGetMyClientId() || msg.MapLoadTime != gs->GameLoadStartTime
      || (s8)gs->PlayerClients[msg.PlayerId] != msg.ClientId
      || gs->PlayerAccountIds[msg.PlayerId] != msg.AccountId || (msg.ActiveMask & ~3))
    return sizeof(msg);

  Player* player = playerGetFromIndex(msg.PlayerId);
  if (!skillsPlayerIsReady(player) || player->IsLocal
      || bankGetPlayerEquippedClass(msg.PlayerId)->ClassType != msg.ClassType)
    return sizeof(msg);

  struct SkillPowerupState* state = powerupsSyncPlayer(player);
  if (!state || (state->HasSequence && (s32)(msg.Sequence - state->Sequence) <= 0))
    return sizeof(msg);

  int i, now = gameGetTime();
  for (i = 0; i < SKILL_POWERUP_COUNT; ++i) {
    if (!(msg.ActiveMask & (1 << i)))
      continue;

    int limit = powerupsDurationTicks(player, i);
    int remaining = powerupsRemaining(msg.EndsAt[i], now);
    if (msg.DurationTicks[i] <= 0 || msg.DurationTicks[i] > limit
        || remaining > msg.DurationTicks[i] * TIME_SECOND / TPS)
      return sizeof(msg);
  }

  state->HasSequence = 1;
  state->Sequence = msg.Sequence;

  for (i = 0; i < SKILL_POWERUP_COUNT; ++i) {
    struct SkillPowerupEffect* effect = &state->Effects[i];
    if (!(msg.ActiveMask & (1 << i)) || playerIsDead(player) || player->Health <= 0
        || powerupsRemaining(msg.EndsAt[i], now) <= 0) {
      powerupsStopEffect(player, effect, i);
      continue;
    }
    effect->Active = 1;
    effect->EndsAt = msg.EndsAt[i];
    effect->DurationTicks = msg.DurationTicks[i];
    powerupsApplyEffect(player, effect, i);
  }

  return sizeof(msg);
}

//--------------------------------------------------------------------------
void powerupsRegisterHud(Player* player, struct SkillPowerupState* state)
{
  int i, now = gameGetTime();
  for (i = 0; i < SKILL_POWERUP_COUNT; ++i) {
    if (powerupsDurationTicks(player, i) <= 0)
      continue;

    struct SkillPowerupEffect* effect = &state->Effects[i];
    int cooldownMs = i == SKILL_POWERUP_QUAD ? POWERUP_FRENZY_COOLDOWN_MS : POWERUP_IRONCLAD_COOLDOWN_MS;
    float cooldown = effect->HasCooldown
      ? powerupsRemaining(effect->CooldownUntil, now) / (float)cooldownMs : 0;
    float progress;
    if (effect->Active) {
      cooldown = 0;
      progress = powerupsEffectTicks(effect, now) / (float)effect->DurationTicks;
    } else {
      int windowMs = i == SKILL_POWERUP_QUAD ? POWERUP_FRENZY_WINDOW_MS : POWERUP_IRONCLAD_WINDOW_MS;
      float threshold = i == SKILL_POWERUP_QUAD ? POWERUP_FRENZY_THRESHOLD : POWERUP_IRONCLAD_THRESHOLD;
      progress = powerupsWindowPrune(&powerupWindows[i], now, windowMs) / threshold;
    }

    skillsRegisterPowerupCooldown(powerupsSkillId(i),
      i == SKILL_POWERUP_QUAD ? powerupsQuadIconTexId : powerupsShieldIconTexId, cooldown, progress, effect->Active);
  }
}

//--------------------------------------------------------------------------
void powerupsTick(void)
{
  int i, j, now = gameGetTime();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = playerGetFromIndex(i);
    struct SkillPowerupState* state = &powerupStates[i];
    if (!playerIsValid(player) || !playerIsConnected(player)) {
      memset(state, 0, sizeof(*state));
      if (player && player->IsLocal)
        memset(powerupWindows, 0, sizeof(powerupWindows));
      continue;
    }

    if (!skillsPlayerIsReady(player)) {
      powerupsResetPlayer(player);
      continue;
    }

    state = powerupsSyncPlayer(player);
    if (!state)
      continue;

    if (playerIsDead(player) || player->Health <= 0) {
      powerupsResetPlayer(player);
      continue;
    }

    int changed = 0, active = 0;
    for (j = 0; j < SKILL_POWERUP_COUNT; ++j) {
      struct SkillPowerupEffect* effect = &state->Effects[j];
      if (effect->Active && (powerupsRemaining(effect->EndsAt, now) <= 0 || powerupsDurationTicks(player, j) <= 0)) {
        powerupsStopEffect(player, effect, j);
        changed = 1;
      }

      active |= effect->Active;
      if (player->IsLocal && !powerupsCanCharge(player, state, j))
        memset(&powerupWindows[j], 0, sizeof(powerupWindows[j]));
    }

    if (player->IsLocal) {
      if (changed || (active && (u32)now - (u32)state->LastSendTime >= POWERUP_REFRESH_MS))
        powerupsSend(state);

      powerupsRegisterHud(player, state);
    }
  }
}

//--------------------------------------------------------------------------
void powerupsInit(void)
{
  memset(powerupStates, 0, sizeof(powerupStates));
  memset(powerupWindows, 0, sizeof(powerupWindows));
  powerupSequence = gameGetTime();
  netInstallCustomMsgHandler(CUSTOM_MSG_SKILL_POWERUP_STATE, &powerupsOnState);
}

//--------------------------------------------------------------------------
void powerupsCleanup(void)
{
  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = playerGetFromIndex(i);
    if (playerIsValid(player))
      powerupsResetPlayer(player);
  }

  netUninstallCustomMsgHandler(CUSTOM_MSG_SKILL_POWERUP_STATE, &powerupsOnState);
}
