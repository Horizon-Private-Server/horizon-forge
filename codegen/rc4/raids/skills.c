#include <float.h>
#include <libdl/stdio.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/game.h>
#include <libdl/moby.h>
#include <libdl/pad.h>
#include <libdl/string.h>
#include <libdl/utils.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/weapon.h>
#include <libdl/vehicle.h>
#include <libdl/collision.h>
#include <libdl/graphics.h>
#include <libdl/hud.h>
#include <libdl/color.h>

#include "skills.h"
#include "game.h"
#include "bank.h"
#include "utils.h"
#include "common.h"
#include "collectible.h"
#include "skillfield.h"
#include "menu.h"

#define SKILLS_FIELD_RADIUS (5.0)
#define SKILLS_FIELD_DURATION_MS (10 * TIME_SECOND)
#define SKILLS_FIELD_COOLDOWN_MS (30 * TIME_SECOND)
#define SKILLS_GOLD_BOLT_CACHE_TICKS (6)
#define SKILLS_COOLDOWN_HUD_ICON_SIZE (24)
#define SKILLS_COOLDOWN_HUD_SPACING (4)
#define SKILLS_COOLDOWN_HUD_BOTTOM (5)

int skillsPlayerHasMovementInput(Player* player);

// Extra jumps and hoverboots have per-player, per-class transient state.
struct SkillsMovementState
{
  Player* Player;
  int ClassType;
  int ExtraJumps;
  int ExtraJumpLimit;
  int HoverTicks;
  int HoverDuration;
} skillsMovementStates[GAME_MAX_PLAYERS];

// Damage Immunity tracks the extended timer, never a particular countdown value.
struct SkillsImmunityState
{
  Player* Player;
  int ClassType;
  int LastTimer;
} skillsImmunityStates[GAME_MAX_PLAYERS];

// Fractional field ammo and pickup bonuses are transient, never saved equipment.
struct SkillsAmmoState
{
  Player* Player;
  GadgetBox* GadgetBox;
  int ClassType;
  float SuppliesFactor;
  float Remainder[WEAPON_SLOT_COUNT];
} skillsAmmoStates[GAME_MAX_PLAYERS];

// Vehicle tuning is map-owned and shared by both vehicle skills.
struct SkillsVehicleTuning
{
  float HealthMultiplier;
  float DriverDamage;
  float PassengerDamage;
} skillsVehicleTuning[] = {
  { 2, 50, 100 }, // puma
  { 3, 75, 120 }, // hovership
  { 0, 0, 0 },
  { 0, 0, 0 },
  { 0, 0, 0 },
  { 3, 50, 120 }, // landstalker
  { 1, 50, 0 }, // hoverbike
};

struct SkillsVehicleState
{
  Vehicle* Vehicle;
  Moby* Moby;
  int UID;
  float BaseMaxHealth;
} skillsVehicleStates[GAME_MAX_PLAYERS * 2];

// Cooldowns belong to the caster; active fields own their state in their PVars.
struct SkillsFieldCasterState
{
  int CooldownMask;
  int HealingActivatedAt;
  int AmmoActivatedAt;
} skillsFieldCasters[GAME_MAX_PLAYERS];

// One entry per skill keeps ordering stable and repeated registration replaces it.
struct SkillsCooldownHudEntry
{
  int Registered;
  int IconTexId;
  u32 Color;
  float Cooldown;
  int ShowCooldownBar;
} skillsCooldownHud[RAIDS_CLASS_SKILL_COUNT];

// Cache both proximity results; collection changes invalidate immediately.
struct SkillsGoldBoltState
{
  u32 CollectiblesMask;
  int CacheTicks;
  int HasNearby;
} skillsGoldBoltState;

//--------------------------------------------------------------------------
float skillsGetDamageFactor(int skillPoints)
{
  if (skillPoints <= 0) return 1.0f;
  return 1.0f + 0.10f * skillPoints;
}

//--------------------------------------------------------------------------
float skillsGetMovementSpeedFactor(int skillPoints)
{
  if (skillPoints <= 0) return 1.0f;
  return 1.0f + 0.05f * skillPoints;
}

//--------------------------------------------------------------------------
float skillsGetFactor(enum RaidsClassSkillId skillId, int skillPoints, float multiplier)
{
  if (skillPoints <= 0 || !(multiplier > 0 && multiplier <= FLT_MAX)) return 1.0f;
  switch (skillId)
  {
    case RAIDS_CLASS_SKILL_DAMAGE: return 1.0f + (skillsGetDamageFactor(skillPoints) - 1.0f) * multiplier;
    case RAIDS_CLASS_SKILL_MOVEMENT_SPEED: return 1.0f + (skillsGetMovementSpeedFactor(skillPoints) - 1.0f) * multiplier;
    case RAIDS_CLASS_SKILL_CLOSE_RANGE_DAMAGE:
    case RAIDS_CLASS_SKILL_LONG_RANGE_DAMAGE:
    case RAIDS_CLASS_SKILL_VEHICLE_DAMAGE:
    case RAIDS_CLASS_SKILL_VEHICLE_HEALTH:
    case RAIDS_CLASS_SKILL_MAX_HEALTH:
    case RAIDS_CLASS_SKILL_VIPER_PROJECTILE_SPEED:
    case RAIDS_CLASS_SKILL_B6_PROJECTILE_SPEED:
    case RAIDS_CLASS_SKILL_AIR_SUPERIORITY:
    case RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES:
      return 1.0f + 0.10f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_CHARGEBOOT_LENGTH:
      return 1.0f + 0.15f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_WRENCH_DAMAGE:
      return 1.0f + 0.20f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_BLAST_REACH:
      return maxf(0.0f, 1.0f - 0.10f * skillPoints * multiplier);
    case RAIDS_CLASS_SKILL_STEALTH:
      // Ordinary vision and proximity detection only. Existing targets, retaliation,
      // and map-authored forced aggro remain effective.
      return maxf(0.5f, 1.0f - 0.10f * skillPoints * multiplier);
    default: return 1.0f;
  }
}

