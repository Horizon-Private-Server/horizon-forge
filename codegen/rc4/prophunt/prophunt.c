#include <libdl/utils.h>
#include <libdl/stdio.h>
#include <libdl/player.h>
#include <libdl/spawnpoint.h>
#include <libdl/math3d.h>
#include <libdl/hud.h>
#include <libdl/ui.h>
#include <libdl/area.h>
#include <libdl/game.h>
#include <libdl/radar.h>
#include <libdl/net.h>
#include <libdl/collision.h>
#include <libdl/moby.h>
#include <libdl/random.h>
#include <libdl/string.h>
#include <libdl/stdio.h>
#include <libdl/stdlib.h>
#include <tamtypes.h>
#include "common.h"
#include "prophunt.h"

struct OctantNode
{
  short FirstIndex;
  short Count;
  short Drawn;
};

struct OctantTree
{
  int Count;
  int MinOX;
  int MinOY;
  struct OctantNode Octants[TREE_MAX_OCTANTS];
};

struct PropInstance
{
  float Position[3];
  float Rotation[3];
  float Scale;
  Moby* PropMoby;
  short PropIndex;
};

struct PlayerPropPVars
{
  struct TargetVars * TargetVarsPtr;
  char _pad0[0x0C];
  struct ReactVars * ReactVarsPtr;
  char _pad1[0x08];
  struct MoveVars_V2 * MoveVarsPtr;
  char _pad2[0x14];
  struct FlashVars * FlashVarsPtr;
  char _pad3[0x14];

  struct TargetVars TargetVars;
};

struct PlayerSetPropMobyClassMessage
{
  VECTOR Offset;
  VECTOR Rotation;
  float Scale;
  int PlayerId;
  int MobyClass;
  int PropIndex;
};

struct PlayerPlaySoundMessage
{
  int PlayerId;
  int SoundDefIndex;
};

struct PlayerPropState
{
  VECTOR PropLastIdleCheckPos;
  VECTOR PropOffset;
  VECTOR PropRotation;
  float PropScale;
  int PropIndex;
  int PropMobyClass;
  Moby* PropMoby;
  int PropIdleFor;
  int PropIdleIter;
  int PropIdleCheckCooldownTicks;
  char Init;
};

struct State
{
  int PropsCount;
  int PropsDrawn;
  float PropFurthestDrawn;
  struct PlayerPropState PlayerStates[GAME_MAX_PLAYERS];
  //struct PropInstance Props[MAX_SPAWN_PROPS];
  struct PropInstance* Props;
  struct OctantTree PropsTree;
} State;

VECTOR PropMobyDefaultRotations[MAX_PROP_CLASSES] = {0};
VECTOR PropMobyDefaultOffsets[MAX_PROP_CLASSES] = {0};
float PropMobyDefaultScales[MAX_PROP_CLASSES] = {0};
short PropMobyClasses[MAX_PROP_CLASSES] = {0};
int PropMobyClassCount = 0;

const short PropSoundDefs[][2] = {
  { MOBY_ID_SHEEP, 6 },
  { MOBY_ID_CHICKEN, 2 },
  { MOBY_ID_DUCK, 1 },
  { MOBY_ID_PIG, 3 },
  { MOBY_ID_PIG, 4 },
};
const int PropSoundDefsCount = COUNT_OF(PropSoundDefs);

// ------------------------------------------------------
int propGetAxisOctant(float axis)
{
  return (int)axis >> TREE_OCTANT_SIZE_BITS;
}

// ------------------------------------------------------
int propGetOctant(struct OctantTree* tree, float* position)
{
  int ox = propGetAxisOctant(position[0]) - tree->MinOX;
  int oy = propGetAxisOctant(position[1]) - tree->MinOY;
  //int oz = propGetAxisOctant(position[2]);
  int octantIndex = (ox & TREE_AXIS_BITMASK) | ((oy & TREE_AXIS_BITMASK) << TREE_BITS_PER_AXIS);
  if (ox > TREE_AXIS_BITMASK || oy > TREE_AXIS_BITMASK) {
    printf("ERROR: propGetOctant() out of bounds ox:%d oy:%d => %d\n", ox, oy, octantIndex);
  }

  return octantIndex;
}

// ------------------------------------------------------
void propGetOctantCenter(struct OctantTree* tree, int octant, float* outPosition)
{
  int ox = (octant & TREE_AXIS_BITMASK) + tree->MinOX;
  int oy = ((octant >> TREE_BITS_PER_AXIS) & TREE_AXIS_BITMASK) + tree->MinOY;

  outPosition[0] = (float)(ox << TREE_OCTANT_SIZE_BITS) + (TREE_OCTANT_SIZE >> 1);
  outPosition[1] = (float)(oy << TREE_OCTANT_SIZE_BITS) + (TREE_OCTANT_SIZE >> 1);
}

// ------------------------------------------------------
int propIsOctantAdjacent(struct OctantTree* tree, float* positionA, float* positionB)
{
  int oxA = propGetAxisOctant(positionA[0]) - tree->MinOX;
  int oyA = propGetAxisOctant(positionA[1]) - tree->MinOY;

  int oxB = propGetAxisOctant(positionB[0]) - tree->MinOX;
  int oyB = propGetAxisOctant(positionB[1]) - tree->MinOY;

  int axisDistance = maxf(fabsf(oxA - oxB), fabsf(oyA - oyB));
  return axisDistance <= 1;
}

