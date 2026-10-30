#include <float.h>
#include <libdl/string.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include "bank.h"
#include "common.h"

const RaidsClassDef_t classDefinitions[] = {
  {
    .ClassType = RAIDS_CLASS_GUNNER,
    .Tier = 1,
    .Name = "Gunner",
    .ExperienceCurveMultiplier = 1.0f,
    .WeaponMaxRarities = { RAIDS_ITEM_RARITY_UNCOMMON, -1, -1, -1, -1, -1, -1, -1 },
    .Skills = {
      // Vipers: speed mods, area mods, projectile speed, then rarity.
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_SPEED, .MaxRank = 2, .X = 3, .Y = 4, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AREA, .MaxRank = 2, .X = 3, .Y = 5, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VIPER_PROJECTILE_SPEED, .MaxRank = 1, .X = 2, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VIPER_MAX_RARITY, .MaxRank = 1, .X = 1, .Y = 5, .Multiplier = 1.0f },

      // Fusion: damage, critical chance, then weapon access and rarity.
      { .SkillId = RAIDS_CLASS_SKILL_DAMAGE, .MaxRank = 3, .X = 3, .Y = 2, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_CRITICAL_CHANCE, .MaxRank = 3, .X = 3, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_FUSION_MAX_RARITY, .MaxRank = 3, .X = 2, .Y = 1, .Multiplier = 1.0f },

      // Ammo: drops and capacity branch into pickup efficiency or Holoshield access.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 5, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 5, .Y = 2, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_HOLOSHIELD_MAX_RARITY, .MaxRank = 1, .X = 5, .Y = 4, .Multiplier = 1.0f },
    },
  },
  {
    .ClassType = RAIDS_CLASS_BOMBER,
    .Tier = 1,
    .Name = "Bomber",
    .ExperienceCurveMultiplier = 1.0f,
    .WeaponMaxRarities = { -1, -1, -1, -1, -1, RAIDS_ITEM_RARITY_UNCOMMON, -1, -1 },
    .Skills = {
      // B6: impact mods, area mods, projectile speed, then rarity.
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_IMPACT, .MaxRank = 2, .X = 3, .Y = 4, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AREA, .MaxRank = 2, .X = 3, .Y = 5, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_B6_PROJECTILE_SPEED, .MaxRank = 1, .X = 2, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_B6_MAX_RARITY, .MaxRank = 1, .X = 1, .Y = 5, .Multiplier = 1.0f },

      // Arbiter: damage, blast reach, then weapon access and rarity.
      { .SkillId = RAIDS_CLASS_SKILL_DAMAGE, .MaxRank = 3, .X = 3, .Y = 2, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_BLAST_REACH, .MaxRank = 3, .X = 3, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ARBITER_MAX_RARITY, .MaxRank = 3, .X = 2, .Y = 1, .Multiplier = 1.0f },

      // Ammo: drops, capacity, then pickup efficiency.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 5, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 5, .Y = 2, .Multiplier = 1.0f },
    },
  },
  {
    .ClassType = RAIDS_CLASS_BRAWLER,
    .Tier = 1,
    .Name = "Brawler",
    .ExperienceCurveMultiplier = 1.0f,
    .WeaponMaxRarities = { -1, -1, -1, -1, -1, -1, RAIDS_ITEM_RARITY_UNCOMMON, -1 },
    .Skills = {
      // Flail: damage, area mods, critical chance, then rarity.
      { .SkillId = RAIDS_CLASS_SKILL_DAMAGE, .MaxRank = 3, .X = 2, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AREA, .MaxRank = 2, .X = 2, .Y = 4, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_CRITICAL_CHANCE, .MaxRank = 3, .X = 2, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_FLAIL_MAX_RARITY, .MaxRank = 1, .X = 3, .Y = 5, .Multiplier = 1.0f },

      // Wrench: nanoleech mods, movement, then wrench damage.
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_NANOLEECH, .MaxRank = 2, .X = 3, .Y = 2, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_MOVEMENT_SPEED, .MaxRank = 3, .X = 3, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_WRENCH_DAMAGE, .MaxRank = 5, .X = 2, .Y = 1, .Multiplier = 1.0f },

      // Ammo: drops and capacity branch into Magma Cannon, pickup efficiency, or Holoshield access.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 5, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_MAGMA_CANNON_MAX_RARITY, .MaxRank = 3, .X = 5, .Y = 4, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 5, .Y = 2, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_HOLOSHIELD_MAX_RARITY, .MaxRank = 1, .X = 6, .Y = 3, .Multiplier = 1.0f },
    },
  },
  {
    .ClassType = RAIDS_CLASS_MARKSMAN,
    .Tier = 2,
    .Name = "Marksman",
    .ExperienceCurveMultiplier = 2.0f,
    .WeaponMaxRarities = { RAIDS_ITEM_RARITY_RARE, -1, -1, RAIDS_ITEM_RARITY_UNCOMMON, -1, -1, -1, -1 },
    .UnlockRulesCount = 1,
    .UnlockRules = { { RAIDS_CLASS_GUNNER, 10 } },
    .Skills = {
      // Vipers: speed mods, area mods, projectile speed, then rarity.
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_SPEED, .MaxRank = 2, .X = 3, .Y = 4, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AREA, .MaxRank = 2, .X = 3, .Y = 5, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VIPER_PROJECTILE_SPEED, .MaxRank = 1, .X = 2, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VIPER_MAX_RARITY, .MaxRank = 1, .X = 1, .Y = 5, .Multiplier = 1.0f },

      // Fusion: damage, range, critical chance, then rarity.
      { .SkillId = RAIDS_CLASS_SKILL_DAMAGE, .MaxRank = 3, .X = 3, .Y = 2, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_LONG_RANGE_DAMAGE, .MaxRank = 3, .X = 3, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_CRITICAL_CHANCE, .MaxRank = 3, .X = 2, .Y = 1, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_FUSION_MAX_RARITY, .MaxRank = 1, .X = 1, .Y = 1, .Multiplier = 1.0f },

      // Ammo: drops, capacity, then pickup efficiency.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 5, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 5, .Y = 2, .Multiplier = 1.0f },
    },
  },
  {
    .ClassType = RAIDS_CLASS_GRENADIER,
    .Tier = 2,
    .Name = "Grenadier",
    .ExperienceCurveMultiplier = 2.0f,
    .WeaponMaxRarities = { -1, -1, RAIDS_ITEM_RARITY_UNCOMMON, -1, -1, RAIDS_ITEM_RARITY_RARE, -1, -1 },
    .UnlockRulesCount = 1,
    .UnlockRules = { { RAIDS_CLASS_BOMBER, 10 } },
    .Skills = {
      // B6: impact mods, area mods, projectile speed, rarity, then cluster bombs.
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_IMPACT, .MaxRank = 2, .X = 3, .Y = 4, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AREA, .MaxRank = 2, .X = 3, .Y = 5, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_B6_PROJECTILE_SPEED, .MaxRank = 1, .X = 2, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_B6_MAX_RARITY, .MaxRank = 1, .X = 1, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_CLUSTER_BOMBS, .MaxRank = 1, .X = 1, .Y = 4, .Multiplier = 1.0f },

      // Arbiter: damage and blast reach branch into rocket guidance or vehicles.
      { .SkillId = RAIDS_CLASS_SKILL_DAMAGE, .MaxRank = 3, .X = 3, .Y = 2, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_BLAST_REACH, .MaxRank = 2, .X = 3, .Y = 1, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ARBITER_MAX_RARITY, .MaxRank = 1, .X = 2, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VISIBOMB, .MaxRank = 1, .X = 1, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VEHICLE_DAMAGE, .MaxRank = 3, .X = 4, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VEHICLE_HEALTH, .MaxRank = 3, .X = 5, .Y = 1, .Multiplier = 1.0f },

      // Ammo: drops, capacity, then pickup efficiency.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 5, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 5, .Y = 4, .Multiplier = 1.0f },
    },
  },
  {
    .ClassType = RAIDS_CLASS_MELEE,
    .Tier = 2,
    .Name = "Berserker",
    .ExperienceCurveMultiplier = 2.0f,
    .WeaponMaxRarities = { -1, RAIDS_ITEM_RARITY_UNCOMMON, -1, -1, -1, -1, RAIDS_ITEM_RARITY_RARE, -1 },
    .UnlockRulesCount = 1,
    .UnlockRules = { { RAIDS_CLASS_BRAWLER, 10 } },
    .Skills = {
      // Flail: damage and close-range damage branch into rarity or smash protection.
      { .SkillId = RAIDS_CLASS_SKILL_DAMAGE, .MaxRank = 3, .X = 3, .Y = 4, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_CLOSE_RANGE_DAMAGE, .MaxRank = 3, .X = 3, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_FLAIL_MAX_RARITY, .MaxRank = 1, .X = 2, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_UNSTOPPABLE_SMASH, .MaxRank = 4, .X = 4, .Y = 5, .Multiplier = 1.0f },

      // Wrench: health, nanoleech mods, movement, then wrench damage.
      { .SkillId = RAIDS_CLASS_SKILL_MAX_HEALTH, .MaxRank = 4, .X = 3, .Y = 2, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_NANOLEECH, .MaxRank = 2, .X = 3, .Y = 1, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_MOVEMENT_SPEED, .MaxRank = 3, .X = 2, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_WRENCH_DAMAGE, .MaxRank = 5, .X = 1, .Y = 1, .Multiplier = 2.0f },

      // Ammo: drops, capacity, then branch into Magma Cannon or pickup efficiency.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 5, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_MAGMA_CANNON_MAX_RARITY, .MaxRank = 1, .X = 5, .Y = 4, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_BLOOD_FRENZY, .MaxRank = 3, .X = 5, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 5, .Y = 2, .Multiplier = 1.0f },
    },
  },
  {
    .ClassType = RAIDS_CLASS_SCOUT,
    .Tier = 2,
    .Name = "Scout",
    .ExperienceCurveMultiplier = 2.0f,
    .WeaponMaxRarities = { RAIDS_ITEM_RARITY_RARE, -1, -1, -1, -1, -1, RAIDS_ITEM_RARITY_UNCOMMON, -1 },
    .UnlockRulesCount = 2,
    .UnlockRules = { { RAIDS_CLASS_GUNNER, 5 }, { RAIDS_CLASS_BRAWLER, 5 } },
    .Skills = {
      // Flail: movement and chargeboots branch into rarity or concealment.
      { .SkillId = RAIDS_CLASS_SKILL_MOVEMENT_SPEED, .MaxRank = 3, .X = 3, .Y = 2, .Multiplier = 1.5f },
      { .SkillId = RAIDS_CLASS_SKILL_CHARGEBOOT_LENGTH, .MaxRank = 3, .X = 3, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_FLAIL_MAX_RARITY, .MaxRank = 1, .X = 2, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_STEALTH, .MaxRank = 3, .X = 4, .Y = 1, .Multiplier = 1.0f },

      // Vipers: extra jumps, airborne damage, then rarity.
      { .SkillId = RAIDS_CLASS_SKILL_EXTRA_JUMPS, .MaxRank = 2, .X = 3, .Y = 4, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_AIR_SUPERIORITY, .MaxRank = 3, .X = 3, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VIPER_MAX_RARITY, .MaxRank = 1, .X = 2, .Y = 5, .Multiplier = 1.0f },

      // Ammo: drops, capacity, then pickup efficiency.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 5, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 5, .Y = 2, .Multiplier = 1.0f },
    },
  },
  {
    .ClassType = RAIDS_CLASS_SUPPORT,
    .Tier = 2,
    .Name = "Support",
    .ExperienceCurveMultiplier = 2.0f,
    .WeaponMaxRarities = { -1, -1, -1, -1, RAIDS_ITEM_RARITY_RARE, -1, -1, RAIDS_ITEM_RARITY_RARE },
    .UnlockRulesCount = 2,
    .UnlockRules = { { RAIDS_CLASS_BOMBER, 5 }, { RAIDS_CLASS_GUNNER, 5 } },
    .Skills = {
      // Mines: impact mods, area mods, blast reach, then rarity.
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_IMPACT, .MaxRank = 2, .X = 3, .Y = 4, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AREA, .MaxRank = 2, .X = 3, .Y = 5, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_BLAST_REACH, .MaxRank = 3, .X = 2, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_MINE_LAUNCHER_MAX_RARITY, .MaxRank = 1, .X = 1, .Y = 5, .Multiplier = 1.0f },

      // Shields: health, damage immunity, then Holoshield rarity.
      { .SkillId = RAIDS_CLASS_SKILL_MAX_HEALTH, .MaxRank = 3, .X = 3, .Y = 2, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_DAMAGE_IMMUNITY, .MaxRank = 3, .X = 3, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_HOLOSHIELD_MAX_RARITY, .MaxRank = 1, .X = 2, .Y = 1, .Multiplier = 1.0f },

      // Ammo: drops and capacity branch into pickup efficiency or team resupply.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 5, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 5, .Y = 2, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_FIELD, .MaxRank = 3, .X = 5, .Y = 4, .Multiplier = 1.0f },
    },
  },
  {
    .ClassType = RAIDS_CLASS_SHARPSHOOTER,
    .Tier = 3,
    .Name = "Sharpshooter",
    .ExperienceCurveMultiplier = 3.0f,
    .WeaponMaxRarities = { RAIDS_ITEM_RARITY_RARE, -1, RAIDS_ITEM_RARITY_RARE, RAIDS_ITEM_RARITY_LEGENDARY, -1, -1, -1, -1 },
    .UnlockRulesCount = 1,
    .UnlockRules = { { RAIDS_CLASS_MARKSMAN, 10 } },
    .Skills = {
      // Vipers: speed mods, area mods, projectile speed, then rarity.
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_SPEED, .MaxRank = 2, .X = 3, .Y = 4, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AREA, .MaxRank = 2, .X = 3, .Y = 5, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VIPER_PROJECTILE_SPEED, .MaxRank = 1, .X = 2, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VIPER_MAX_RARITY, .MaxRank = 1, .X = 1, .Y = 5, .Multiplier = 1.0f },

      // Fusion: damage, range, critical chance, rarity, then Deadeye.
      { .SkillId = RAIDS_CLASS_SKILL_DAMAGE, .MaxRank = 3, .X = 3, .Y = 2, .Multiplier = 3.0f },
      { .SkillId = RAIDS_CLASS_SKILL_LONG_RANGE_DAMAGE, .MaxRank = 3, .X = 3, .Y = 1, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_CRITICAL_CHANCE, .MaxRank = 3, .X = 2, .Y = 1, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_FUSION_MAX_RARITY, .MaxRank = 1, .X = 1, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_DEADEYE, .MaxRank = 3, .X = 1, .Y = 2, .Multiplier = 1.0f },

      // Ammo: drops and capacity branch into pickup efficiency or Arbiter rarity.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 5, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 5, .Y = 2, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ARBITER_MAX_RARITY, .MaxRank = 1, .X = 5, .Y = 4, .Multiplier = 1.0f },

      // Survival.
      { .SkillId = RAIDS_CLASS_SKILL_MAX_HEALTH, .MaxRank = 3, .X = 2, .Y = 3, .Multiplier = 2.0f },
    },
  },
  {
    .ClassType = RAIDS_CLASS_DEMOLITIONIST,
    .Tier = 3,
    .Name = "Demolitionist",
    .ExperienceCurveMultiplier = 3.0f,
    .WeaponMaxRarities = { -1, RAIDS_ITEM_RARITY_RARE, RAIDS_ITEM_RARITY_LEGENDARY, -1, -1, RAIDS_ITEM_RARITY_LEGENDARY, -1, -1 },
    .UnlockRulesCount = 1,
    .UnlockRules = { { RAIDS_CLASS_GRENADIER, 10 } },
    .Skills = {
      // B6: impact mods, area mods, projectile speed, rarity, then two cluster bombs.
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_IMPACT, .MaxRank = 2, .X = 3, .Y = 2, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AREA, .MaxRank = 2, .X = 3, .Y = 1, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_B6_PROJECTILE_SPEED, .MaxRank = 1, .X = 4, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_B6_MAX_RARITY, .MaxRank = 1, .X = 5, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_CLUSTER_BOMBS, .MaxRank = 2, .X = 5, .Y = 2, .Multiplier = 1.0f },

      // Arbiter: damage and blast reach branch into rocket guidance or vehicles.
      { .SkillId = RAIDS_CLASS_SKILL_DAMAGE, .MaxRank = 3, .X = 2, .Y = 3, .Multiplier = 3.0f },
      { .SkillId = RAIDS_CLASS_SKILL_BLAST_REACH, .MaxRank = 2, .X = 1, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ARBITER_MAX_RARITY, .MaxRank = 1, .X = 1, .Y = 2, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VISIBOMB, .MaxRank = 1, .X = 1, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VEHICLE_DAMAGE, .MaxRank = 3, .X = 1, .Y = 4, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VEHICLE_HEALTH, .MaxRank = 3, .X = 1, .Y = 5, .Multiplier = 1.0f },

      // Ammo: capacity branches into Magma rarity or efficient supplies leading to health.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 4, .Y = 4, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 4, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_MAX_HEALTH, .MaxRank = 3, .X = 3, .Y = 5, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_MAGMA_CANNON_MAX_RARITY, .MaxRank = 1, .X = 5, .Y = 4, .Multiplier = 1.0f },
    },
  },
  {
    .ClassType = RAIDS_CLASS_JUGGERNAUT,
    .Tier = 3,
    .Name = "Juggernaut",
    .ExperienceCurveMultiplier = 3.0f,
    .WeaponMaxRarities = { -1, RAIDS_ITEM_RARITY_LEGENDARY, -1, -1, -1, -1, RAIDS_ITEM_RARITY_LEGENDARY, -1 },
    .UnlockRulesCount = 1,
    .UnlockRules = { { RAIDS_CLASS_MELEE, 10 } },
    .Skills = {
      // Flail: damage and close-range damage branch into rarity or flinch immunity.
      { .SkillId = RAIDS_CLASS_SKILL_DAMAGE, .MaxRank = 3, .X = 3, .Y = 4, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_CLOSE_RANGE_DAMAGE, .MaxRank = 3, .X = 3, .Y = 5, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_FLAIL_MAX_RARITY, .MaxRank = 1, .X = 2, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_FLINCH_IMMUNITY, .MaxRank = 4, .X = 4, .Y = 5, .Multiplier = 1.0f },

      // Wrench: health, nanoleech, movement, wrench damage, then low-health kill healing.
      { .SkillId = RAIDS_CLASS_SKILL_MAX_HEALTH, .MaxRank = 4, .X = 3, .Y = 2, .Multiplier = 3.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_NANOLEECH, .MaxRank = 2, .X = 3, .Y = 1, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_MOVEMENT_SPEED, .MaxRank = 3, .X = 2, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_WRENCH_DAMAGE, .MaxRank = 5, .X = 1, .Y = 1, .Multiplier = 3.0f },
      { .SkillId = RAIDS_CLASS_SKILL_SECOND_WIND, .MaxRank = 3, .X = 1, .Y = 2, .Multiplier = 1.0f },

      // Ammo: drops and capacity branch into Magma rarity or pickup efficiency.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 5, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_MAGMA_CANNON_MAX_RARITY, .MaxRank = 1, .X = 5, .Y = 4, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_BLOOD_FRENZY, .MaxRank = 3, .X = 5, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_IRONCLAD, .MaxRank = 3, .X = 2, .Y = 2, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 5, .Y = 2, .Multiplier = 1.0f },
    },
  },
  {
    .ClassType = RAIDS_CLASS_INFILTRATOR,
    .Tier = 3,
    .Name = "Infiltrator",
    .ExperienceCurveMultiplier = 3.0f,
    .WeaponMaxRarities = { RAIDS_ITEM_RARITY_LEGENDARY, -1, RAIDS_ITEM_RARITY_RARE, -1, -1, -1, RAIDS_ITEM_RARITY_RARE, -1 },
    .UnlockRulesCount = 1,
    .UnlockRules = { { RAIDS_CLASS_SCOUT, 10 } },
    .Skills = {
      // Flail: movement, chargeboot length, then rarity.
      { .SkillId = RAIDS_CLASS_SKILL_MOVEMENT_SPEED, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_CHARGEBOOT_LENGTH, .MaxRank = 3, .X = 5, .Y = 3, .Multiplier = 3.0f },
      { .SkillId = RAIDS_CLASS_SKILL_FLAIL_MAX_RARITY, .MaxRank = 1, .X = 5, .Y = 2, .Multiplier = 1.0f },

      // Vipers: jumps and hovering branch into wall walking and rarity or airborne damage.
      { .SkillId = RAIDS_CLASS_SKILL_EXTRA_JUMPS, .MaxRank = 2, .X = 3, .Y = 4, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_HOVERBOOTS, .MaxRank = 3, .X = 3, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_WALL_WALKING, .MaxRank = 3, .X = 4, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_VIPER_MAX_RARITY, .MaxRank = 1, .X = 5, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_AIR_SUPERIORITY, .MaxRank = 3, .X = 2, .Y = 5, .Multiplier = 2.0f },

      // Ammo: drops and capacity branch into pickup efficiency or Arbiter rarity.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 2, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 1, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 1, .Y = 4, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ARBITER_MAX_RARITY, .MaxRank = 1, .X = 1, .Y = 2, .Multiplier = 1.0f },

      // Recon: health, concealment, then gold bolt detection.
      { .SkillId = RAIDS_CLASS_SKILL_MAX_HEALTH, .MaxRank = 3, .X = 3, .Y = 2, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_STEALTH, .MaxRank = 3, .X = 3, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_GOLD_BOLT_INDICATOR, .MaxRank = 1, .X = 2, .Y = 1, .Multiplier = 1.0f },
    },
  },
  {
    .ClassType = RAIDS_CLASS_FIELD_MEDIC,
    .Tier = 3,
    .Name = "Field Medic",
    .ExperienceCurveMultiplier = 3.0f,
    .WeaponMaxRarities = { -1, -1, -1, RAIDS_ITEM_RARITY_RARE, RAIDS_ITEM_RARITY_LEGENDARY, -1, -1, RAIDS_ITEM_RARITY_LEGENDARY },
    .UnlockRulesCount = 1,
    .UnlockRules = { { RAIDS_CLASS_SUPPORT, 10 } },
    .Skills = {
      // Mines: impact mods, area mods, blast reach, then rarity.
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_IMPACT, .MaxRank = 2, .X = 3, .Y = 4, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AREA, .MaxRank = 2, .X = 3, .Y = 5, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_BLAST_REACH, .MaxRank = 3, .X = 2, .Y = 5, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_MINE_LAUNCHER_MAX_RARITY, .MaxRank = 1, .X = 1, .Y = 5, .Multiplier = 1.0f },

      // Defense: health and immunity branch into Holoshield rarity or team healing.
      { .SkillId = RAIDS_CLASS_SKILL_MAX_HEALTH, .MaxRank = 3, .X = 3, .Y = 2, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_DAMAGE_IMMUNITY, .MaxRank = 3, .X = 3, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_HOLOSHIELD_MAX_RARITY, .MaxRank = 1, .X = 2, .Y = 1, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_HEALING_FIELD, .MaxRank = 3, .X = 4, .Y = 1, .Multiplier = 3.0f },

      // Fusion: nanoleech, rarity, then direct-hit teammate healing.
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_NANOLEECH, .MaxRank = 2, .X = 2, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_FUSION_MAX_RARITY, .MaxRank = 1, .X = 1, .Y = 3, .Multiplier = 1.0f },
      { .SkillId = RAIDS_CLASS_SKILL_COMBAT_MEDIC, .MaxRank = 3, .X = 1, .Y = 2, .Multiplier = 1.0f },

      // Ammo: drops and capacity branch into pickup efficiency or team resupply.
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE, .MaxRank = 3, .X = 4, .Y = 3, .Multiplier = 3.0f },
      { .SkillId = RAIDS_CLASS_SKILL_ALPHA_AMMO, .MaxRank = 2, .X = 5, .Y = 3, .Multiplier = 2.0f },
      { .SkillId = RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES, .MaxRank = 3, .X = 5, .Y = 4, .Multiplier = 3.0f },
      { .SkillId = RAIDS_CLASS_SKILL_AMMO_FIELD, .MaxRank = 3, .X = 5, .Y = 2, .Multiplier = 2.0f },
    },
  },
};