//--------------------------------------------------------------------------
float skillsGetBonus(enum RaidsClassSkillId skillId, int skillPoints, float multiplier)
{
  if (skillPoints <= 0 || !(multiplier > 0 && multiplier <= FLT_MAX)) return 0.0f;
  switch (skillId)
  {
    case RAIDS_CLASS_SKILL_EXTRA_JUMPS:
    case RAIDS_CLASS_SKILL_ALPHA_AREA:
    case RAIDS_CLASS_SKILL_ALPHA_SPEED:
    case RAIDS_CLASS_SKILL_ALPHA_NANOLEECH:
    case RAIDS_CLASS_SKILL_ALPHA_AMMO:
      return floorf(skillPoints * multiplier);
    case RAIDS_CLASS_SKILL_HEALING_FIELD: return 10.0f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_AMMO_FIELD:
      // Normal ammo-pickup equivalents per pulse, using ammodrop's per-weapon tuning.
      return 0.25f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE: return 5.0f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_CRITICAL_CHANCE: return 5.0f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_DEADEYE:
      // Percentage points while stationary with Fusion; total critical chance caps at 100%.
      return 20.0f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_DAMAGE_IMMUNITY: return floorf(15.0f * skillPoints * multiplier);
    case RAIDS_CLASS_SKILL_HOVERBOOTS: return 1.5f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_WALL_WALKING: return 2.0f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_SECOND_WIND: return 5.0f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_COMBAT_MEDIC: return 50.0f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_CLUSTER_BOMBS:
      // Primary B6 explosions only; secondary bombs must not produce more clusters.
      return minf(2.0f, floorf(skillPoints * multiplier));
    case RAIDS_CLASS_SKILL_FLINCH_IMMUNITY:
    case RAIDS_CLASS_SKILL_VISIBOMB:
    case RAIDS_CLASS_SKILL_GOLD_BOLT_INDICATOR:
      return 1.0f;
    default: return 0.0f;
  }
}

//--------------------------------------------------------------------------
int skillsIsValid(enum RaidsClassSkillId skillId)
{
  return skillId >= RAIDS_CLASS_SKILL_DAMAGE && skillId < RAIDS_CLASS_SKILL_COUNT;
}

//--------------------------------------------------------------------------
int skillsIsUnlock(enum RaidsClassSkillId skillId)
{
  return skillId == RAIDS_CLASS_SKILL_FLINCH_IMMUNITY || skillId == RAIDS_CLASS_SKILL_VISIBOMB
    || skillId == RAIDS_CLASS_SKILL_GOLD_BOLT_INDICATOR;
}

//--------------------------------------------------------------------------
void skillsFormatRank(enum RaidsClassSkillId skillId, int skillPoints, float multiplier, char* buf, int bufSize)
{
  if (!buf || bufSize <= 0) return;
  if (skillPoints < 0)
    skillPoints = 0;

  float bonus = skillsGetBonus(skillId, skillPoints, multiplier);

  switch (skillId)
  {
    case RAIDS_CLASS_SKILL_EXTRA_JUMPS:
      snprintf(buf, bufSize, "+%d jump%s", (int)bonus, bonus == 1 ? "" : "s");
      break;
    case RAIDS_CLASS_SKILL_ALPHA_AREA:
    case RAIDS_CLASS_SKILL_ALPHA_SPEED:
    case RAIDS_CLASS_SKILL_ALPHA_NANOLEECH:
    case RAIDS_CLASS_SKILL_ALPHA_AMMO:
      snprintf(buf, bufSize, "+%d mod%s", (int)bonus, bonus == 1 ? "" : "s");
      break;
    case RAIDS_CLASS_SKILL_HEALING_FIELD:
      snprintf(buf, bufSize, "+%.0f HP/pulse", bonus);
      break;
    case RAIDS_CLASS_SKILL_AMMO_FIELD:
      snprintf(buf, bufSize, "%.2fx drop/pulse", bonus);
      break;
    case RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE:
      snprintf(buf, bufSize, "+%.0f%% drop chance", bonus);
      break;
    case RAIDS_CLASS_SKILL_CRITICAL_CHANCE:
      snprintf(buf, bufSize, "+%.0f%% crit", bonus);
      break;
    case RAIDS_CLASS_SKILL_DAMAGE_IMMUNITY:
      snprintf(buf, bufSize, "+%.1f sec", bonus / TPS);
      break;
    case RAIDS_CLASS_SKILL_HOVERBOOTS:
    case RAIDS_CLASS_SKILL_WALL_WALKING:
      snprintf(buf, bufSize, "+%.1f sec", bonus);
      break;
    case RAIDS_CLASS_SKILL_SECOND_WIND:
      snprintf(buf, bufSize, "+%.0f HP/kill", bonus);
      break;
    case RAIDS_CLASS_SKILL_DEADEYE:
      snprintf(buf, bufSize, "+%.0f%% crit chance", bonus);
      break;
    case RAIDS_CLASS_SKILL_COMBAT_MEDIC:
      snprintf(buf, bufSize, "+%.0f HP/hit", bonus);
      break;
    case RAIDS_CLASS_SKILL_CLUSTER_BOMBS:
      snprintf(buf, bufSize, "+%d B6 bomb%s", (int)bonus, bonus == 1 ? "" : "s");
      break;
    case RAIDS_CLASS_SKILL_FLINCH_IMMUNITY:
    case RAIDS_CLASS_SKILL_VISIBOMB:
    case RAIDS_CLASS_SKILL_GOLD_BOLT_INDICATOR:
      snprintf(buf, bufSize, "%s", skillPoints > 0 ? "Unlocked" : "Not learned");
      break;
    case RAIDS_CLASS_SKILL_BLAST_REACH:
      snprintf(buf, bufSize, "%.0f%% less falloff", (1 - skillsGetFactor(skillId, skillPoints, multiplier)) * 100);
      break;
    case RAIDS_CLASS_SKILL_STEALTH:
      snprintf(buf, bufSize, "%.0f%% less detection", (1 - skillsGetFactor(skillId, skillPoints, multiplier)) * 100);
      break;
    default:
      if (skillsIsValid(skillId))
        snprintf(buf, bufSize, "+%.0f%%", (skillsGetFactor(skillId, skillPoints, multiplier) - 1) * 100);
      else
        snprintf(buf, bufSize, "--");
      break;
  }
}

