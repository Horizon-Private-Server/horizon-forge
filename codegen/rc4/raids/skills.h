#ifndef RAIDS_SKILLS_H
#define RAIDS_SKILLS_H

#include <libdl/player.h>

struct RaidsPlayerStats;
struct RaidsPlayerDamageContext;
struct VehicleBase;

enum RaidsClassSkillId
{
  RAIDS_CLASS_SKILL_NONE = 0,
  RAIDS_CLASS_SKILL_DAMAGE = 1,
  RAIDS_CLASS_SKILL_MOVEMENT_SPEED = 2,
  RAIDS_CLASS_SKILL_CLOSE_RANGE_DAMAGE = 3,
  RAIDS_CLASS_SKILL_LONG_RANGE_DAMAGE = 4,
  RAIDS_CLASS_SKILL_EXTRA_JUMPS = 5,
  RAIDS_CLASS_SKILL_CHARGEBOOT_LENGTH = 6,
  RAIDS_CLASS_SKILL_WRENCH_DAMAGE = 7,
  RAIDS_CLASS_SKILL_AMMO_DROP_CHANCE = 8,
  RAIDS_CLASS_SKILL_HEALING_FIELD = 9,
  RAIDS_CLASS_SKILL_AMMO_FIELD = 10,
  RAIDS_CLASS_SKILL_CRITICAL_CHANCE = 11,
  RAIDS_CLASS_SKILL_DAMAGE_IMMUNITY = 12,
  RAIDS_CLASS_SKILL_ALPHA_AREA = 13,
  RAIDS_CLASS_SKILL_ALPHA_SPEED = 14,
  RAIDS_CLASS_SKILL_ALPHA_NANOLEECH = 15,
  RAIDS_CLASS_SKILL_FLINCH_IMMUNITY = 16,
  RAIDS_CLASS_SKILL_HOVERBOOTS = 17,
  RAIDS_CLASS_SKILL_VISIBOMB = 18,
  RAIDS_CLASS_SKILL_WALL_WALKING = 19,
  RAIDS_CLASS_SKILL_GOLD_BOLT_INDICATOR = 20,
  RAIDS_CLASS_SKILL_VEHICLE_DAMAGE = 21,
  RAIDS_CLASS_SKILL_VEHICLE_HEALTH = 22,
  RAIDS_CLASS_SKILL_MAX_HEALTH = 23,
  RAIDS_CLASS_SKILL_ALPHA_AMMO = 24,
  RAIDS_CLASS_SKILL_VIPER_PROJECTILE_SPEED = 25,
  RAIDS_CLASS_SKILL_B6_PROJECTILE_SPEED = 26,
  RAIDS_CLASS_SKILL_BLAST_REACH = 27,
  RAIDS_CLASS_SKILL_SECOND_WIND = 28,
  RAIDS_CLASS_SKILL_AIR_SUPERIORITY = 29,
  RAIDS_CLASS_SKILL_EFFICIENT_SUPPLIES = 30,
  RAIDS_CLASS_SKILL_STEALTH = 31,
  RAIDS_CLASS_SKILL_DEADEYE = 32,
  RAIDS_CLASS_SKILL_COMBAT_MEDIC = 33,
  RAIDS_CLASS_SKILL_CLUSTER_BOMBS = 34,
  RAIDS_CLASS_SKILL_UNSTOPPABLE_SMASH = 35,
  RAIDS_CLASS_SKILL_VIPER_MAX_RARITY = 36,
  RAIDS_CLASS_SKILL_MAGMA_CANNON_MAX_RARITY = 37,
  RAIDS_CLASS_SKILL_ARBITER_MAX_RARITY = 38,
  RAIDS_CLASS_SKILL_FUSION_MAX_RARITY = 39,
  RAIDS_CLASS_SKILL_MINE_LAUNCHER_MAX_RARITY = 40,
  RAIDS_CLASS_SKILL_B6_MAX_RARITY = 41,
  RAIDS_CLASS_SKILL_FLAIL_MAX_RARITY = 42,
  RAIDS_CLASS_SKILL_HOLOSHIELD_MAX_RARITY = 43,
  RAIDS_CLASS_SKILL_ALPHA_IMPACT = 44,
  RAIDS_CLASS_SKILL_BLOOD_FRENZY = 45,
  RAIDS_CLASS_SKILL_IRONCLAD = 46,
  RAIDS_CLASS_SKILL_COUNT
};

// Factors are multipliers; unlearned and unknown skills return 1.
float skillsGetFactor(enum RaidsClassSkillId skillId, int skillPoints, float multiplier);

// Flat bonuses use the skill's units (counts, seconds, ticks, HP, ammo fractions, or percentage points).
float skillsGetBonus(enum RaidsClassSkillId skillId, int skillPoints, float multiplier);
int skillsIsValid(enum RaidsClassSkillId skillId);
int skillsIsUnlock(enum RaidsClassSkillId skillId);
enum RaidsClassSkillId skillsGetWeaponRaritySkillId(int weaponSlot);
void skillsFormatRank(enum RaidsClassSkillId skillId, int skillPoints, float multiplier, char* buf, int bufSize);

char* skillsGetName(enum RaidsClassSkillId skillId);
char* skillsGetDescription(enum RaidsClassSkillId skillId);

int skillsPlayerIsReady(Player* player);
float skillsGetPlayerFactor(Player* player, enum RaidsClassSkillId skillId);
float skillsGetPlayerBonus(Player* player, enum RaidsClassSkillId skillId);
float skillsGetExplosionMinDamage(Player* player, int weaponId, float fullDamage, float minDamage);
float skillsGetAmmoDropChance(Player* player, float baseChance);
int skillsGetAlphaModBonus(Player* player, int alphaModId);
float skillsGetDetectionFactor(Player* player);
void skillsOnHealthboxHeal(Player* player, float amount);
void skillsGiveAmmo(Player* player, int weaponId, float amount);
void skillsOnAmmoPickup(Player* player, int weaponId, int amount);
void skillsOnVehicleInitialized(struct VehicleBase* vehicle);
void skillsModifyPlayerStats(Player* player, struct RaidsPlayerStats* stats);
void skillsModifyPlayerCritChance(struct RaidsPlayerDamageContext* context);
void skillsModifyPlayerCritDamage(struct RaidsPlayerDamageContext* context);
void skillsFinalizePlayerDamage(struct RaidsPlayerDamageContext* context);
void skillsOnMobKilled(Moby* moby, int killedByPlayerId);
void skillsRegisterCooldown(enum RaidsClassSkillId skillId, int iconTexId, u32 color, float cooldown, int showCooldownBar);
void skillsRegisterPowerupCooldown(enum RaidsClassSkillId skillId, int iconTexId, float cooldown, float progress, int active);
void skillsDrawCooldowns(void);
void skillsResetPlayer(Player* player);
void skillsOnPlayerUpdate(Player* player);
void skillsTick(void);
void skillsFrameTick(void);
void skillsInit(void);
void skillsCleanup(void);

#endif // RAIDS_SKILLS_H
