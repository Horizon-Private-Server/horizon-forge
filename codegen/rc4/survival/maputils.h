

#ifndef SURVIVAL_MAP_UTILS_H
#define SURVIVAL_MAP_UTILS_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>
#include "game.h"


extern struct SurvivalMapConfig MapConfig;

Moby * spawnExplosion(VECTOR position, float size, u32 color);
void damageRadius(Moby* moby, VECTOR position, u32 damageFlags, float damage, float damageRadius);
void playPaidSound(Player* player);
int tryPlayerInteract(Moby* moby, Player* player, char* message, char* lowerMessage, int boltCost, int tokenCost, int actionCooldown, float sqrDistance, int btns);
GuberEvent* guberCreateEvent(Moby* moby, u32 eventType);

struct PartInstance * spawnParticle(VECTOR position, u32 color, char opacity, int idx);
void destroyParticle(struct PartInstance* particle);

float getSignedSlope(VECTOR forward, VECTOR normal);
float getSignedRelativeSlope(VECTOR forward, VECTOR normal);

u8 decTimerU8(u8* timeValue);
u16 decTimerU16(u16* timeValue);
u32 decTimerU32(u32* timeValue);

void pushSnack(int localPlayerIdx, char* string, int ticksAlive);
void uiShowLowerPopup(int localPlayerIdx, int msgStringId);

int isInDrawDist(Moby* moby);

int mobyIsMob(Moby* moby);
Player* mobyGetPlayer(Moby* moby);
Moby* playerGetTargetMoby(Player* player);
int playerHasBlessing(int playerId, int blessing);
int playerGetStackableCount(int playerId, int stackable);

void draw3DMarker(VECTOR position, float scale, u32 color, char* str);

void playDialog(short dialogId, int force);

void transformToSplitscreenPixelCoordinates(int localPlayerIndex, float *x, float *y);

int bakedSpawnGetFirst(int bakedSpawnType, VECTOR outPos, VECTOR outRot);

#endif // SURVIVAL_MAP_UTILS_H