//--------------------------------------------------------------------------
char* skillsGetName(enum RaidsClassSkillId skillId)
{
  switch (skillId)
  {
    case RAIDS_CLASS_SKILL_DAMAGE: return "Damage";
    case RAIDS_CLASS_SKILL_MOVEMENT_SPEED: return "Movement Speed";
    case RAIDS_CLASS_SKILL_CLOSE_RANGE_DAMAGE: return "Close-range Damage";
    case RAIDS_CLASS_SKILL_LONG_RANGE_DAMAGE: return "Long-range Damage";
    case RAIDS_CLASS_SKILL_EXTRA_JUMPS: return "Extra Jumps";
    case RAIDS_CLASS_SKILL_CHARGEBOOT_LENGTH: return "Chargeboot Length";
    case RAIDS_CLASS_SKILL_WRENCH_DAMAGE: return "Wrench Damage";
    case RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE: return "Ammo Drop Chance";
    case RAIDS_CLASS_SKILL_HEALING_FIELD: return "Healing Field";
    case RAIDS_CLASS_SKILL_AMMO_FIELD: return "Ammo Field";
    case RAIDS_CLASS_SKILL_CRITICAL_CHANCE: return "Critical Chance";
    case RAIDS_CLASS_SKILL_DAMAGE_IMMUNITY: return "Damage Immunity";
    case RAIDS_CLASS_SKILL_ALPHA_AREA: return "Area Mods";
    case RAIDS_CLASS_SKILL_ALPHA_SPEED: return "Speed Mods";
    case RAIDS_CLASS_SKILL_ALPHA_NANOLEECH: return "Nanoleech Mods";
    case RAIDS_CLASS_SKILL_FLINCH_IMMUNITY: return "Flinch Immunity";
    case RAIDS_CLASS_SKILL_HOVERBOOTS: return "Hoverboots";
    case RAIDS_CLASS_SKILL_VISIBOMB: return "Visibomb";
    case RAIDS_CLASS_SKILL_WALL_WALKING: return "Wall Walking";
    case RAIDS_CLASS_SKILL_GOLD_BOLT_INDICATOR: return "Gold Bolt Indicator";
    case RAIDS_CLASS_SKILL_VEHICLE_DAMAGE: return "Vehicle Damage";
    case RAIDS_CLASS_SKILL_VEHICLE_HEALTH: return "Vehicle Health";
    case RAIDS_CLASS_SKILL_MAX_HEALTH: return "Maximum Health";
    case RAIDS_CLASS_SKILL_ALPHA_AMMO: return "Ammo Mods";
    case RAIDS_CLASS_SKILL_VIPER_PROJECTILE_SPEED: return "Viper Projectile Speed";
    case RAIDS_CLASS_SKILL_B6_PROJECTILE_SPEED: return "B6 Projectile Speed";
    case RAIDS_CLASS_SKILL_BLAST_REACH: return "Blast Reach";
    case RAIDS_CLASS_SKILL_SECOND_WIND: return "Second Wind";
    case RAIDS_CLASS_SKILL_AIR_SUPERIORITY: return "Air Superiority";
    case RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES: return "Efficient Supplies";
    case RAIDS_CLASS_SKILL_STEALTH: return "Stealth";
    case RAIDS_CLASS_SKILL_DEADEYE: return "Deadeye";
    case RAIDS_CLASS_SKILL_COMBAT_MEDIC: return "Combat Medic";
    case RAIDS_CLASS_SKILL_CLUSTER_BOMBS: return "Cluster Bombs";
    default: return "Unknown skill";
  }
}

//--------------------------------------------------------------------------
char* skillsGetDescription(enum RaidsClassSkillId skillId)
{
  switch (skillId)
  {
    case RAIDS_CLASS_SKILL_DAMAGE: return "Increase damage.";
    case RAIDS_CLASS_SKILL_MOVEMENT_SPEED: return "Increase movement speed.";
    case RAIDS_CLASS_SKILL_CLOSE_RANGE_DAMAGE: return "Increase damage against nearby mobs.";
    case RAIDS_CLASS_SKILL_LONG_RANGE_DAMAGE: return "Increase damage against distant mobs.";
    case RAIDS_CLASS_SKILL_EXTRA_JUMPS: return "Add extra jumps before landing.";
    case RAIDS_CLASS_SKILL_CHARGEBOOT_LENGTH: return "Extend chargeboot travel duration.";
    case RAIDS_CLASS_SKILL_WRENCH_DAMAGE: return "Increase wrench damage.";
    case RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE: return "Add percentage points to the chance of an ammo drop.";
    case RAIDS_CLASS_SKILL_HEALING_FIELD: return "Wrench hyperstrike creates a healing field. Has a cooldown.";
    case RAIDS_CLASS_SKILL_AMMO_FIELD: return "Wrench hyperstrike creates an ammo field. Uses ammo drops. Has cooldown.";
    case RAIDS_CLASS_SKILL_CRITICAL_CHANCE: return "Add percentage points to critical hit chance.";
    case RAIDS_CLASS_SKILL_DAMAGE_IMMUNITY: return "Extend the damage immunity after getting hit.";
    case RAIDS_CLASS_SKILL_ALPHA_AREA: return "Add Area alpha mods to your equipped weapon.";
    case RAIDS_CLASS_SKILL_ALPHA_SPEED: return "Add Speed alpha mods to your equipped weapon.";
    case RAIDS_CLASS_SKILL_ALPHA_NANOLEECH: return "Add Nanoleech alpha mods to your equipped weapon.";
    case RAIDS_CLASS_SKILL_FLINCH_IMMUNITY: return "Prevent hit flinching. Damage still applies.";
    case RAIDS_CLASS_SKILL_HOVERBOOTS: return "Walk off ledges and hover briefly before falling.";
    case RAIDS_CLASS_SKILL_VISIBOMB: return "Toggle Arbiter rocket control or fire normally.";
    case RAIDS_CLASS_SKILL_WALL_WALKING: return "Walk on walls for a limited time.";
    case RAIDS_CLASS_SKILL_GOLD_BOLT_INDICATOR: return "Show an indicator near uncollected gold bolts.";
    case RAIDS_CLASS_SKILL_VEHICLE_DAMAGE: return "Increase damage from vehicles you operate.";
    case RAIDS_CLASS_SKILL_VEHICLE_HEALTH: return "Increase vehicle maximum health while driving.";
    case RAIDS_CLASS_SKILL_MAX_HEALTH: return "Increase hero maximum health.";
    case RAIDS_CLASS_SKILL_ALPHA_AMMO: return "Add Ammo alpha mods to your equipped weapon.";
    case RAIDS_CLASS_SKILL_VIPER_PROJECTILE_SPEED: return "Increase Dual Vipers projectile speed.";
    case RAIDS_CLASS_SKILL_B6_PROJECTILE_SPEED: return "Increase B6 projectile speed.";
    case RAIDS_CLASS_SKILL_BLAST_REACH: return "Retain more damage at the edges of explosions.";
    case RAIDS_CLASS_SKILL_SECOND_WIND: return "Kills heal you below 25% maximum health.";
    case RAIDS_CLASS_SKILL_AIR_SUPERIORITY: return "Increase damage while airborne.";
    case RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES: return "Health and ammo pickups restore more.";
    case RAIDS_CLASS_SKILL_STEALTH: return "Reduce detection ranges. Mobs can still retaliate.";
    case RAIDS_CLASS_SKILL_DEADEYE: return "Gain critical chance while standing still with Fusion. Total caps at 100%.";
    case RAIDS_CLASS_SKILL_COMBAT_MEDIC: return "Direct Fusion hits heal living teammates up to max HP. Uses normal ammo.";
    case RAIDS_CLASS_SKILL_CLUSTER_BOMBS: return "Primary B6 blasts spawn extra B6 bombs. Secondary bombs cannot cluster.";
    default: return "No description available.";
  }
}