// ------------------------------------------------------
int propBuildOctantTree(struct PropInstance* props, int propsCount, struct OctantTree* tree)
{
  int octantCount = 0;
  struct OctantNode* octants = tree->Octants;
  int i;
  int largestOctant = 0;
  int minOX = 1000000, maxOX = 0;
  int minOY = 1000000, maxOY = 0;
  int minOZ = 1000000, maxOZ = 0;

  // first get min and max octants
  for (i = 0; i < propsCount; ++i) {
    struct PropInstance* prop = &props[i];
    int ox = propGetAxisOctant(prop->Position[0]);
    int oy = propGetAxisOctant(prop->Position[1]);
    int oz = propGetAxisOctant(prop->Position[2]);

    if (ox < minOX) minOX = ox;
    if (oy < minOY) minOY = oy;
    if (oz < minOZ) minOZ = oz;
    if (ox > maxOX) maxOX = ox;
    if (oy > maxOY) maxOY = oy;
    if (oz > maxOZ) maxOZ = oz;
  }

  // update min octants in tree
  tree->MinOX = minOX;
  tree->MinOY = minOY;

  // next build octant nodes
  for (i = 0; i < propsCount; ++i) {
    struct PropInstance* prop = &props[i];
    int octantIndex = propGetOctant(tree, prop->Position);

    struct OctantNode* octant = &octants[octantIndex];
    if (octant->Count == 0) {
      octant->FirstIndex = i;
      octantCount++;
      //printf("TREE OCTANT %d FIRST INDEX %d\n", octantIndex, i);
    } else {
      // swap to keep together
      struct PropInstance swap;
      memcpy(&swap, prop, sizeof(struct PropInstance));
      memmove(&props[i], &props[i + 1], (MAX_SPAWN_PROPS - i - 1) * sizeof(struct PropInstance));
      memmove(&props[octant->FirstIndex + octant->Count + 1], &props[octant->FirstIndex + octant->Count], (MAX_SPAWN_PROPS - octant->FirstIndex - octant->Count - 1) * sizeof(struct PropInstance));
      memcpy(&props[octant->FirstIndex + octant->Count], &swap, sizeof(struct PropInstance));

      //printf("TREE OCTANT %d ADD INDEX %d (SWAP %d)\n", octantIndex, octant->FirstIndex + octant->Count, i);

      // shift existing octants up 1 if needed
      int j;
      for (j = 0; j < TREE_MAX_OCTANTS; ++j) {
        if (j != octantIndex) {
          struct OctantNode* otherOctant = &octants[j];
          if (otherOctant->Count > 0 && otherOctant->FirstIndex >= octant->FirstIndex + octant->Count) {
            otherOctant->FirstIndex++;
          }
        }
      }
    }
    octant->Count++;

    // track largest octant
    if (octant->Count > largestOctant)
      largestOctant = octant->Count;
  }

  // validate octants don't overlap
  for (i = 0; i < TREE_MAX_OCTANTS; ++i) {
    struct OctantNode* octantA = &octants[i];
    if (octantA->Count == 0) continue;

    int endA = octantA->FirstIndex + octantA->Count;
    int j;
    for (j = i + 1; j < TREE_MAX_OCTANTS; ++j) {
      struct OctantNode* octantB = &octants[j];
      if (octantB->Count == 0) continue;

      int endB = octantB->FirstIndex + octantB->Count;
      if (!((endA <= octantB->FirstIndex) || (endB <= octantA->FirstIndex))) {
        printf("ERROR: propBuildOctantTree() octant overlap A:%d [%d-%d] B:%d [%d-%d]\n", i, octantA->FirstIndex, endA, j, octantB->FirstIndex, endB);
      }
    }
  }

  // validate octants contain props in their respective octant
  for (i = 0; i < TREE_MAX_OCTANTS; ++i) {
    struct OctantNode* octantA = &octants[i];
    if (octantA->Count == 0) continue;

    int j;
    for (j = 0; j < octantA->Count; ++j) {
      struct PropInstance* prop = &State.Props[octantA->FirstIndex + j];

      int jOctant = propGetOctant(tree, prop->Position);
      if (jOctant != i) {
        printf("ERROR: propBuildOctantTree() octant has bad prop octant:%d prop:%d\n", i, j + octantA->FirstIndex);
      }
    }
  }

#if DEBUG_OCTANTS
  printf("TREE BUILD OCTANT STATS:\n");
  printf("TREE OCTANT SIZE: %dx%d\nAXIS BITMASK: 0x%X\nTOTAL OCTANTS: %d\n", TREE_OCTANT_SIZE, TREE_OCTANT_SIZE, TREE_AXIS_BITMASK, TREE_MAX_OCTANTS);
  printf("TREE FOUND MIN OCTANTS OX:%d to %d, OY:%d to %d, OZ:%d to %d\n", minOX, maxOX, minOY, maxOY, minOZ, maxOZ);
  printf("TREE BUILT WITH %d OCTANTS, LARGEST HAS %d PROPS\n", octantCount, largestOctant);
#endif

  return tree->Count = octantCount;
}

// ------------------------------------------------------
int propGetPropsFromOctant(struct PropInstance* props, int propsCount, struct OctantTree* tree, VECTOR position, struct PropInstance** outProps)
{
  int octantIndex = propGetOctant(tree, position);

  struct OctantNode* octant = &tree->Octants[octantIndex];
  if (octant->Count == 0) {
    *outProps = NULL;
    return 0;
  }

  *outProps = &props[octant->FirstIndex];
  return octant->Count;
}

// ------------------------------------------------------
int propOnPlayerSetPropMobyClassRemote(void* connection, void* data)
{
  struct PlayerSetPropMobyClassMessage msg;
  memcpy(&msg, data, sizeof(msg));

  // update prop
  if (msg.PlayerId >= 0 && msg.PlayerId < GAME_MAX_PLAYERS) {
    struct PlayerPropState* playerState = &State.PlayerStates[msg.PlayerId];
    playerState->PropMobyClass = msg.MobyClass;
    playerState->PropScale = msg.Scale;
    playerState->PropIndex = msg.PropIndex;
    vector_copy(playerState->PropOffset, msg.Offset);
    vector_copy(playerState->PropRotation, msg.Rotation);
  }

  return sizeof(msg);
}

// ------------------------------------------------------
int propOnPlayerPlaySoundRemote(void* connection, void* data)
{
  struct PlayerPlaySoundMessage msg;
  memcpy(&msg, data, sizeof(msg));

  // play sound
  if (msg.PlayerId >= 0 && msg.PlayerId < GAME_MAX_PLAYERS) {
    Player* player = playerGetAll()[msg.PlayerId];
    if (player->PlayerMoby) {
      mobyPlaySoundByClass(PropSoundDefs[msg.SoundDefIndex][1], 0, player->PlayerMoby, PropSoundDefs[msg.SoundDefIndex][0]);
    }
  }

  return sizeof(msg);
}

// ------------------------------------------------------
void propBroadcastPlayerPropMobyClass(int playerId, int propIndex, int mobyClass, VECTOR offset, VECTOR rotation, float scale)
{
  struct PlayerSetPropMobyClassMessage msg = {
    .PlayerId = playerId,
    .MobyClass = mobyClass,
    .PropIndex = propIndex,
    .Scale = scale
  };
  vector_copy(msg.Offset, offset);
  vector_copy(msg.Rotation, rotation);

  void* dmeConnection = netGetDmeServerConnection();
  if (!dmeConnection) return;

  netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, dmeConnection, 150, sizeof(msg), &msg);
}

