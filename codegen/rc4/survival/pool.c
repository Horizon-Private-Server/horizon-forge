/***************************************************
 * FILENAME :		pool.c
 * 
 * DESCRIPTION :
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <tamtypes.h>

#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include "messageid.h"
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/hud.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/collision.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include "pool.h"
#include "shared.h"

extern struct SurvivalMapConfig MapConfig;

#define POOL_MAX_COUNT (256)

Moby* poolMobys[POOL_MAX_COUNT] = {0};
PoolGroup_t poolGroups[] = {
  { .MobyOClass = 0x13A1, .PoolSize = 16 },
  { .MobyOClass = 0x0070, .PoolSize = 8 },
  { .MobyOClass = 0x007A, .PoolSize = 8 },
  { .MobyOClass = 0x20E2, .PoolSize = 8 },
  { .MobyOClass = 0x1F8C, .PoolSize = 8 },
  { .MobyOClass = 0x20B8, .PoolSize = 8 },
};

//--------------------------------------------------------------------------
PoolGroup_t* poolGetGroup(int oclass)
{
  int i;
  for (i = 0; i < COUNT_OF(poolGroups); ++i) {
    if (poolGroups[i].MobyOClass == oclass)
      return &poolGroups[i];
  }

  return NULL;
}

//--------------------------------------------------------------------------
Moby* poolGetMoby(PoolGroup_t* group, int idx)
{
  if (!group) return NULL;

  // out of bounds
  if (idx < 0 || idx >= group->PoolSize) return NULL;
  
  return poolMobys[group->PoolStart + idx];
}

//--------------------------------------------------------------------------
void poolSetMoby(PoolGroup_t* group, int idx, Moby* moby)
{
  if (!group) return;

  // out of bounds
  if (idx < 0 || idx >= group->PoolSize) return;
  
  poolMobys[group->PoolStart + idx] = moby;
}

//--------------------------------------------------------------------------
void poolInitMoby(Moby* moby)
{
  // reset
  moby->ModeBits = 0;
  moby->CollDamage = -1;
  moby->Group = -1;
  moby->LSeq = -1;
  moby->UpdateId = 0;
  moby->SoundTrigger = 0;
  moby->SoundChannel = -1;
  moby->GridMinX = 0x7f;
  moby->GridMinY = 0x7f;
  moby->GridMaxX = 0x80;
  moby->GridMaxY = 0x80;
  moby->OcclIndex = 0x7f80;
  matrix_unit(moby->M0_03);
  ((void (*)(Moby*))0x004f99d8)(moby); // SetMobyLightDefault()

  // init
  if (moby->PClass) {
    moby->Scale = *(float*)(moby->PClass + 0x24);
    moby->CollData = *(void**)(moby->PClass + 0x10);
    moby->CollActive = 0;
    moby->GlowRGBA = *(u32*)(moby->PClass + 0x40);
    moby->ModeBits2 = *(u8*)(moby->PClass + 0x47);

    void* animSeq = *(void**)(moby->PClass + 0x48);
    if (animSeq) {
      moby->AnimSeq = animSeq;
      moby->AnimSpeed = 1;
      moby->AnimSeqId = 0;
      moby->SoundTrigger = *(char*)(animSeq + 0x12);
      moby->SoundDesired = *(char*)(animSeq + 0x11);
      if (*(u8*)(animSeq + 0x10) >= 2) {
        moby->ModeBits &= ~MOBY_MODE_BIT_NO_UPDATE;
      }
      if (*(u8*)(moby->PClass + 0xc) == 1 && *(u8*)(animSeq + 0x10) < 2) {
        moby->AnimSpeed = 0;
        if (*(char*)(animSeq + 0x11) < 0) {
          moby->ModeBits |= MOBY_MODE_BIT_UNK_40;
        }
      }
    }
  } else {
    moby->ModeBits |= MOBY_MODE_BIT_UNK_40 | MOBY_MODE_BIT_NO_POST_UPDATE | MOBY_MODE_BIT_DISABLED;
  }

  if (moby->GlowRGBA) moby->ModeBits |= MOBY_MODE_BIT_HAS_GLOW;
  if (moby->JointCnt == 0) moby->ModeBits |= MOBY_MODE_BIT_UNK_40;
  if (moby->Shadow) moby->ModeBits |= MOBY_MODE_BIT_DRAW_SHADOW;
  if (moby->PUpdate == 0) moby->ModeBits |= MOBY_MODE_BIT_NO_UPDATE;
}

//--------------------------------------------------------------------------
Moby* poolSpawn(int oclass, int pvarSize)
{
  PoolGroup_t* group = poolGetGroup(oclass);
  if (!group || group->PoolSize <= 0) return mobySpawn(oclass, pvarSize);
  
  int poolIdx = group->PoolIndex;
  Moby* moby = poolGetMoby(group, poolIdx);
  if (!moby || mobyIsDestroyed(moby)) {

#if DEBUG_POOL
    if (moby && mobyIsDestroyed(moby)) { DPRINTF("POOL FOUND ALREADY DESTROYED MOBY %08X (%04X)\n", (u32)moby, moby->OClass); }
#endif

    moby = mobySpawn(oclass, pvarSize);
    poolSetMoby(group, poolIdx, moby);

#if DEBUG_POOL
    DPRINTF("POOL OCLASS:%d SPAWN NEW MOBY %d %08X (pvar:0x%x)\n", oclass, poolIdx, (u32)moby, pvarSize);
#endif

  } else {

#if DEBUG_POOL
    DPRINTF("POOL OCLASS:%d REUSE MOBY %d %08X (pvar:0x%x)\n", oclass, poolIdx, (u32)moby, pvarSize);
#endif

    if (moby->ModeBits & MOBY_MODE_BIT_DISABLED) {

      void* pvar = moby->PVar;
      int deathCount = moby->Xp;
      ((void (*)(Moby*, int, int))0x004f7330)(moby, oclass, 1); // init moby instance
      moby->Xp = (deathCount + 1) % 128; // indicate pool moby respawned
      if (pvar) {
        memset(pvar, 0, pvarSize);
        moby->PVar = pvar;
      }
    } else {

      poolInitMoby(moby);
      moby->Xp = (moby->Xp + 1) % 128; // indicate pool moby respawned
      if (moby->PVar) {
        memset(moby->PVar, 0, pvarSize);
      }
    }

    // void* pvar = moby->PVar;
    // int deathCount = moby->Xp;
    // ((void (*)(Moby*, int, int))0x004f7330)(moby, oclass, 1); // init moby instance
    // moby->Xp = (deathCount + 1) % 128; // indicate pool moby respawned
    // if (pvar) {
    //   memset(pvar, 0, pvarSize);
    //   moby->PVar = pvar;
    // }
  }

  moby->Pad = 0x80 | poolIdx;
  group->PoolIndex = (poolIdx + 1) % group->PoolSize;
  return moby;
}

//--------------------------------------------------------------------------
void poolDestroy(Moby* moby)
{
  PoolGroup_t* group = poolGetGroup(moby->OClass);
  if (!group || group->PoolSize <= 0) {
    mobyDestroy(moby);
    return;
  }
  
  int poolIdx = (u8)moby->Pad & ~0x80;
  if (poolIdx >= 0 && poolIdx < group->PoolSize && poolGetMoby(group, poolIdx) == moby) {
    moby->ModeBits |= MOBY_MODE_BIT_NO_POST_UPDATE | MOBY_MODE_BIT_DISABLED | MOBY_MODE_BIT_NO_UPDATE;
    moby->CollActive = -1;

#if DEBUG_POOL
    DPRINTF("POOL OCLASS:%d DISABLE MOBY %d %08X\n", moby->OClass, poolIdx, (u32)moby);
#endif

  } else {

#if DEBUG_POOL
    DPRINTF("POOL OCLASS:%d MISSING MOBY %d %08X... DESTROYING\n", moby->OClass, poolIdx, (u32)moby);
#endif

    mobyDestroy(moby);
  }
}

//--------------------------------------------------------------------------
void poolHookSpawn(u32 addr)
{
  HOOK_JAL(addr, &poolSpawn);
}

//--------------------------------------------------------------------------
void poolHookDestroy(u32 addr)
{
  HOOK_JAL(addr, &poolDestroy);
}

//--------------------------------------------------------------------------
void poolTick(void)
{
  
}

//--------------------------------------------------------------------------
void poolInit(void)
{
  // reset moby table
  memset(poolMobys, 0, sizeof(poolMobys));

  // map each group to a section in the moby table
  // if groups extend beyond moby table, truncate
  int i;
  int idx = 0;
  for (i = 0; i < COUNT_OF(poolGroups); ++i) {
    int size = (int)minf(poolGroups[i].PoolSize, POOL_MAX_COUNT - (poolGroups[i].PoolSize + idx));
    poolGroups[i].PoolStart = idx;
    poolGroups[i].PoolSize = size;

#if DEBUG_POOL
    DPRINTF("POOL GROUP OCLASS:%d FROM:%d-%d\n", poolGroups[i].MobyOClass, idx, size);
#endif

    idx += size;
  }

  // 0x13A1 -- explosion moby
  poolHookSpawn(0x003C3BE4); 
  poolHookDestroy(0x003c45fc);

  // 0x20B8 -- 
  poolHookSpawn(0x0042c1d4);
  poolHookDestroy(0x0042c4e4);
  poolHookDestroy(0x0042c114);

  // 0x0070 -- explosion bubble
  poolHookSpawn(0x003a15e8);
  poolHookDestroy(0x003a17f4);

  // 0x20E2 -- splash
  poolHookSpawn(0x0042ea50); 
  poolHookDestroy(0x0042ec28);

  // 0x1F8C -- 
  //poolHookSpawn(0x0041D464); 
  //poolHookDestroy(0x0041ddf8);
}