//--------------------------------------------------------------------------
// Runtime skills use confirmed, effective bank ranks (including mastery).
int skillsPlayerIsReady(Player* player)
{
  if (!MapConfig.State || !playerIsValid(player) || !playerIsConnected(player)) return 0;
  if (player->PlayerId < 0 || player->PlayerId >= GAME_MAX_PLAYERS) return 0;

  if (player->IsLocal) {
    if (!bankGetHasAccount() || bankHasPendingAccountRequest()
        || !bankGetHasEquippedClassProgress() || bankHasPendingEquippedClassProgressRequest())
      return 0;

    RaidsPlayerBank_t* bank = bankGetLocalBank();
    if (!bank || bank->Account.EquippedClass != bank->EquippedClassProgress.ClassType) return 0;
  }
  return bankGetPlayerEquippedClass(player->PlayerId) != NULL;
}

//--------------------------------------------------------------------------
float skillsGetPlayerFactor(Player* player, enum RaidsClassSkillId skillId)
{
  if (!skillsPlayerIsReady(player)) return 1.0f;

  return skillsGetFactor(skillId, bankGetPlayerClassSkillRank(player->PlayerId, skillId),
    bankGetPlayerClassSkillMultiplier(player->PlayerId, skillId));
}

//--------------------------------------------------------------------------
float skillsGetPlayerBonus(Player* player, enum RaidsClassSkillId skillId)
{
  if (!skillsPlayerIsReady(player)) return 0;

  return skillsGetBonus(skillId, bankGetPlayerClassSkillRank(player->PlayerId, skillId),
    bankGetPlayerClassSkillMultiplier(player->PlayerId, skillId));
}

//--------------------------------------------------------------------------
// Ammo-drop bonuses are percentage points.
float skillsGetAmmoDropChance(Player* player, float baseChance)
{
  return baseChance + skillsGetPlayerBonus(player, RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE) / 100.0f;
}

//--------------------------------------------------------------------------
// Damage, movement speed and maximum health are applied to fresh baselines.
void skillsFinalizePlayerDamage(struct RaidsPlayerDamageContext* context)
{
  if (!context) return;

  Player* player = context->Attacker;
  context->Damage *= skillsGetPlayerFactor(player, RAIDS_CLASS_SKILL_DAMAGE);
  if (!skillsPlayerIsReady(player)) return;

  if (context->Target) {
    float distance = sqrtf(vector_sqrdistance(player->PlayerPosition, context->Target->Position));
    if (distance >= 0 && distance <= FLT_MAX) {
      float closeWeight = clamp(1 - distance / 15.0f, 0, 1);
      float longWeight = clamp((distance - 25.0f) / 25.0f, 0, 1);
      context->Damage *= 1 + (skillsGetPlayerFactor(player, RAIDS_CLASS_SKILL_CLOSE_RANGE_DAMAGE) - 1) * closeWeight;
      context->Damage *= 1 + (skillsGetPlayerFactor(player, RAIDS_CLASS_SKILL_LONG_RANGE_DAMAGE) - 1) * longWeight;
    }
  }
  if (context->WeaponId == WEAPON_ID_WRENCH)
    context->Damage *= skillsGetPlayerFactor(player, RAIDS_CLASS_SKILL_WRENCH_DAMAGE);
  if (!player->InVehicle && !player->Ground.onGood)
    context->Damage *= skillsGetPlayerFactor(player, RAIDS_CLASS_SKILL_AIR_SUPERIORITY);
}

//--------------------------------------------------------------------------
void skillsModifyPlayerStats(Player* player, struct RaidsPlayerStats* stats)
{
  if (!stats) return;

  stats->Speed *= skillsGetPlayerFactor(player, RAIDS_CLASS_SKILL_MOVEMENT_SPEED);
  stats->MaxHealth *= skillsGetPlayerFactor(player, RAIDS_CLASS_SKILL_MAX_HEALTH);
}

//--------------------------------------------------------------------------
// Critical chance bonuses are percentage points, not multipliers.
void skillsModifyPlayerCritChance(struct RaidsPlayerDamageContext* context)
{
  if (!context) return;

  context->CritChance += skillsGetPlayerBonus(context->Attacker, RAIDS_CLASS_SKILL_CRITICAL_CHANCE) / 100.0f;
  Player* player = context->Attacker;
  if (skillsPlayerIsReady(player) && !player->InVehicle && player->Ground.onGood
      && player->WeaponHeldId == WEAPON_ID_FUSION_RIFLE && context->WeaponId == WEAPON_ID_FUSION_RIFLE
      && player->Velocity[0] * player->Velocity[0] + player->Velocity[1] * player->Velocity[1] < 0.0001f)
    context->CritChance += skillsGetPlayerBonus(player, RAIDS_CLASS_SKILL_DEADEYE) / 100.0f;
}

//--------------------------------------------------------------------------
void skillsModifyPlayerCritDamage(struct RaidsPlayerDamageContext* context)
{
  // No current skill changes the critical multiplier. Keep this extension point.
}

//--------------------------------------------------------------------------
int skillsGetAlphaModBonus(Player* player, int alphaModId)
{
  enum RaidsClassSkillId skillId;

  switch (alphaModId)
  {
    case ALPHA_MOD_AMMO:
      skillId = RAIDS_CLASS_SKILL_ALPHA_AMMO;
      break;
    case ALPHA_MOD_AREA:
      skillId = RAIDS_CLASS_SKILL_ALPHA_AREA;
      break;
    case ALPHA_MOD_SPEED:
      skillId = RAIDS_CLASS_SKILL_ALPHA_SPEED;
      break;
    case ALPHA_MOD_NANOLEECH:
      skillId = RAIDS_CLASS_SKILL_ALPHA_NANOLEECH;
      break;
    default: return 0;
  }
  return (int)skillsGetPlayerBonus(player, skillId);
}

//--------------------------------------------------------------------------
void skillsOnMobKilled(Moby* moby, int killedByPlayerId)
{
  if (killedByPlayerId < 0 || killedByPlayerId >= GAME_MAX_PLAYERS) return;

  Player* player = playerGetAll()[killedByPlayerId];
  if (!playerIsValid(player) || !player->IsLocal || playerIsDead(player)
      || player->Health <= 0 || player->MaxHealth <= 0 || player->Health >= player->MaxHealth * 0.25f)
    return;

  float heal = skillsGetPlayerBonus(player, RAIDS_CLASS_SKILL_SECOND_WIND);
  if (heal > 0)
    playerSetHealth(player, minf(player->MaxHealth, player->Health + heal));
}

//--------------------------------------------------------------------------
float skillsGetDetectionFactor(Player* player)
{
  return skillsGetPlayerFactor(player, RAIDS_CLASS_SKILL_STEALTH);
}