// ------------------------------------------------------
void propBroadcastPlayerPlayRandomSound(Player* player)
{
  int defIdx = rand(PropSoundDefsCount);
  struct PlayerPlaySoundMessage msg = {
    .PlayerId = player->PlayerId,
    .SoundDefIndex = defIdx
  };

  // player locally
  mobyPlaySoundByClass(PropSoundDefs[defIdx][1], 0, player->PlayerMoby, PropSoundDefs[defIdx][0]);

  // broadcast
  void* dmeConnection = netGetDmeServerConnection();
  if (!dmeConnection) return;

  netBroadcastCustomAppMessage(NET_DELIVERY_CRITICAL, dmeConnection, 151, sizeof(msg), &msg);
}

// ------------------------------------------------------
void propPlayerPropMobyDrawIdle(Moby* moby)
{
  char buf[64];

  Player** players = playerGetAll();
  struct PlayerPropPVars* pvars = (struct PlayerPropPVars*)moby->PVar;
  if (!pvars) return;

  int playerId = moby->Mission;
  if (playerId < 0 || playerId >= GAME_MAX_PLAYERS) return;

  Player* player = players[playerId];
  if (!playerIsValid(player)) return;

  // after awhile start to show
  struct PlayerPropState* playerState = &State.PlayerStates[player->PlayerId];
  int next = maxf(3, PROP_SOUND_PERIOD_SEC - (playerState->PropIdleIter * PROP_SOUND_PERIOD_DEC));
  if (playerState->PropIdleFor > (next - 10)) {
    snprintf(buf, sizeof(buf), "%d", (int)maxf(0, next - playerState->PropIdleFor));
    gfxHelperDrawText(10, SCREEN_HEIGHT - 10, 0, 0, 1, 0x80FFFFFF, buf, -1, TEXT_ALIGN_BOTTOMLEFT, COMMON_DZO_DRAW_NORMAL);
  }

  // play sound
  if (playerState->PropIdleFor > next) {
    propBroadcastPlayerPlayRandomSound(player);
    playerState->PropIdleFor = 0;
    playerState->PropIdleIter++;
  }
}

// ------------------------------------------------------
void propPlayerPropMobyUpdate(Moby* moby)
{
  Player** players = playerGetAll();
  struct PlayerPropPVars* pvars = (struct PlayerPropPVars*)moby->PVar;
  if (!pvars) return;

  int playerId = moby->Mission;
  if (playerId < 0 || playerId >= GAME_MAX_PLAYERS) return;

  Player* player = players[playerId];
  if (!playerIsValid(player)) return;

  moby->ModeBits |= MOBY_MODE_BIT_CAN_BE_DAMAGED;
	MobyColDamage* colDamage = mobyGetDamage(moby, 0xfffffff, 0);
  if (moby->CollDamage != -1 && colDamage) {
    colDamage->Moby = player->PlayerMoby;
    player->PlayerMoby->CollDamage = moby->CollDamage;
    moby->CollDamage = -1;
  }

  if (player->IsLocal) {
	  gfxRegisterDrawFunction((void**)0x0022251C, (void*)&propPlayerPropMobyDrawIdle, moby);
  }
}

// ------------------------------------------------------
void propRandomPropMobyUpdate(Moby* moby)
{
  int idx = radarGetBlipIndex(moby);
  if (idx < 0) return;

  RadarBlip* blip = radarGetBlips() + idx;
  blip->Moby = moby;
  blip->X = moby->Position[0];
  blip->Y = moby->Position[1];
  blip->Life = 30;
  blip->Team = TEAM_WHITE;
  blip->Type = 3;
}

// ------------------------------------------------------
Moby* propSpawnMoby(int mobyClass, int pvarsSize)
{
  Moby* propMoby = mobySpawn(mobyClass, pvarsSize);
  if (!propMoby) return NULL;

  // get default collision ptr
  // if (!propMoby->CollData) {
  //   void* collMobyClassPtr = mobyGetClass(0x2479);
  //   if (collMobyClassPtr) {
  //     propMoby->CollData = *(void**)((u32)collMobyClassPtr + 0x10);
  //     propMoby->CollActive = 0;
  //   }
  // }

  if (propMoby->PClass) {
    propMoby->Bangles = ~*((u16*)((u32)propMoby->PClass + 0x2e)) & 0x1fff; //*((u16*)((u32)propMoby->PClass + 0x2c));
  }

  propMoby->PUpdate = NULL;
  propMoby->Mission = -1;
  propMoby->AnimSpeed = 0;
  propMoby->UpdateDist = 0x40;
  propMoby->DrawDist = PROP_MOBY_DRAW_DIST;
  propMoby->ModeBits = (propMoby->ModeBits & 0x00f0) | 0x0400;
  return propMoby;
}

// ------------------------------------------------------
void propUpdatePlayerModel(Player* player)
{
  struct PlayerPropState* playerState = &State.PlayerStates[player->PlayerId];
  Moby* propMoby = playerState->PropMoby;
  if (!propMoby) return;

  // transform moby offset by moby rotation
  // VECTOR r;
  // vector_subtract(r, playerState->PropRotation, PropMobyDefaultRotations[playerState->PropIndex]);
  // MATRIX m;
  // matrix_unit(m);
  // matrix_rotate_x(m, m, r[0]);
  // matrix_rotate_y(m, m, r[1]);
  // matrix_rotate_z(m, m, r[2]);
  // matrix_multiply(m, player->WorldMatrix, m);
  // vector_apply(propMoby->Position, playerState->PropOffset, m);
  vector_apply(propMoby->Position, playerState->PropOffset, player->WorldMatrix);

  // position, rotate, and scale
  vector_add(propMoby->Position, propMoby->Position, player->PlayerPosition);
  vector_add(propMoby->Rotation, playerState->PropRotation, player->PlayerRotation);
  propMoby->Scale = playerState->PropScale;
  propMoby->CollActive = player->IsLocal ? -1 : 0;
}

