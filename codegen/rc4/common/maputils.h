#ifndef COMMON_MAP_UTILS_H
#define COMMON_MAP_UTILS_H

#define TPS (60)

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>

typedef int (*CanSelectIndex_func)(void* userdata, int index);

void mapInstallMobyFunctions(MobyFunctions* mobyFunctions);
Moby * spawnExplosion(VECTOR position, float size, u32 color);
Moby * spawnExplosionDamage(VECTOR position, float size, u32 color, Moby* damager, float damage, u32 damageFlags);
void damageRadius(Moby* moby, VECTOR position, u32 damageFlags, float damage, float damageRadius);
GuberEvent* guberCreateEvent(Moby* moby, u32 eventType);

struct PartInstance * spawnParticle(VECTOR position, u32 color, char opacity, int idx);
void destroyParticle(struct PartInstance* particle);

float getSignedSlope(VECTOR forward, VECTOR normal);
float getSignedRelativeSlope(VECTOR forward, VECTOR normal);

u8 decTimerU8(u8* timeValue);
u16 decTimerU16(u16* timeValue);
u32 decTimerU32(u32* timeValue);

void pushSnack(int localPlayerIdx, char* string, int ticksAlive);

int isInDrawDist(Moby* moby);

void * mobyGetClassPtr(int oClass);
Moby* mobyGetFromIdxOrNull(int mobyIdx);
Player* mobyGetPlayer(Moby* moby);
Moby* playerGetTargetMoby(Player* player);

void draw3DMarker(VECTOR position, float scale, u32 color, char* str);

void playDialog(short dialogId, int force);

void transformToSplitscreenPixelCoordinates(int localPlayerIndex, float *x, float *y);

int selectRandomIndex(int count, void* userdata, CanSelectIndex_func canSelectIndexFunc);

void replenishAmmo(Player* player);
void respawnAllPlayers(void);

void blowCorn(Moby* moby);
int countBits(u32 value);

int allClientsReady(void);

#endif // COMMON_MAP_UTILS_H