//--------------------------------------------------------------------------
void skillsDamageImmunityUpdate(Player* player)
{
  if (!player->IsLocal) return;

  struct SkillsImmunityState* state = &skillsImmunityStates[player->PlayerId];
  int classType = bankGetPlayerEquippedClass(player->PlayerId)->ClassType;
  int timer = player->timers.postHitInvinc;
  if (state->Player != player || state->ClassType != classType) {
    state->Player = player;
    state->ClassType = classType;
  } else if (timer > 0 && (state->LastTimer <= 0 || timer > state->LastTimer)) {
    float bonus = skillsGetPlayerBonus(player, RAIDS_CLASS_SKILL_DAMAGE_IMMUNITY);
    timer += (int)minf(32767 - timer, bonus);
    player->timers.postHitInvinc = timer;
  }
  state->LastTimer = timer;
}

//--------------------------------------------------------------------------
struct SkillsAmmoState* skillsSyncAmmoState(Player* player)
{
  struct SkillsAmmoState* state = &skillsAmmoStates[player->PlayerId];
  RaidsPlayerClassProgress_t* playerClass = bankGetPlayerEquippedClass(player->PlayerId);
  int classType = playerClass ? playerClass->ClassType : -1;
  float suppliesFactor = skillsGetPlayerFactor(player, RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES);
  if (state->Player != player || state->GadgetBox != player->GadgetBox
      || state->ClassType != classType || state->SuppliesFactor != suppliesFactor) {
    memset(state, 0, sizeof(*state));
    state->Player = player;
    state->GadgetBox = player->GadgetBox;
    state->ClassType = classType;
    state->SuppliesFactor = suppliesFactor;
  }
  return state;
}

//--------------------------------------------------------------------------
void skillsGiveAmmo(Player* player, int weaponId, float amount)
{
  if (!playerIsValid(player) || !player->IsLocal || playerIsDead(player) || player->Health <= 0
      || !player->GadgetBox || player->PlayerId < 0 || player->PlayerId >= GAME_MAX_PLAYERS
      || !(amount > 0 && amount <= FLT_MAX))
    return;

  int slot = weaponIdToSlot(weaponId);
  if (slot <= 0 || slot >= WEAPON_SLOT_COUNT || player->GadgetBox->Gadgets[weaponId].Level < 0) return;

  struct SkillsAmmoState* state = skillsSyncAmmoState(player);

  int maxAmmo = playerGetWeaponMaxAmmo(player->GadgetBox, weaponId);
  int ammo = player->GadgetBox->Gadgets[weaponId].Ammo;
  if (maxAmmo <= 0 || ammo >= maxAmmo) {
    state->Remainder[slot] = 0;
    return;
  }

  amount = minf(maxAmmo - ammo, amount + state->Remainder[slot]);
  int whole = (int)amount;
  player->GadgetBox->Gadgets[weaponId].Ammo = ammo + whole;
  state->Remainder[slot] = ammo + whole >= maxAmmo ? 0 : amount - whole;
}

//--------------------------------------------------------------------------
void skillsOnAmmoPickup(Player* player, int weaponId, int amount)
{
  float factor = skillsGetPlayerFactor(player, RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES);
  if (amount > 0 && factor > 1)
    skillsGiveAmmo(player, weaponId, amount * (factor - 1));
}

//--------------------------------------------------------------------------
void skillsOnHealthboxHeal(Player* player, float amount)
{
  if (!playerIsValid(player) || !player->IsLocal || playerIsDead(player) || player->Health <= 0) return;

  amount = minf(amount, 100);
  if (!(amount > 0 && amount <= FLT_MAX)) return;
  amount *= skillsGetPlayerFactor(player, RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES);
  playerSetHealth(player, minf(player->MaxHealth, player->Health + amount));
}

//--------------------------------------------------------------------------
int skillsVehicleIsSupported(Vehicle* vehicle)
{
  return vehicle && vehicle->vehicleType >= 0 && vehicle->vehicleType < COUNT_OF(skillsVehicleTuning)
    && skillsVehicleTuning[vehicle->vehicleType].HealthMultiplier > 0;
}

//--------------------------------------------------------------------------
int skillsVehicleStateIsValid(struct SkillsVehicleState* state)
{
  return state->Vehicle && state->Moby && !mobyIsDestroyed(state->Moby) && state->Moby->UID == state->UID
    && state->Vehicle->pMoby == state->Moby && skillsVehicleIsSupported(state->Vehicle);
}

//--------------------------------------------------------------------------
struct SkillsVehicleState* skillsTrackVehicle(Vehicle* vehicle)
{
  if (!skillsVehicleIsSupported(vehicle) || !vehicle->pMoby) return NULL;

  int i;
  struct SkillsVehicleState* freeState = NULL;
  for (i = 0; i < COUNT_OF(skillsVehicleStates); ++i) {
    struct SkillsVehicleState* state = &skillsVehicleStates[i];
    if (skillsVehicleStateIsValid(state)) {
      if (state->Vehicle == vehicle) return state;
    } else if (!freeState) {
      freeState = state;
    }
  }
  if (!freeState) return NULL;

  freeState->Vehicle = vehicle;
  freeState->Moby = vehicle->pMoby;
  freeState->UID = vehicle->pMoby->UID;
  freeState->BaseMaxHealth = vehicle->maxHP;
  return freeState;
}

//--------------------------------------------------------------------------
Player* skillsGetVehicleOccupant(Vehicle* vehicle, Player* player)
{
  return playerIsValid(player) && playerIsConnected(player) && !playerIsDead(player)
    && player->Health > 0 && player->InVehicle && player->Vehicle == vehicle ? player : NULL;
}

//--------------------------------------------------------------------------
void skillsUpdateVehicle(struct SkillsVehicleState* state)
{
  if (!skillsVehicleStateIsValid(state)) {
    memset(state, 0, sizeof(*state));
    return;
  }

  Vehicle* vehicle = state->Vehicle;
  struct SkillsVehicleTuning* tuning = &skillsVehicleTuning[vehicle->vehicleType];
  Player* driver = skillsGetVehicleOccupant(vehicle, vehicle->pDriver);
  Player* passenger = skillsGetVehicleOccupant(vehicle, vehicle->pPassenger);
  float maxHealth = state->BaseMaxHealth;
  if (driver) {
    vector_write(driver->Velocity, 0); // make sure predictive movements aren't used
    state->BaseMaxHealth = driver->MaxHealth * tuning->HealthMultiplier;
    maxHealth = state->BaseMaxHealth * skillsGetPlayerFactor(driver, RAIDS_CLASS_SKILL_VEHICLE_HEALTH);
  }

  // set vehicle stats
  if (maxHealth > 0 && maxHealth <= FLT_MAX) {
    vehicle->maxHP = maxHealth;
    if (vehicle->hitPoints > maxHealth)
      vehicle->hitPoints = maxHealth;
  }
  vehicle->fDriverAttackDamage = tuning->DriverDamage * skillsGetPlayerFactor(driver, RAIDS_CLASS_SKILL_VEHICLE_DAMAGE);
  vehicle->fPassengerAttackDamage = tuning->PassengerDamage * skillsGetPlayerFactor(passenger, RAIDS_CLASS_SKILL_VEHICLE_DAMAGE);
}