// ------------------------------------------------------
void propSetPlayerModel(Player* player, int mobyClass)
{
  struct PlayerPropState* playerState = &State.PlayerStates[player->PlayerId];
  Moby* propMoby = playerState->PropMoby;

  // destroy prop
  if (mobyClass <= 0) {
    if (propMoby) {
      mobyDestroy(propMoby);
      playerState->PropMoby = NULL;
    }
    return;
  }

  // validate oclass
  if (propMoby && propMoby->OClass == mobyClass) return;

  // destroy and recreate
  if (propMoby) {
    mobyDestroy(propMoby);
    playerState->PropMoby = NULL;
  }

  // spawn
  propMoby = propSpawnMoby(mobyClass, sizeof(struct PlayerPropPVars));
  if (!propMoby) {
    printf("ERROR: SPAWN PLAYER %d PROP FAILED\n", player->PlayerId);
    return;
  }

  propMoby->Bolts = playerState->PropIndex;
  propMoby->Mission = player->PlayerId; // player owned prop
  propMoby->PUpdate = &propPlayerPropMobyUpdate;
  propMoby->UpdateDist = -1; // always update
  playerState->PropMoby = propMoby;

#if DEBUG_PLAYER
  printf("SPAWN PLAYER %d PROP %04X => %08X\n", player->PlayerId, mobyClass, propMoby);
#endif
}

// ------------------------------------------------------
void propSetPlayerProp(Player* player, int propIndex, float scale, int broadcast)
{
  struct PlayerPropState* playerState = &State.PlayerStates[player->PlayerId];

  playerState->PropIndex = propIndex;
  playerState->PropMobyClass = PropMobyClasses[propIndex];
  playerState->PropScale = PropMobyDefaultScales[propIndex] * scale; //randRange(PROP_SCALE_RAND_MIN, PROP_SCALE_RAND_MAX);
  vector_scale(playerState->PropOffset, PropMobyDefaultOffsets[propIndex], scale);
  vector_copy(playerState->PropRotation, PropMobyDefaultRotations[propIndex]);
  //playerState->PropRotation[2] = clampAngle(playerState->PropRotation[2] + randRange(-MATH_PI, MATH_PI));

  if (broadcast) {
    propBroadcastPlayerPropMobyClass(player->PlayerId, propIndex, playerState->PropMobyClass, playerState->PropOffset, playerState->PropRotation, playerState->PropScale);
  }

#if DEBUG_PLAYER
  printf("SET PLAYER %d PROP => CLASS:%04X SCALE:%f\n", player->PlayerId, playerState->PropMobyClass, playerState->PropScale);
#endif
}

// ------------------------------------------------------
void propSetPlayerOpacity(Player* player, int opacity)
{
  player->PlayerMoby->Opacity = opacity;
  
  int i;
  for (i = 0; i < 6; ++i) {
    if (player->Gadgets[i].pMoby) {
      player->Gadgets[i].pMoby->Opacity = opacity;
    }
    if (player->Gadgets[i].pMoby2) {
      player->Gadgets[i].pMoby2->Opacity = opacity;
    }
  }
}

// ------------------------------------------------------
void propOnHeroGetGroundInfo(Player* player, int a1)
{
  // disable player prop collision
  if (State.PlayerStates[player->PlayerId].PropMoby) {
    State.PlayerStates[player->PlayerId].PropMoby->CollActive = -1;
  }

  ((void (*)(Player*, int))0x005e5a60)(player, a1);

  // enable player prop collision
  if (State.PlayerStates[player->PlayerId].PropMoby) {
    State.PlayerStates[player->PlayerId].PropMoby->CollActive = 0;
  }
}

// ------------------------------------------------------
void propPlayerSetCollider(Player* player, int playerEnabled, int propEnabled)
{
  static void* pMobyCollData = NULL;
  if (!pMobyCollData) pMobyCollData = player->PlayerMoby->CollData;

  if (playerEnabled) {
    //player->timers.invisible = 0;
    //player->timers.ignoreHeroColl = 0;
    player->timers.collOff = 0;
    player->PlayerMoby->CollData = pMobyCollData;
    player->Coll.idealRadius = 0.45;
  } else {
    //player->timers.invisible = 10;
    //player->timers.ignoreHeroColl = 10;
    player->timers.collOff = 10;
    player->PlayerMoby->CollData = 0;
    player->Coll.idealRadius = 0;
  }

  Moby* propMoby = State.PlayerStates[player->PlayerId].PropMoby;
  if (propMoby) {
    if (propEnabled) {
      propMoby->CollActive = 0;
    } else {
      propMoby->CollActive = -1;
    }
  }
}

// ------------------------------------------------------
void propSetOtherPlayersColliders(Player* player, int playerEnabled, int propEnabled)
{
  Player** players = playerGetAll();
  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* otherPlayer = players[i];
    if (otherPlayer == player || !playerIsValid(otherPlayer)) continue;

    propPlayerSetCollider(otherPlayer, playerEnabled, propEnabled);
  }
}

// ------------------------------------------------------
void propOnUpdateRemoteHero(Player* player)
{
  if (!playerIsValid(player)) return;

  // disable all other player colliders
  // so that this player is never moved by other players
  propSetOtherPlayersColliders(player, 0, 1);
  propPlayerSetCollider(player, 1, 0);

  ((void (*)(Player*))0x00613ed0)(player);
  
  // disable player collider and enable prop
  propPlayerSetCollider(player, 0, 1);
  propSetOtherPlayersColliders(player, 1, 1);
}

//--------------------------------------------------------------------------
void propOnUpdateLocalHeros(void)
{
  void* collData[GAME_MAX_LOCALS] = {0};
  int i;

  // disable local prop collision
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!playerIsValid(player)) continue;

    if (State.PlayerStates[player->PlayerId].PropMoby) {
      collData[i] = State.PlayerStates[player->PlayerId].PropMoby->CollData;
      State.PlayerStates[player->PlayerId].PropMoby->CollData = NULL;
    }
  }

  ((void (*)(void))0x0060ffb8)();
  
  // re-enable local prop collision
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!playerIsValid(player)) continue;
    
    if (State.PlayerStates[player->PlayerId].PropMoby) {
      State.PlayerStates[player->PlayerId].PropMoby->CollData = collData[i];
    }
  }
}

//--------------------------------------------------------------------------
void propOnUpdateAllCameras(int cameraIdx)
{
  void* collData[GAME_MAX_LOCALS] = {0};
  int i;

  // disable local prop collision
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!playerIsValid(player)) continue;

    if (State.PlayerStates[player->PlayerId].PropMoby) {
      collData[i] = State.PlayerStates[player->PlayerId].PropMoby->CollData;
      State.PlayerStates[player->PlayerId].PropMoby->CollData = NULL;
    }
  }

  // camera
  ((void (*)(int))0x004afba8)(cameraIdx);
  
  // re-enable local prop collision
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!playerIsValid(player)) continue;
    
    if (State.PlayerStates[player->PlayerId].PropMoby) {
      State.PlayerStates[player->PlayerId].PropMoby->CollData = collData[i];
    }
  }
}

