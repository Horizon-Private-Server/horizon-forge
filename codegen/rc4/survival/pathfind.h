#ifndef SURVIVAL_PATHFIND_H
#define SURVIVAL_PATHFIND_H

#include <tamtypes.h>

#include <libdl/dl.h>
#include <libdl/player.h>
#include <libdl/pad.h>
#include <libdl/time.h>
#include <libdl/net.h>
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/stdio.h>
#include <libdl/gamesettings.h>
#include <libdl/dialog.h>
#include <libdl/sound.h>
#include <libdl/patch.h>
#include <libdl/ui.h>
#include <libdl/graphics.h>
#include <libdl/color.h>
#include <libdl/utils.h>

#include "messageid.h"
#include "game.h"
#include "mob.h"

extern struct SurvivalMapConfig MapConfig;

extern const int MOB_PATHFINDING_NODES_COUNT;
extern const int MOB_PATHFINDING_EDGES_COUNT;
extern VECTOR MOB_PATHFINDING_NODES[];
extern u8 MOB_PATHFINDING_NODES_CORNERING[];
extern u8 MOB_PATHFINDING_EDGES[][2];
extern u8 MOB_PATHFINDING_EDGES_PATHFIT[];
extern u16 MOB_PATHFINDING_EDGES_REQUIRED[];
extern u16 MOB_PATHFINDING_EDGES_JUMPPADSPEED[];
extern u16 MOB_PATHFINDING_EDGES_JUMPPADAT[];
extern const int MOB_PATHFINDING_PATHS_MAX_PATH_LENGTH;
extern u8 MOB_PATHFINDING_PATHS[];

int pathGetClosestNodeIdx(VECTOR pos);
int pathGetClosestNode(Moby* moby);
int pathGetClosestNodeInSight(Moby* moby, int * foundInSight);
int pathTargetCacheGetClosestNodeIdx(Moby* moby);
int pathHasRouteFromTo(int fromNodeIdx, int toNodeIdx);
int pathHasRouteToTarget(Moby* moby, Moby* target);

int pathUseTargetMoby(Moby* moby);
int pathShouldJump(Moby* moby);
float pathGetJumpSpeed(Moby* moby);
int pathGetTargetPos(VECTOR output, Moby* moby);
void pathSetPath(Moby* moby, int fromNodeIdx, int toNodeIdx, int currentOnPath, int hasReachedStart, int hasReachedEnd);

#endif // SURVIVAL_PATHFIND_H
