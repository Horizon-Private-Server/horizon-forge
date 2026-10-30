

#ifndef RAIDS_MAP_UTILS_H
#define RAIDS_MAP_UTILS_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>
#include "window.h"
#include "game.h"
#include "maputils.h"

#define WRAP_DISTANCE(value, max) (((value % max) + max) % max)

extern struct RaidsMapConfig MapConfig;

void playEquipRejectSound(Player* player);
void playEquipSound(Player* player);
void playUpgradeSound(Player* player);
void playPaidSound(Player* player);

enum WEAPON_IDS getWeaponIdFromDamageSource(enum MobDamageSource source);

int getProficiencyFromXp(double xp);
double getXpForProficiency(int level);
int getLevelFromXp(u64 xp);
u64 getXpForLevel(int level);

int mobyIsMob(Moby* moby);
int mobyIsNpc(Moby* moby);

int hasPendingWorldHop(void);
int isOnHubWorld(void);
int shouldCloseMenu(void);
int shouldCloseHubMenu(void);
int missionIsFailed(void);
int missionIsComplete(void);
int missionIsActive(void);
int missionIsBossRaid(void);
int missionIsOpenWorld(void);

int bankTryChargeLocalAccount(Player* player, u32 cost);

int getAmmoRefillCost(Player* player);

#endif // RAIDS_MAP_UTILS_H