//--------------------------------------------------------------------------
void propPlayerCheckForIdle(Player* player)
{
  struct PlayerPropState* playerState = &State.PlayerStates[player->PlayerId];
  if (playerState->PropIdleCheckCooldownTicks > 0) {
    --playerState->PropIdleCheckCooldownTicks;
    return;
  }

  float movedDistSqr = vector_sqrdistance(playerState->PropLastIdleCheckPos, player->PlayerPosition);
  if (movedDistSqr > 9) {
    playerState->PropIdleFor = 0;
    playerState->PropIdleIter = 0;
  } else {
    playerState->PropIdleFor++;
  }

  vector_copy(playerState->PropLastIdleCheckPos, player->PlayerPosition);
  playerState->PropIdleCheckCooldownTicks = 60; // 1 second
}

// ------------------------------------------------------
void propProcessRemote(Player* player)
{
  int isSeeker = player->Team == TEAM_SEEKER;
  struct PlayerPropState* playerState = &State.PlayerStates[player->PlayerId];

  // update player
  player->Invisible = !isSeeker;
  propSetPlayerOpacity(player, isSeeker ? 0x80 : 0);
  player->WrenchOnly = !isSeeker;
  // player->timers.ignoreHeroColl = 10;
  // player->PlayerMoby->CollData = 0;
  // player->Coll.idealRadius = 0;

  //hudGetPlayerFlags(player->LocalPlayerIndex)->Flags.Weapons = isSeeker;
  propSetPlayerModel(player, State.PlayerStates[player->PlayerId].PropMobyClass);
  propUpdatePlayerModel(player);
}

// ------------------------------------------------------
void propProcessLocal(Player* player)
{
  int isSeeker = player->Team == TEAM_SEEKER;
  struct PlayerPropState* playerState = &State.PlayerStates[player->PlayerId];

  // set first moby
  if (!playerState->Init && !isSeeker) {
    int propIndex = rand(PropMobyClassCount);
    propSetPlayerProp(player, propIndex, 1, 1);
    playerState->Init = 1;
  }

  // let player navigate props
  if (!isSeeker && !gameIsStartMenuOpen(player->LocalPlayerIndex) && !PATCH_POINTERS_PATCHMENU) {
    int propIndex = playerState->PropIndex;
    float scale = playerState->PropScale / PropMobyDefaultScales[propIndex];
    int change = 0;
    if (padGetButtonDown(player->LocalPlayerIndex, PAD_LEFT) > 0) {
      propIndex = (propIndex - 1 + PropMobyClassCount) % PropMobyClassCount;
      change = 1;
    } else if (padGetButtonDown(player->LocalPlayerIndex, PAD_RIGHT) > 0) {
      propIndex = (propIndex + 1) % PropMobyClassCount;
      change = 1;
    } else if (padGetButtonDown(player->LocalPlayerIndex, PAD_UP) > 0) {
      scale = clamp(scale + 0.1f, PROP_SCALE_RAND_MIN, PROP_SCALE_RAND_MAX);
      change = 1;
    } else if (padGetButtonDown(player->LocalPlayerIndex, PAD_DOWN) > 0) {
      scale = clamp(scale - 0.1f, PROP_SCALE_RAND_MIN, PROP_SCALE_RAND_MAX);
      change = 1;
    }

    // update
    if (change) {
      propSetPlayerProp(player, propIndex, scale, 1);
    }
  } else if (isSeeker && playerState->PropMobyClass > 0) {
    // clear prop if seeker
    propSetPlayerProp(player, -1, 1, 1);
  }

  // set weapons
  if (isSeeker) {
    player->WrenchOnly = 0;
    // if (player->WeaponHeldId != DEFAULT_WEAPON && player->WeaponHeldId != WEAPON_ID_WRENCH) {
    //   //playerStripWeapons(player);
    //   //playerGiveWeapon(player->GadgetBox, 17, 0, 1); // cboots
    //   //playerGiveWeapon(player->GadgetBox, DEFAULT_WEAPON, 1, 1);
    //   playerSetLocalEquipslot(player->LocalPlayerIndex, 0, DEFAULT_WEAPON);
    //   playerEquipWeapon(player, DEFAULT_WEAPON);
    // }
  } else {
    player->WrenchOnly = 1;
    propPlayerCheckForIdle(player);
  }

  // update player
  propSetPlayerOpacity(player, isSeeker ? 0x80 : 0);
  //hudGetPlayerFlags(player->LocalPlayerIndex)->Flags.Weapons = isSeeker;
  propSetPlayerModel(player, State.PlayerStates[player->PlayerId].PropMobyClass);
  propUpdatePlayerModel(player);

  // disable collision
  // if (playerState->PropMoby) {
  //   playerState->PropMoby->CollData = 0;
  // }
}

//--------------------------------------------------------------------------
int propIsCollisionIdWalkable(int collisionId)
{
  collisionId &= 0x0f;
  return collisionId == 0x02 || collisionId == 0x03 || collisionId == 0x07 || collisionId == 0x09 || collisionId == 0x0A || collisionId == 0x0E || collisionId == 0x0F;
}

//--------------------------------------------------------------------------
void propHidePropsInOctant(int octantIndex)
{
  struct OctantNode* octant = &State.PropsTree.Octants[octantIndex];
  
  int i;
  for (i = 0; i < octant->Count; ++i) {
    struct PropInstance* prop = &State.Props[octant->FirstIndex + i];
    Moby* propMoby = prop->PropMoby;

    if (propMoby) {
#if DEBUG_DRAW_PROPS
      printf("destroyed prop moby %08X at %f %f %f\n", propMoby, propMoby->Position[0], propMoby->Position[1], propMoby->Position[2]);
#endif

      mobyDestroy(propMoby);
      prop->PropMoby = NULL;
    }
  }

  octant->Drawn = 0;
}

