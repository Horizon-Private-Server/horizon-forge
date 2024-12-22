#ifndef RAIDS_PATHFIND_H
#define RAIDS_PATHFIND_H

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

#define PATHGRAPH_MAX_NUM_NODES             (100)
#define PATHGRAPH_MAX_HEIGHT_LIMIT          (256)
#define TARGETS_CACHE_COUNT                 (16)

struct TargetCache
{
  Moby* Target;
  int ClosestNodeIdx;
  int DelayNextCheckTicks;
};

typedef u8 PathGraphEdge[2];

struct PathGraph
{
  int NumNodes;
	int NumEdges;
	int MaxPathNodeCount;
	VECTOR* Nodes;
	u8* Cornering;
  u16* Heights;
	PathGraphEdge* Edges;
	u8* EdgesRequired;
	u8* EdgesPathFit;
	u8* EdgesJumpSpeed;
	u8* EdgesJumpAt;
	u8* Paths;
  struct TargetCache TargetsCache[TARGETS_CACHE_COUNT];
  int LastTargetUpdatedIdx;
};

extern struct RaidsMapConfig MapConfig;
extern struct PathGraph Paths[];
extern const int PathsCount;

void pathTick(struct PathGraph* path);
int pathUseTargetMoby(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars);
int pathShouldJump(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars);
float pathGetJumpSpeed(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars);
void pathGetNodePosition(struct PathGraph* path, int nodeIdx, float preferredHeight, VECTOR output);
int pathGetTargetPos(struct PathGraph* path, VECTOR output, Moby* moby, struct MobMoveVars* moveVars);
void pathSetPath(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars, int fromNodeIdx, int toNodeIdx, int currentOnPath, int hasReachedStart, int hasReachedEnd);
struct PathGraph* pathGetMobyPathGraph(Moby* moby, struct MobMoveVars* moveVars);

#endif // RAIDS_PATHFIND_H