//--------------------------------------------------------------------------
void skillsOnVehicleInitialized(Vehicle* vehicle)
{
  if (!skillsVehicleIsSupported(vehicle)) return;

  vehicle->maxHP = vehicle->hitPoints = 10000;
  vehicle->fDriverAttackDamage = skillsVehicleTuning[vehicle->vehicleType].DriverDamage;
  vehicle->fPassengerAttackDamage = skillsVehicleTuning[vehicle->vehicleType].PassengerDamage;
  struct SkillsVehicleState* state = skillsTrackVehicle(vehicle);
  if (state)
    state->BaseMaxHealth = 10000;
}

//--------------------------------------------------------------------------
void skillsVehiclesTick(void)
{
  int i;
  Player** players = playerGetAll();

  if (!MapConfig.State) return;

  // track when a player is in a vehicle
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (playerIsValid(player) && player->InVehicle)
      skillsTrackVehicle(player->Vehicle);
  }

  // update each tracked vehicle
  for (i = 0; i < COUNT_OF(skillsVehicleStates); ++i)
    skillsUpdateVehicle(&skillsVehicleStates[i]);

  // Empty vehicles have already had their bonuses removed and can be evicted.
  for (i = 0; i < COUNT_OF(skillsVehicleStates); ++i) {
    struct SkillsVehicleState* state = &skillsVehicleStates[i];
    if (state->Vehicle && !skillsGetVehicleOccupant(state->Vehicle, state->Vehicle->pDriver)
        && !skillsGetVehicleOccupant(state->Vehicle, state->Vehicle->pPassenger))
      memset(state, 0, sizeof(*state));
  }
}

//--------------------------------------------------------------------------
void skillsFieldsActivate(Player* player)
{
  if (!skillsPlayerHasMovementInput(player) || player->PlayerState != PLAYER_STATE_JUMP_ATTACK
      || player->WeaponHeldId != WEAPON_ID_WRENCH || player->Ground.onGood != 1)
    return;

  struct SkillsFieldCasterState* caster = &skillsFieldCasters[player->PlayerId];
  struct SkillFieldSpawnArgs args = {
    .Radius = SKILLS_FIELD_RADIUS,
    .StartTime = gameGetTime(),
    .DurationMs = SKILLS_FIELD_DURATION_MS,
    .PulseIntervalMs = TIME_SECOND,
    .CasterPlayerId = player->PlayerId,
    .CasterClassType = bankGetPlayerEquippedClass(player->PlayerId)->ClassType,
    .Team = player->Team
  };

  VECTOR position;
  VECTOR from = {0,0,1,0};
  VECTOR to = {0,0,-10,0};
  vector_copy(position, player->PlayerPosition);
  vector_add(from, position, from);
  vector_add(to, position, to);
  if (CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_MOBY_SPECIAL_COLLIDERS, player->PlayerMoby, NULL))
    vector_copy(position, CollLine_Fix_GetHitPosition());
  memcpy(args.Position, position, sizeof(args.Position));

  // check for healing
  if (!(caster->CooldownMask & SKILLFIELD_TYPE_HEALING)
      || (int)((u32)args.StartTime - (u32)caster->HealingActivatedAt) >= SKILLS_FIELD_COOLDOWN_MS) {
    args.HealingAmount = skillsGetPlayerBonus(player, RAIDS_CLASS_SKILL_HEALING_FIELD);
    if (args.HealingAmount > 0)
      args.Type |= SKILLFIELD_TYPE_HEALING;
  }

  // check for ammo
  if (!(caster->CooldownMask & SKILLFIELD_TYPE_AMMO)
      || (int)((u32)args.StartTime - (u32)caster->AmmoActivatedAt) >= SKILLS_FIELD_COOLDOWN_MS) {
    args.AmmoAmount = skillsGetPlayerBonus(player, RAIDS_CLASS_SKILL_AMMO_FIELD);
    if (args.AmmoAmount > 0)
      args.Type |= SKILLFIELD_TYPE_AMMO;
  }

  // try spawn skillfield
  if (args.Type != SKILLFIELD_TYPE_NONE && skillfieldCreate(&args)) {
    if (args.Type & SKILLFIELD_TYPE_HEALING)
      caster->HealingActivatedAt = args.StartTime;
    if (args.Type & SKILLFIELD_TYPE_AMMO)
      caster->AmmoActivatedAt = args.StartTime;
    caster->CooldownMask |= args.Type;
  }
}

//--------------------------------------------------------------------------
void skillsFieldsRegisterCooldowns(Player* player)
{
  struct SkillsFieldCasterState* caster = &skillsFieldCasters[player->PlayerId];
  struct {
    enum RaidsClassSkillId SkillId;
    int Type;
    int IconTexId;
    u32 Color;
    int ActivatedAt;
  } fields[] = {
    { RAIDS_CLASS_SKILL_HEALING_FIELD, SKILLFIELD_TYPE_HEALING, 94, 0x8000E080, caster->HealingActivatedAt },
    { RAIDS_CLASS_SKILL_AMMO_FIELD, SKILLFIELD_TYPE_AMMO, 90, 0x8000FFFF, caster->AmmoActivatedAt }
  };

  int now = gameGetTime();
  int i;
  for (i = 0; i < COUNT_OF(fields); ++i) {
    if (skillsGetPlayerBonus(player, fields[i].SkillId) <= 0) continue;

    float cooldown = 0;
    if (caster->CooldownMask & fields[i].Type)
      cooldown = 1 - (int)((u32)now - (u32)fields[i].ActivatedAt) / (float)SKILLS_FIELD_COOLDOWN_MS;
    skillsRegisterCooldown(fields[i].SkillId, fields[i].IconTexId, fields[i].Color, cooldown, 1);
  }
}