//--------------------------------------------------------------------------
int propRenderPropsInOctant(int octantIndex)
{
  GameCamera* camera = cameraGetGameCamera(0);
  struct OctantNode* octant = &State.PropsTree.Octants[octantIndex];
  
  octant->Drawn = 0;

  int i;
  for (i = 0; i < octant->Count; ++i) {
    struct PropInstance* prop = &State.Props[octant->FirstIndex + i];
    Moby* propMoby = prop->PropMoby;

    // check if near player
    VECTOR propPosition = {0,0,0,0};
    memcpy(&propPosition, prop->Position, 12);
    float distSqr = vector_sqrdistance(camera->pos, propPosition);
    int propOctant = propGetOctant(&State.PropsTree, propPosition);
    int shouldRender = distSqr < (PROP_MAX_SPAWN_DIST*PROP_MAX_SPAWN_DIST) && State.PropsDrawn < MAX_DRAW_PROPS;

    if (!shouldRender && propMoby) {
#if DEBUG_DRAW_PROPS
      printf("destroyed prop moby %08X at %f %f %f\n", propMoby, propMoby->Position[0], propMoby->Position[1], propMoby->Position[2]);
#endif

      mobyDestroy(propMoby);
      prop->PropMoby = NULL;
    } else if (shouldRender && !propMoby && mobyGetNumSpawnableMobys() > 50) {
        
      propMoby = prop->PropMoby = propSpawnMoby(PropMobyClasses[prop->PropIndex], 0);
      if (!propMoby) continue;

      memcpy(propMoby->Position, prop->Position, 12);
      memcpy(propMoby->Rotation, prop->Rotation, 12);
      propMoby->Scale = prop->Scale;
      propMoby->Bolts = prop->PropIndex;
      //propMoby->PUpdate = &propRandomPropMobyUpdate;
      //propMoby->UpdateDist = -1;
      mobyUpdateTransform(propMoby);

#if DEBUG_DRAW_PROPS
      printf("spawned prop moby %08X at %f %f %f\n", propMoby, propMoby->Position[0], propMoby->Position[1], propMoby->Position[2]);
#endif
    }

    // count
    if (prop->PropMoby) {
      State.PropFurthestDrawn = maxf(State.PropFurthestDrawn, distSqr);
      octant->Drawn++;
      State.PropsDrawn++;
    }
  }

  return octant->Drawn;
}

// ------------------------------------------------------
void propCheckForPropsToRender(void)
{
#define MAX_OCTANTS_TO_DRAW (64)

  int i,j;
  Player** players = playerGetAll();
  VECTOR cameraPos, cameraForward;
  GameCamera* camera = cameraGetGameCamera(0);
  vector_copy(cameraPos, camera->pos);
  vector_fromyaw(cameraForward, camera->rot[2]);
  int cameraOctant = propGetOctant(&State.PropsTree, cameraPos);
  
  // reset counter of how many drawn
  State.PropsDrawn = 0;
  State.PropFurthestDrawn = 0;

  float octantsToDrawDistSqr[MAX_OCTANTS_TO_DRAW];
  int octantsToDraw[MAX_OCTANTS_TO_DRAW];
  int octantsToDrawCount = 0;
  memset(octantsToDrawDistSqr, 0, sizeof(octantsToDrawDistSqr));
  memset(octantsToDraw, 0, sizeof(octantsToDraw));
  u32 octantsToDrawMask[TREE_MAX_OCTANTS / 32] = {0};

  // determine which octants to draw
  // based on camera position and direction
  for (i = 0; i < TREE_MAX_OCTANTS; ++i) {
    struct OctantNode* octant = &State.PropsTree.Octants[i];
    //if (octant->Count == 0) continue;

    // get octant center position
    VECTOR octantPos = { 0, 0, cameraPos[2], 0 };
    propGetOctantCenter(&State.PropsTree, i, octantPos);

    // check if in front of camera
    VECTOR toOctant;
    vector_subtract(toOctant, octantPos, cameraPos);
    vector_normalize(toOctant, toOctant);
    float dot = vector_innerproduct(cameraForward, toOctant);
    float angle = acosf(dot) * (180.0f / MATH_PI);
    float distSqr = vector_sqrdistance(cameraPos, octantPos);
    int isInFront = dot > 0 && distSqr < (PROP_MAX_SPAWN_DIST*PROP_MAX_SPAWN_DIST);

    // check if adjacent octant
    int isAdjacent = propIsOctantAdjacent(&State.PropsTree, cameraPos, octantPos);

    // add to draw list if in front or adjacent
    if (isInFront || isAdjacent) {
      if (octant->Count > 0 && octantsToDrawCount < MAX_OCTANTS_TO_DRAW) {
        octantsToDraw[octantsToDrawCount] = i;
        octantsToDrawDistSqr[octantsToDrawCount] = distSqr;
        octantsToDrawCount++;
      }
      octantsToDrawMask[i/32] |= 1 << (i % 32);
      //propRenderPropsInOctant(i);
      //printf("DRAW OCTANT %d (draw:%d/%d total:%d inFront:%d adjacent:%d angle:%f camera:%d)\n", i, octant->Drawn, octant->Count, State.PropsDrawn, isInFront, isAdjacent, angle, cameraOctant);
    } else if (octant->Drawn) {
      propHidePropsInOctant(i);
    }
  }

  // sort octants by dist
  for (i = 0; i < (octantsToDrawCount - 1); ++i) {
    int swapped = 0;
    for (j = 0; j < (octantsToDrawCount - i - 1); ++j) {
      if (octantsToDrawDistSqr[j] > octantsToDrawDistSqr[j+1]) {
        int swap = octantsToDraw[j+1];
        float swapDist = octantsToDrawDistSqr[j+1];
        octantsToDraw[j+1] = octantsToDraw[j];
        octantsToDraw[j] = swap;
        octantsToDrawDistSqr[j+1] = octantsToDrawDistSqr[j];
        octantsToDrawDistSqr[j] = swapDist;
        swapped = 1;
      }
    }

    if (!swapped) break;
  }

  // hide players
  for (j = 0; j < GAME_MAX_PLAYERS; ++j) {
    Moby* playerPropMoby = State.PlayerStates[j].PropMoby;
    if (!playerPropMoby) continue;
    if (!playerIsValid(players[j])) continue;
    if (players[j]->IsLocal) continue;

    playerPropMoby->DrawDist = 0;
  }

  // draw octants
  for (i = 0; i < octantsToDrawCount; ++i) {
    propRenderPropsInOctant(octantsToDraw[i]);
  }

  // determine which player props to draw based on octants & furthest drawn prop
  for (j = 0; j < GAME_MAX_PLAYERS; ++j) {
    Moby* playerPropMoby = State.PlayerStates[j].PropMoby;
    if (!playerPropMoby) continue;

    int propOctant = propGetOctant(&State.PropsTree, playerPropMoby->Position);
    int drawOctant = (octantsToDrawMask[propOctant / 32] & (1 << (propOctant % 32))) != 0;
    if (!drawOctant) continue;
    
    float distSqr = vector_sqrdistance(playerPropMoby->Position, camera->pos);
    if (distSqr <= State.PropFurthestDrawn) {
      playerPropMoby->DrawDist = PROP_MOBY_DRAW_DIST;
    }
  }

  //printf("%d props drawn (camera octant %d)\n", State.PropsDrawn, cameraOctant);
}

