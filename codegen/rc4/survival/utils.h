

#ifndef SURVIVAL_UTILS_H
#define SURVIVAL_UTILS_H

#define SEQ_DIFF_U8(a, b) (((b - a + 128*3) % (128*2)) - 128)

#if DEBUG
  #define DEBUG_ONLY(s) s
#else
  #define DEBUG_ONLY(s) 
#endif

#if RELEASE
  #define RELEASE_ONLY(s) s
#else
  #define RELEASE_ONLY(s) 
#endif

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>

void setFreeze(int isActive);
void setDoublePoints(int isActive);
void setDoubleXP(int isActive);
void playerRevive(Player* player, int fromPlayerId);
int playerGetStackableCount(int playerId, int stackable);
int playerHasBlessing(int playerId, int blessing);
Moby * spawnExplosion(VECTOR position, float size, u32 color);
void playUpgradeSound(Player* player);
void playPaidSound(Player* player);
int getWeaponIdFromOClass(short oclass);
u8 decTimerU8(u8* timeValue);
u16 decTimerU16(u16* timeValue);
u32 decTimerU32(u32* timeValue);
u32 getXpForNextToken(int counter);
void drawDreadTokenIcon(float x, float y, float scale);
struct PartInstance * spawnParticle(VECTOR position, u32 color, char opacity, int idx);
void destroyParticle(struct PartInstance* particle);

int intArrayContains(int* list, int count, int value);
int charArrayContains(char* list, int count, char value);

void vectorProjectOnVertical(VECTOR output, VECTOR input0);
void vectorProjectOnHorizontal(VECTOR output, VECTOR input0);
float getSignedSlope(VECTOR forward, VECTOR normal);

int mobyIsMob(Moby* moby);
Player* mobyGetPlayer(Moby* moby);
Moby* playerGetTargetMoby(Player* player);
int localPlayerHasInput(void);

void transformToSplitscreenPixelCoordinates(int localPlayerIndex, float *x, float *y);

#endif // SURVIVAL_UTILS_H