int classesDefinitionsValid = 0;

//--------------------------------------------------------------------------
int classesGetSkillMaxRank(const RaidsClassDef_t* def, int skillIdx)
{
  if (!def || def->Tier < 1 || def->Tier > 3 || skillIdx < 0 || skillIdx >= RAIDS_MAX_CLASS_SKILLS) return 0;
  const RaidsClassSkillDef_t* skill = &def->Skills[skillIdx];
  if (!skillsIsValid(skill->SkillId) || !skill->MaxRank) return 0;
  if (skillsIsUnlock(skill->SkillId)) return 1;
  int maxRank = skill->MaxRank;
  if (skill->SkillId == RAIDS_CLASS_SKILL_STEALTH && maxRank > 3) maxRank = 3;
  return maxRank;
}

//--------------------------------------------------------------------------
int classesGridIsAdjacent(int x, int y, int otherX, int otherY)
{
  int dx = x - otherX;
  int dy = y - otherY;
  return dx*dx + dy*dy == 1;
}

//--------------------------------------------------------------------------
int classesValidateDefinitions(void)
{
  int count = COUNT_OF(classDefinitions);
  if (count <= 0 || count > RAIDS_MAX_CLASSES) return 0;
  int i, j, k, pass;
  int hasDefault = 0;
  for (i = 0; i < count; ++i) {
    const RaidsClassDef_t* def = &classDefinitions[i];
    if (def->ClassType < 0 || def->ClassType >= RAIDS_MAX_CLASSES || def->Tier < 1 || def->Tier > 3 || !def->Name || !def->Name[0]
        || strlen(def->Name) > 31 || !(def->ExperienceCurveMultiplier > 0)
        || def->UnlockRulesCount < 0 || def->UnlockRulesCount > RAIDS_MAX_CLASS_UNLOCK_RULES) return 0;
    for (j = 0; j < i; ++j) {
      if (classDefinitions[j].ClassType == def->ClassType || !strcmp(classDefinitions[j].Name, def->Name)) return 0;
    }
    for (j = 0; j < 8; ++j) {
      if (def->WeaponMaxRarities[j] < -1 || def->WeaponMaxRarities[j] >= RAIDS_ITEM_RARITY_COUNT) return 0;
    }
    if (def->ClassType == RAIDS_CLASS_GUNNER) {
      if (def->UnlockRulesCount) return 0;
      hasDefault = 1;
    }
    for (j = 0; j < def->UnlockRulesCount; ++j) {
      const RaidsClassUnlockRule_t* rule = &def->UnlockRules[j];
      if (rule->ClassType == def->ClassType || rule->RequiredLevel < 1 || rule->RequiredLevel > RAIDS_CLASS_MAX_LEVEL) return 0;
      for (k = 0; k < count && classDefinitions[k].ClassType != rule->ClassType; ++k) {}
      if (k == count) return 0;
      for (k = 0; k < j; ++k) if (def->UnlockRules[k].ClassType == rule->ClassType) return 0;
    }

    int distances[RAIDS_MAX_CLASS_SKILLS];
    for (j = 0; j < RAIDS_MAX_CLASS_SKILLS; ++j) {
      const RaidsClassSkillDef_t* skill = &def->Skills[j];
      distances[j] = RAIDS_MAX_CLASS_SKILLS + 1;
      if (!skill->SkillId) {
        if (skill->MaxRank || skill->X || skill->Y || skill->MinClassLevel || skill->Multiplier) return 0;
        continue;
      }
      if (!skillsIsValid(skill->SkillId) || !skill->MaxRank || !(skill->Multiplier > 0 && skill->Multiplier <= FLT_MAX)
          || skill->MinClassLevel > RAIDS_CLASS_MAX_LEVEL
          || skill->X >= RAIDS_CLASS_GRID_SIZE || skill->Y >= RAIDS_CLASS_GRID_SIZE
          || (skill->X == RAIDS_CLASS_GRID_CENTER && skill->Y == RAIDS_CLASS_GRID_CENTER)) return 0;
      if (skillsIsUnlock(skill->SkillId) && (skill->MaxRank != 1 || skill->Multiplier != 1)) return 0;
      if (skill->SkillId == RAIDS_CLASS_SKILL_STEALTH && skill->MaxRank > 3) return 0;
      for (k = 0; k < j; ++k) {
        if (def->Skills[k].SkillId == skill->SkillId
            || (def->Skills[k].SkillId && def->Skills[k].X == skill->X && def->Skills[k].Y == skill->Y)) return 0;
      }
      if (classesGridIsAdjacent(skill->X, skill->Y, RAIDS_CLASS_GRID_CENTER, RAIDS_CLASS_GRID_CENTER)) distances[j] = 1;
    }

    // Grid loops are allowed. Find each node's shortest route from the free center.
    for (pass = 0; pass < RAIDS_MAX_CLASS_SKILLS; ++pass) {
      for (j = 0; j < RAIDS_MAX_CLASS_SKILLS; ++j) {
        if (!def->Skills[j].SkillId) continue;
        for (k = 0; k < RAIDS_MAX_CLASS_SKILLS; ++k) {
          if (def->Skills[k].SkillId && distances[k] + 1 < distances[j]
              && classesGridIsAdjacent(def->Skills[j].X, def->Skills[j].Y, def->Skills[k].X, def->Skills[k].Y))
            distances[j] = distances[k] + 1;
        }
      }
    }
    for (j = 0; j < RAIDS_MAX_CLASS_SKILLS; ++j) {
      if (!def->Skills[j].SkillId) continue;
      // One point per preceding tile plus this cap must fit the ten points earned by level X.
      if (distances[j] > RAIDS_MAX_CLASS_SKILLS || distances[j] - 1 + classesGetSkillMaxRank(def, j) > RAIDS_CLASS_MAX_LEVEL) return 0;
    }
  }

  int resolved = 0;
  for (pass = 0; pass < count; ++pass) {
    for (i = 0; i < count; ++i) {
      const RaidsClassDef_t* def = &classDefinitions[i];
      for (j = 0; j < def->UnlockRulesCount; ++j) {
        for (k = 0; k < count; ++k) {
          if (classDefinitions[k].ClassType == def->UnlockRules[j].ClassType) break;
        }
        if (!(resolved & (1 << k))) break;
      }
      if (j == def->UnlockRulesCount) resolved |= 1 << i;
    }
  }
  return hasDefault && resolved == (1 << count) - 1;
}

//--------------------------------------------------------------------------
void classesInit(void)
{
  classesDefinitionsValid = classesValidateDefinitions();
  if (!classesDefinitionsValid) DPRINTF("Invalid class definitions\n");
}

//--------------------------------------------------------------------------
const RaidsClassDef_t* classesGetDefinitionByIdx(int idx)
{
  if (!classesDefinitionsValid || idx < 0 || idx >= COUNT_OF(classDefinitions)) return NULL;
  return &classDefinitions[idx];
}

//--------------------------------------------------------------------------
const RaidsClassDef_t* classesGetDefinition(int classType)
{
  if (!classesDefinitionsValid) return NULL;
  int i;
  for (i = 0; i < COUNT_OF(classDefinitions); ++i) {
    if (classDefinitions[i].ClassType == classType) return &classDefinitions[i];
  }
  return NULL;
}