// ------------------------------------------------------
void propRepositionProp(Moby* propMoby)
{
  VECTOR from, to, up;
  vector_scale(up, propMoby->M2_03, 5);
  vector_add(from, propMoby->Position, up);
  vector_subtract(to, propMoby->Position, up);

  // col check
  if (!CollLine_Fix(from, to, 0, propMoby, NULL)) return;

  // set position
  vector_add(propMoby->Position, CollLine_Fix_GetHitPosition(), PropMobyDefaultOffsets[propMoby->Bolts]);
}

// ------------------------------------------------------
int propPositionIsNearPlayerSpawn(VECTOR position)
{
  int spCount = spawnPointGetCount();
  int i;
  for (i = 0; i < spCount; ++i) {
    if (!spawnPointIsPlayer(i)) continue;

    SpawnPoint* sp = spawnPointGet(i);
    if (vector_sqrdistance(position, &sp->M0[12]) < 16) return 1;
  }

  return 0;
}

// ------------------------------------------------------
int propSpawnRandom(int cuboidIdx, int count)
{
  if (cuboidIdx < 0) return 0;

  SpawnPoint* cuboid = spawnPointGet(cuboidIdx);

  // get random point in cuboid
  float rx = randRange(-1, 1);
  float ry = randRange(-1, 1);
  VECTOR bot, top = {rx,ry,1,0};
  vector_copy(bot, top);
  bot[2] = -1;
  vector_apply(top, top, cuboid->M0);
  vector_apply(bot, bot, cuboid->M0);

  int i;
  int totalSpawned = 0;
  for (i = 0; i < count; ++i) {

    // get random point near top/bot
    VECTOR from, to, off;
    vector_scale(off, (VECTOR){randRange(-1, 1),randRange(-1, 1),0,0}, PROP_SPAWN_CLUSTER_SPREAD);
    vector_copy(from, top);
    vector_add(to, bot, off);

#if PROP_SPAWN_CLUSTER_STRAIGHT
    vector_add(from, top, off);
#endif

    // col check
    // need ground
    if (!CollLine_Fix(from, to, 0, NULL, NULL)) continue;

    // skip unwalkable
    int collId = CollLine_Fix_GetHitCollisionId() & 0x0f;
    if (!propIsCollisionIdWalkable(collId)) continue;

    // skip steep surfaces
    // unless magnet wall
    if (collId != 2) {
      VECTOR hitNormal;
      VECTOR up = {0,0,1,0};
      vector_normalize(hitNormal, CollLine_Fix_GetHitNormal());
      float hitNormalDot = vector_innerproduct(hitNormal, up);
      float slope = fabsf(acosf(hitNormalDot)) * MATH_RAD2DEG;
      if (slope > 55) continue;
    }

    // don't spawn on top of player spawn
    if (propPositionIsNearPlayerSpawn(CollLine_Fix_GetHitPosition())) continue;

    // calculate prop parameters
    VECTOR propPosition, propRotation;
    float propScale;
    int propIdx = rand(PropMobyClassCount);
    vector_add(propPosition, CollLine_Fix_GetHitPosition(), PropMobyDefaultOffsets[propIdx]);
    vector_copy(propRotation, PropMobyDefaultRotations[propIdx]);
    propRotation[2] = randRadian();
    propScale = PropMobyDefaultScales[propIdx] * randRange(PROP_SCALE_RAND_MIN, PROP_SCALE_RAND_MAX);

    // align to surface normal
    // if grav wall
    if (collId == 2) {
      VECTOR hitNormal;
      VECTOR forward, right;
      VECTOR up = {0,0,1,0};
      vector_normalize(hitNormal, CollLine_Fix_GetHitNormal());
      vector_fromyaw(forward, propRotation[2]);
      vector_outerproduct(right, forward, hitNormal);
      vector_outerproduct(forward, hitNormal, right);
      MATRIX m;
      matrix_unit(m);
      matrix_fromrows(m, right, forward, hitNormal, (VECTOR){0,0,0,0});
      matrix_toeuler(m, propRotation);
    }

    // adjust from/to positions based on prop size
    // float rad = randRadian();
    // VECTOR offset;
    // vector_fromyaw(offset, rad);
    // //vector_scale(offset, offset, (propMoby->BSphere[3] / 1024.0) * 0.5f);
    // vector_scale(offset, offset, 2.0);
    // vector_add(top, top, offset);
    // vector_add(bot, bot, offset);
    
    // define
    int propId = State.PropsCount++;
    memcpy(State.Props[propId].Position, propPosition, 12);
    memcpy(State.Props[propId].Rotation, propRotation, 12);
    State.Props[propId].Scale = PropMobyDefaultScales[propIdx];
    State.Props[propId].PropIndex = propIdx;
    
    ++totalSpawned;
  }

  return totalSpawned;
}

// ------------------------------------------------------
int propSpawnGrid(int cuboidIdx, int count)
{
  if (cuboidIdx < 0) return 0;

  SpawnPoint* cuboid = spawnPointGet(cuboidIdx);

  int i;
  int totalSpawned = 0;
  for (i = 0; i < count; ++i) {

    int gridSize = (int)sqrtf((float)MAX_SPAWN_PROPS);
    float tX = ((float)(i % gridSize) / (float)(gridSize - 1) - 0.5) * 2;
    float tY = ((float)(i / gridSize) / (float)(gridSize - 1) - 0.5) * 2;

    // get random point near top/bot
    VECTOR spawnPos = {tX, tY, 0, 0};
    vector_apply(spawnPos, spawnPos, cuboid->M0);

    // spawn
    VECTOR propPosition, propRotation;
    float propScale;
    int propIdx = 0; //rand(PropMobyClassCount);

    vector_add(propPosition, spawnPos, PropMobyDefaultOffsets[propIdx]);
    vector_copy(propRotation, PropMobyDefaultRotations[propIdx]);
    propScale = PropMobyDefaultScales[propIdx];

    // define
    int propId = State.PropsCount++;
    memcpy(State.Props[propId].Position, propPosition, 12);
    memcpy(State.Props[propId].Rotation, propRotation, 12);
    State.Props[propId].Scale = PropMobyDefaultScales[propIdx];
    State.Props[propId].PropIndex = propIdx;
    
    ++totalSpawned;
  }

  return totalSpawned;
}

