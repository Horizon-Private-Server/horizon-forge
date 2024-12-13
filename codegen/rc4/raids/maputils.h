

#ifndef RAIDS_MAP_UTILS_H
#define RAIDS_MAP_UTILS_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>
#include "game.h"


extern struct RaidsMapConfig MapConfig;

typedef int (*CanSelectIndex_func)(void* userdata, int index);


Moby * spawnExplosion(VECTOR position, float size, u32 color);
void damageRadius(Moby* moby, VECTOR position, u32 damageFlags, float damage, float damageRadius);
void playPaidSound(Player* player);
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

void * mobyGetClassPtr(int oClass);
int mobyIsMob(Moby* moby);
int mobyIsNpc(Moby* moby);
Moby* mobyGetFromIdxOrNull(int mobyIdx);
Player* mobyGetPlayer(Moby* moby);
Moby* playerGetTargetMoby(Player* player);

void draw3DMarker(VECTOR position, float scale, u32 color, char* str);

void playDialog(short dialogId, int force);

void transformToSplitscreenPixelCoordinates(int localPlayerIndex, float *x, float *y);

int selectRandomIndex(int count, void* userdata, CanSelectIndex_func canSelectIndexFunc);

int hasPendingWorldHop(void);
int isOnHubWorld(void);
int missionIsComplete(void);
int missionIsActive(void);

int bankTryChargeLocalAccount(u32 cost);

int getAmmoRefillCost(Player* player);
void replenishAmmo(Player* player);
void respawnAllPlayers(void);

#endif // RAIDS_MAP_UTILS_H