//--------------------------------------------------------------------------
int skillsHasNearbyGoldBolt(Player* player)
{
  if (!skillsPlayerIsReady(player) || MapConfig.State->CurrentMapStats.Invalid
      || skillsGetPlayerBonus(player, RAIDS_CLASS_SKILL_GOLD_BOLT_INDICATOR) <= 0) {
    memset(&skillsGoldBoltState, 0, sizeof(skillsGoldBoltState));
    return 0;
  }

  if (skillsGoldBoltState.CacheTicks > 0)
    --skillsGoldBoltState.CacheTicks;

  u32 collectiblesMask = MapConfig.State->CurrentMapStats.CollectiblesMask;
  if (skillsGoldBoltState.CollectiblesMask == collectiblesMask
      && skillsGoldBoltState.CacheTicks > 0)
    return skillsGoldBoltState.HasNearby;

  skillsGoldBoltState.CollectiblesMask = collectiblesMask;
  skillsGoldBoltState.CacheTicks = SKILLS_GOLD_BOLT_CACHE_TICKS;
  skillsGoldBoltState.HasNearby = 0;

  Moby* moby = mobyListGetStart();
  while ((moby = mobyFindNextByOClass(moby, COLLECTIBLE_OCLASS))) {
    if (!mobyIsDestroyed(moby) && moby->PVar) {
      int index = ((struct CollectiblePVar*)moby->PVar)->Index;
      if (index >= 0 && index < 32 && !(collectiblesMask & (1U << index))
          && vector_sqrdistance(player->PlayerPosition, moby->Position) <= 400) {
        skillsGoldBoltState.HasNearby = 1;
        break;
      }
    }
    ++moby;
  }
  return skillsGoldBoltState.HasNearby;
}

//--------------------------------------------------------------------------
void skillsGoldIndicatorTick(Player* player)
{
  if (skillsHasNearbyGoldBolt(player))
    skillsRegisterCooldown(RAIDS_CLASS_SKILL_GOLD_BOLT_INDICATOR, 4, 0x8000DFFF, 0, 0);
}

//--------------------------------------------------------------------------
void skillsRegisterCooldown(enum RaidsClassSkillId skillId, int iconTexId, u32 color, float cooldown, int showCooldownBar)
{
  if (!skillsIsValid(skillId) || iconTexId < 0) return;

  struct SkillsCooldownHudEntry* entry = &skillsCooldownHud[skillId];
  entry->Registered = 1;
  entry->IconTexId = iconTexId;
  entry->Color = color;
  entry->Cooldown = clamp(cooldown, 0, 1);
  entry->ShowCooldownBar = showCooldownBar != 0;
}

//--------------------------------------------------------------------------
void skillsDrawCooldowns(void)
{
  int i, count = 0;
  for (i = 0; i < COUNT_OF(skillsCooldownHud); ++i)
    count += skillsCooldownHud[i].Registered != 0;
  if (!count) return;

  float iconSize = SKILLS_COOLDOWN_HUD_ICON_SIZE;
  float spacing = SKILLS_COOLDOWN_HUD_SPACING;
  float barHeight = 2;
  // Wrap additional skills into rows while preserving left-to-right ordering.
  int columns = (int)((SCREEN_WIDTH - 40 + spacing) / (iconSize + spacing));
  if (columns < 1) columns = 1;
  if (columns > count) columns = count;
  int rows = (count + columns - 1) / columns;
  float cellHeight = iconSize + barHeight + spacing;
  Window_t window;
  windowCreate(&window, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT - SKILLS_COOLDOWN_HUD_BOTTOM,
    0, 0, columns * (iconSize + spacing) - spacing, rows * cellHeight - spacing, TEXT_ALIGN_BOTTOMCENTER);
  windowSetScreen(&window, 0);

  int index = 0;
  for (i = 0; i < COUNT_OF(skillsCooldownHud); ++i) {
    struct SkillsCooldownHudEntry* entry = &skillsCooldownHud[i];
    if (!entry->Registered) continue;

    float x = (index % columns) * (iconSize + spacing);
    float y = (index / columns) * cellHeight;
    u32 color = colorLerp(0, entry->Color, entry->Cooldown > 0 ? 0.5f : 1.0f);
    windowDrawSprite(&window, TEXT_ALIGN_TOPLEFT, x + 1, y + 1, iconSize, iconSize,
      entry->IconTexId, 0x20000000, TEXT_ALIGN_TOPLEFT);
    windowDrawSprite(&window, TEXT_ALIGN_TOPLEFT, x, y, iconSize, iconSize,
      entry->IconTexId, color, TEXT_ALIGN_TOPLEFT);
    if (entry->ShowCooldownBar && entry->Cooldown > 0) {
      windowDrawBox(&window, TEXT_ALIGN_TOPLEFT, x, y + iconSize, iconSize, barHeight,
        RAIDS_MENU_TRACK_COLOR, TEXT_ALIGN_TOPLEFT);
      windowDrawBox(&window, TEXT_ALIGN_TOPLEFT, x, y + iconSize, ceilf(iconSize * entry->Cooldown), barHeight,
        0x80FFFFFF, TEXT_ALIGN_TOPLEFT);
    }
    ++index;
  }
}

//--------------------------------------------------------------------------
void skillsFrameTick(void)
{
  Player* player = playerGetFromSlot(0);
  PlayerHUDFlags* hudFlags = hudGetPlayerFlags(0);
  if (!skillsPlayerIsReady(player) || !player->IsLocal || playerIsDead(player) || player->Health <= 0
      || !hudFlags || !hudFlags->Flags.Raw || MapConfig.State->MenuOpen != RAIDS_CUSTOM_MENU_NONE || shouldCloseMenu())
    return;

  gfxSetupGifPaging(0);
  skillsDrawCooldowns();
  gfxDoGifPaging();
}

//--------------------------------------------------------------------------
void skillsResetPlayer(Player* player)
{
  if (!player || player->PlayerId < 0 || player->PlayerId >= GAME_MAX_PLAYERS) return;

  memset(&skillsImmunityStates[player->PlayerId], 0, sizeof(skillsImmunityStates[0]));
  memset(&skillsAmmoStates[player->PlayerId], 0, sizeof(skillsAmmoStates[0]));
  struct SkillsMovementState* state = &skillsMovementStates[player->PlayerId];
  memset(state, 0, sizeof(*state));
  state->ClassType = -1;
  state->HoverTicks = -1; // Landing arms hover; switching classes in midair does not.
}

//--------------------------------------------------------------------------
struct SkillsMovementState* skillsSyncMovementState(Player* player)
{
  if (!skillsPlayerIsReady(player)) return NULL;

  struct SkillsMovementState* state = &skillsMovementStates[player->PlayerId];
  RaidsPlayerClassProgress_t* playerClass = bankGetPlayerEquippedClass(player->PlayerId);
  int extraJumps = (int)skillsGetPlayerBonus(player, RAIDS_CLASS_SKILL_EXTRA_JUMPS);
  int hoverDuration = (int)(TPS * skillsGetPlayerBonus(player, RAIDS_CLASS_SKILL_HOVERBOOTS));

  if (state->Player != player || state->ClassType != playerClass->ClassType
      || state->ExtraJumpLimit != extraJumps || state->HoverDuration != hoverDuration) {
    skillsResetPlayer(player);
    state->Player = player;
    state->ClassType = playerClass->ClassType;
    state->ExtraJumpLimit = extraJumps;
    state->HoverDuration = hoverDuration;
  }
  return state;
}

