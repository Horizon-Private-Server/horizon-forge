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

void playPaidSound(Player* player);
int tryPlayerInteract(Moby* moby, Player* player, char* message, char* lowerMessage, int boltCost, int tokenCost, int actionCooldown, float sqrDistance, int btns);

int mobyIsMob(Moby* moby);
int playerHasBlessing(int playerId, int blessing);
int playerGetStackableCount(int playerId, int stackable);
int localPlayerHasInput(void);

int bakedSpawnGetFirst(int bakedSpawnType, VECTOR outPos, VECTOR outRot);

void mapApplyFixes(void);

void mapPrintGambit(int gambit);
void mapSendSendGambitCompletedMessage(int gambit);
void mapLocalPlayerEnforceSingleWeaponRestriction(int localPlayerIdx, int weaponId, int hard);
void mapEnforceSingleWeaponRestriction(int weaponId);

#endif // SURVIVAL_UTILS_H
