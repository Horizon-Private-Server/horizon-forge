#include <float.h>
#include <libdl/stdio.h>
#include <libdl/math.h>
#include "skills.h"

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
  switch (skillId) {
    case RAIDS_CLASS_SKILL_DAMAGE: return 1.0f + (skillsGetDamageFactor(skillPoints) - 1.0f) * multiplier;
    case RAIDS_CLASS_SKILL_MOVEMENT_SPEED: return 1.0f + (skillsGetMovementSpeedFactor(skillPoints) - 1.0f) * multiplier;
    case RAIDS_CLASS_SKILL_CLOSE_RANGE_DAMAGE:
    case RAIDS_CLASS_SKILL_LONG_RANGE_DAMAGE:
    case RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE:
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
      // and map-authored forced aggro remain effective when gameplay is implemented.
      return maxf(0.5f, 1.0f - 0.10f * skillPoints * multiplier);
    default: return 1.0f;
  }
}

//--------------------------------------------------------------------------
float skillsGetBonus(enum RaidsClassSkillId skillId, int skillPoints, float multiplier)
{
  if (skillPoints <= 0 || !(multiplier > 0 && multiplier <= FLT_MAX)) return 0.0f;
  switch (skillId) {
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
    case RAIDS_CLASS_SKILL_CRITICAL_CHANCE: return 2.0f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_DEADEYE:
      // Percentage points while stationary with Fusion; total critical chance caps at 100%.
      return 20.0f * skillPoints * multiplier;
    case RAIDS_CLASS_SKILL_DAMAGE_IMMUNITY: return floorf(3.0f * skillPoints * multiplier);
    case RAIDS_CLASS_SKILL_HOVERBOOTS: return 0.5f * skillPoints * multiplier;
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
  if (skillPoints < 0) skillPoints = 0;
  float bonus = skillsGetBonus(skillId, skillPoints, multiplier);
  switch (skillId) {
    case RAIDS_CLASS_SKILL_EXTRA_JUMPS:
      snprintf(buf, bufSize, "+%d jump%s", (int)bonus, bonus == 1 ? "" : "s");
      break;
    case RAIDS_CLASS_SKILL_ALPHA_AREA:
    case RAIDS_CLASS_SKILL_ALPHA_SPEED:
    case RAIDS_CLASS_SKILL_ALPHA_NANOLEECH:
    case RAIDS_CLASS_SKILL_ALPHA_AMMO:
      snprintf(buf, bufSize, "+%d mod%s", (int)bonus, bonus == 1 ? "" : "s");
      break;
    case RAIDS_CLASS_SKILL_HEALING_FIELD: snprintf(buf, bufSize, "+%.0f HP/pulse", bonus); break;
    case RAIDS_CLASS_SKILL_AMMO_FIELD: snprintf(buf, bufSize, "%.2fx drop/pulse", bonus); break;
    case RAIDS_CLASS_SKILL_CRITICAL_CHANCE: snprintf(buf, bufSize, "+%.0f%% crit", bonus); break;
    case RAIDS_CLASS_SKILL_DAMAGE_IMMUNITY: snprintf(buf, bufSize, "+%.0f ticks", bonus); break;
    case RAIDS_CLASS_SKILL_HOVERBOOTS:
    case RAIDS_CLASS_SKILL_WALL_WALKING:
      snprintf(buf, bufSize, "+%.1f sec", bonus);
      break;
    case RAIDS_CLASS_SKILL_SECOND_WIND: snprintf(buf, bufSize, "+%.0f HP/kill", bonus); break;
    case RAIDS_CLASS_SKILL_DEADEYE:
      snprintf(buf, bufSize, "+%.0f%% crit chance", bonus);
      break;
    case RAIDS_CLASS_SKILL_COMBAT_MEDIC: snprintf(buf, bufSize, "+%.0f HP/hit", bonus); break;
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
      else snprintf(buf, bufSize, "--");
      break;
  }
}

//--------------------------------------------------------------------------
char* skillsGetName(enum RaidsClassSkillId skillId)
{
  switch (skillId) {
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
  switch (skillId) {
    case RAIDS_CLASS_SKILL_DAMAGE: return "Increase damage.";
    case RAIDS_CLASS_SKILL_MOVEMENT_SPEED: return "Increase movement speed.";
    case RAIDS_CLASS_SKILL_CLOSE_RANGE_DAMAGE: return "Increase damage against nearby mobs.";
    case RAIDS_CLASS_SKILL_LONG_RANGE_DAMAGE: return "Increase damage against distant mobs.";
    case RAIDS_CLASS_SKILL_EXTRA_JUMPS: return "Add extra jumps before landing.";
    case RAIDS_CLASS_SKILL_CHARGEBOOT_LENGTH: return "Extend chargeboot travel duration.";
    case RAIDS_CLASS_SKILL_WRENCH_DAMAGE: return "Increase wrench damage.";
    case RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE: return "Increase the base chance of an ammo drop.";
    case RAIDS_CLASS_SKILL_HEALING_FIELD: return "Heal nearby allies per pulse. Has a cooldown.";
    case RAIDS_CLASS_SKILL_AMMO_FIELD: return "Refill allies per pulse. Uses ammo drops. Has cooldown.";
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