//--------------------------------------------------------------------------
int skillsPlayerHasMovementInput(Player* player)
{
  return skillsPlayerIsReady(player) && player->IsLocal && !playerIsDead(player)
    && player->Health > 0 && !player->InVehicle && !player->timers.noInput
    && MapConfig.State->MenuOpen == RAIDS_CUSTOM_MENU_NONE && !shouldCloseMenu();
}

//--------------------------------------------------------------------------
void skillsExtraJumpsOnDoubleJump(Player* player, int stateId, int a2, int a3, int t0)
{
  PlayerVTable* vtable = playerGetVTable(player);
  if (!vtable || !vtable->UpdateState) return;

  struct SkillsMovementState* state = skillsPlayerHasMovementInput(player) ? skillsSyncMovementState(player) : NULL;
  if (state && state->ExtraJumpLimit > 0
      && (player->PlayerState == PLAYER_STATE_JUMP || player->PlayerState == PLAYER_STATE_RUN_JUMP)
      && (state->ExtraJumps < state->ExtraJumpLimit || player->timers.state < 6)) {
    POKE_U8((u32)player + 0x1CA0 + 0xFF, 0); // Survival's double-jump/flip flag.
    return;
  }
  if (state)
    state->ExtraJumps = 0;
  vtable->UpdateState(player, stateId, a2, a3, t0);
}

//--------------------------------------------------------------------------
void skillsExtraJumpsUpdate(Player* player, struct SkillsMovementState* state)
{
  if (player->Ground.onGood || (player->PlayerState != PLAYER_STATE_JUMP && player->PlayerState != PLAYER_STATE_RUN_JUMP)) {
    state->ExtraJumps = 0;
    return;
  }

  if (state->ExtraJumpLimit <= 0) return;

  int mappedJump = padGetMappedPad(0x40, player->PlayerId);
  PlayerVTable* vtable = playerGetVTable(player);
  if (mappedJump > 0 && vtable && vtable->UpdateState && player->timers.state > 6
      && playerPadGetAnyButtonDown(player, mappedJump << 8) > 0) {
    if (state->ExtraJumps < state->ExtraJumpLimit) {
      ++state->ExtraJumps;
      vtable->UpdateState(player, PLAYER_STATE_JUMP, 1, 1, 1);
      POKE_U8((u32)player + 0x1CA0 + 0xFF, 0);
    } else {
      POKE_U8((u32)player + 0x1CA0 + 0xFF, 1);
    }
  }
}

//--------------------------------------------------------------------------
void skillsHoverbootsUpdate(Player* player, struct SkillsMovementState* state)
{
  if (player->Ground.onGood) {
    state->HoverTicks = 0;
    return;
  }

  int canHover = player->PlayerState == PLAYER_STATE_WALK || player->PlayerState == PLAYER_STATE_IDLE
    || player->PlayerState == PLAYER_STATE_FALL || player->PlayerState == PLAYER_STATE_GET_HIT;
  if (!canHover) {
    state->HoverTicks = -1;
    return;
  }

  if (state->HoverDuration > 0 && state->HoverTicks >= 0 && state->HoverTicks < state->HoverDuration) {
    player->Ground.offGood = 0;
    ++state->HoverTicks;

    if ((state->HoverTicks % 10) == 0) {
      mobySpawnExplosion(vector_read(player->PlayerPosition),
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, NULL,
        0, 0, 0, 0, 0, 0, 0, 0, 0x80003080, 0, NULL, NULL, 0, 0.5, 0, 0, 0);
    }
  }
}

//--------------------------------------------------------------------------
void skillsOnPlayerUpdate(Player* player)
{
  if (!skillsPlayerIsReady(player) || playerIsDead(player)) {
    skillsResetPlayer(player);
    return;
  }

  struct SkillsMovementState* state = skillsSyncMovementState(player);
  skillsDamageImmunityUpdate(player);
  if (player->IsLocal)
    skillsSyncAmmoState(player);
  if (!skillsPlayerHasMovementInput(player)) {
    if (!player->Ground.onGood)
      state->HoverTicks = -1;
    return;
  }
  skillsHoverbootsUpdate(player, state);
}

//--------------------------------------------------------------------------
void skillsTick(void)
{
  // reset each tick
  memset(skillsCooldownHud, 0, sizeof(skillsCooldownHud));

  // iterate each player
  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = playerGetFromIndex(i);
    if (!playerIsValid(player) || !playerIsConnected(player))
      memset(&skillsFieldCasters[i], 0, sizeof(skillsFieldCasters[i]));
    if (!skillsPlayerIsReady(player) || playerIsDead(player)) {
      memset(&skillsMovementStates[i], 0, sizeof(skillsMovementStates[i]));
      memset(&skillsImmunityStates[i], 0, sizeof(skillsImmunityStates[i]));
      memset(&skillsAmmoStates[i], 0, sizeof(skillsAmmoStates[i]));
      skillsMovementStates[i].ClassType = -1;
      skillsMovementStates[i].HoverTicks = -1;
      continue;
    }

    struct SkillsMovementState* state = skillsSyncMovementState(player);
    if (skillsPlayerHasMovementInput(player)) {
      // Poll in the map tick, before dlPostUpdate advances the map's pad history.
      skillsExtraJumpsUpdate(player, state);
    }
  }

  // handle local player
  Player* localPlayer = playerGetFromSlot(0);
  if (skillsPlayerIsReady(localPlayer) && localPlayer->IsLocal && !playerIsDead(localPlayer) && localPlayer->Health > 0) {
    skillsFieldsActivate(localPlayer);
    skillsFieldsRegisterCooldowns(localPlayer);
    skillsGoldIndicatorTick(localPlayer);
  }

  // misc ticks
  skillsVehiclesTick();
}

//--------------------------------------------------------------------------
void skillsInit(void)
{
  memset(skillsMovementStates, 0, sizeof(skillsMovementStates));
  memset(skillsImmunityStates, 0, sizeof(skillsImmunityStates));
  memset(skillsAmmoStates, 0, sizeof(skillsAmmoStates));
  memset(skillsVehicleStates, 0, sizeof(skillsVehicleStates));
  memset(skillsFieldCasters, 0, sizeof(skillsFieldCasters));
  memset(skillsCooldownHud, 0, sizeof(skillsCooldownHud));
  memset(&skillsGoldBoltState, 0, sizeof(skillsGoldBoltState));
  HOOK_JAL(0x0060C660, &skillsExtraJumpsOnDoubleJump);
}

//--------------------------------------------------------------------------
void skillsCleanup(void)
{
  memset(skillsFieldCasters, 0, sizeof(skillsFieldCasters));
  memset(skillsImmunityStates, 0, sizeof(skillsImmunityStates));
  memset(skillsAmmoStates, 0, sizeof(skillsAmmoStates));
  memset(skillsMovementStates, 0, sizeof(skillsMovementStates));
}
