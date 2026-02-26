#ifndef SURVIVAL_UTILS_H
#define SURVIVAL_UTILS_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>
#include "game.h"

extern struct SurvivalMapConfig MapConfig;
extern struct SurvivalBakedConfig bakedConfig;
extern char LocalPlayerStrBuffer[2][64];

void playPaidSound(Player *player);
int tryPlayerInteract(Moby *moby, Player *player, char *message, char *lowerMessage, int boltCost, int tokenCost, int actionCooldown, float sqrDistance, int btns, int hold);

int mobyIsMob(Moby *moby);
void playerTeleportToSpawn(Player *player, float dealtDamagePercentOfMaxHealth);
int playerGetItemCount(Player *player, int itemId);
void playerGiveAlphaMod(Player *player, int alphamod);
int localPlayerHasInput(void);

int bakedSpawnGetFirst(int bakedSpawnType, VECTOR outPos, VECTOR outRot);

void mapApplyFixes(void);

void mapPrintGambit(int gambit);
void mapSendSendGambitCompletedMessage(int gambit);
void mapLocalPlayerEnforceSingleWeaponRestriction(int localPlayerIdx, int weaponId, int hard);
void mapEnforceSingleWeaponRestriction(int weaponId);

void spawnHealthBomb(Player *fromPlayer, VECTOR position, float radius, float healPercent);
void randomizeWeaponPickups(void);

void addRadarBlip(Moby *moby, int life, int type, int team);

int survivalIsPaused(void);

#endif // SURVIVAL_UTILS_H