// ------------------------------------------------------
void propReadMapPropList(void)
{
  Moby* m = mobyListGetStart();
  Moby* mEnd = mobyListGetEnd();
  while (m < mEnd) {
    if (m->Group == PROP_MOBY_GROUP_ID) {
      int idx = PropMobyClassCount++;
      PropMobyClasses[idx] = m->OClass;
      PropMobyDefaultScales[idx] = m->Scale;
      vector_copy(PropMobyDefaultOffsets[idx], m->Position);
      vector_copy(PropMobyDefaultRotations[idx], m->Rotation);
      
#if DEBUG_FIND_PROPS
      printf("[%d] FOUND PROP %04X (COLLDATA %08X)\n", idx, m->OClass, m->CollData);
#endif

      // destroy moby after reading
      mobyDestroy(m);
    }
    ++m;
  }
}

// ------------------------------------------------------
void propForceWeapons(void)
{
  // disable autospawn weapons
  GameSettings* gs = gameGetSettings();
  GameOptions* go = gameGetOptions();
  go->GameFlags.MultiplayerGameFlags.AutospawnWeapons = 0;
  go->GameFlags.MultiplayerGameFlags.UnlimitedAmmo = 0;
  go->WeaponFlags.DualVipers = 0;
  go->WeaponFlags.MagmaCannon = 1;
  go->WeaponFlags.Arbiter = 0;
  go->WeaponFlags.FusionRifle = 0;
  go->WeaponFlags.B6 = 1;
  go->WeaponFlags.MineLauncher = 0;
  go->WeaponFlags.Holoshield = 0;
  go->WeaponFlags.Flail = 0;

}

// ------------------------------------------------------
void propCleanup()
{
  // free
  if (State.Props) {
    free(State.Props);
    State.Props = NULL;
    return;
  }
}

// ------------------------------------------------------
void propInit()
{
  // reset state
  memset(&State, 0, sizeof(State));

  // alloc props buffer
  if (!State.Props) {
    State.Props = malloc(MAX_SPAWN_PROPS * sizeof(struct PropInstance));
    if (!State.Props) {
      printf("ERROR: PROP ALLOC FAILED\n");
    }
  }
  
  // force weapons/rules
  propForceWeapons();

  // read available prop mobys on map
  propReadMapPropList();

  // spawn props
  Area_t area;
  GameSettings* gs = gameGetSettings();
  randSeed(gs->GameLoadStartTime);
  if (State.Props && areaGetArea(0, &area) && area.CuboidCount > 0) {
    
    // reset props buffer
    memset(State.Props, 0, MAX_SPAWN_PROPS * sizeof(struct PropInstance));

    // spawn random props
    int count = 0;
    while (count < MAX_SPAWN_PROPS) {
      int clusterSize = (int)minf(randRangeInt(PROP_SPAWN_CLUSTER_MIN, PROP_SPAWN_CLUSTER_MAX), MAX_SPAWN_PROPS - count);
      count += propSpawnRandom(area.Cuboids[rand(area.CuboidCount)], clusterSize);
    }
    //propSpawnGrid(area.Cuboids[rand(area.CuboidCount)], MAX_SPAWN_PROPS);

    // build tree
    propBuildOctantTree(State.Props, State.PropsCount, &State.PropsTree);
  }

  // reset weapons
  int i;
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!playerIsValid(player)) continue;
    
    //playerRespawn(player);
    //playerStripWeapons(player);
    //playerGiveWeapon(player->GadgetBox, 17, 0, 1); // cboots
    int j;
    for (j = WEAPON_SLOT_VIPERS; j < WEAPON_SLOT_COUNT; ++j) {
      int weaponId = weaponSlotToId(j);
      if (player->GadgetBox->Gadgets[weaponId].Level >= 0) {
        player->GadgetBox->Gadgets[weaponId].Level = -1;
      }
    }

    playerSetLocalEquipslot(i, 0, WEAPON_ID_MAGMA_CANNON);
    playerSetLocalEquipslot(i, 1, WEAPON_ID_B6);
    playerSetLocalEquipslot(i, 2, WEAPON_ID_EMPTY);
    playerEquipWeapon(player, WEAPON_ID_MAGMA_CANNON);
  }

  // disable targeting players
  //POKE_U32(0x005F8A80, 0x10A20002);
  //POKE_U32(0x005F8A84, 0x0000102D);
  //POKE_U32(0x005F8A88, 0x24440001);

  // patch props pushing you into walls and killing you
  // POKE_U32(0x005e4188, 0);
  // POKE_U32(0x005e419c, 0);
  // POKE_U32(0x005e41bc, 0);

  // hook get ground info to disable prop collision
  HOOK_JAL(0x005E7640, &propOnHeroGetGroundInfo);
  HOOK_JAL(0x005ce320, &propOnUpdateLocalHeros);
  HOOK_JAL(0x005ce38c, &propOnUpdateRemoteHero);

  // hook camera update to prevent collision with prop
  HOOK_JAL(0x004B2294, &propOnUpdateAllCameras);

  // disable viper reticle/targeting
  POKE_U32(0x004C36B4, 0);

  // disable hero coll
  //POKE_U32(0x005F1568, 0);
  POKE_U32(0x005E3678, 0);

  // force new player sync
  if (PATCH_INTEROP && PATCH_INTEROP->GameConfig) {
    PATCH_INTEROP->GameConfig->grNewPlayerSync = 1;
    PATCH_INTEROP->GameConfig->grNoPacks = 1;
  }

  // hook net messages
  netInstallCustomMsgHandler(150, &propOnPlayerSetPropMobyClassRemote);
  netInstallCustomMsgHandler(151, &propOnPlayerPlaySoundRemote);
}

// ------------------------------------------------------
void propTick()
{
  int i;
  Player** players = playerGetAll();

  // need props
  if (!State.Props) return;

  // force weapons/rules
  propForceWeapons();

  // update which props to render
  propCheckForPropsToRender();

  // check all players
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!playerIsValid(player)) continue;

    if (player->IsLocal) propProcessLocal(player);
    else propProcessRemote(player);
  }
}

// todo
/*
++have player props also pop in and out with propCheckForPropsToRender()
++add player idle sounds
add player hit sounds?
move code to .code file?
fix other players falling constantly on local screen
++add dzo support

add help message on start (with dpad controls)
disable unlimited ammo?

---
lower music automatically
spectate turns on walk thru walls
fix negative/looping respawn timer
fix hidden skins on ps2
add more player spawns
*/
