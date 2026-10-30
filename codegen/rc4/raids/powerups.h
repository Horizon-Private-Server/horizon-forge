#ifndef RAIDS_POWERUPS_H
#define RAIDS_POWERUPS_H

#include <libdl/player.h>

struct MobDamageEventArgs;

// Resolved by RaidsModeData from the final build sprite list.
extern int powerupsQuadIconTexId;
extern int powerupsShieldIconTexId;

void powerupsOnMobDamaged(Moby* moby, struct MobDamageEventArgs* args, float appliedDamage);
void powerupsOnTargetDamaged(Player* player, Moby* target, int weaponId, float appliedDamage);
int powerupsIsEnemyDamage(Moby* source);
void powerupsOnPlayerDamaged(Player* player, float healthLost, float maxHealth);
void powerupsResetPlayer(Player* player);
void powerupsTick(void);
void powerupsInit(void);
void powerupsCleanup(void);

#endif // RAIDS_POWERUPS_H
