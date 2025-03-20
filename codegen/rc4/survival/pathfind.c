/***************************************************
 * FILENAME :		pathfind.c
 * 
 * DESCRIPTION :
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <libdl/collision.h>
#include "include/pathfind.h"

#define PATH_EDGE_IS_EMPTY(x)               (x == 255)
#define TARGETS_CACHE_COUNT                 (16)
#define CLOSEST_NODES_COLL_CHECK_SIZE       (3)

#if GATE
void gateSetCollision(int collActive);
#endif

int mapPathCanBeSkippedForTarget(Moby* moby);

struct TargetCache
{
  Moby* Target;
  int ClosestNodeIdx;
  int DelayNextCheckTicks;
};

struct TargetCache TargetsCache[TARGETS_CACHE_COUNT];

//--------------------------------------------------------------------------
u8* pathGetPathAt(int fromNodeIdx, int toNodeIdx)
{
  int pathIdx = (fromNodeIdx * MOB_PATHFINDING_NODES_COUNT) + toNodeIdx;
  u32 addr = (u32)MOB_PATHFINDING_PATHS;

  return (u8*)(addr + pathIdx * MOB_PATHFINDING_PATHS_MAX_PATH_LENGTH);
}

//--------------------------------------------------------------------------
void pathGetSegment(u8* currentEdge, VECTOR fromNodePos, VECTOR toNodePos)
{
  if (!currentEdge) return;

  vector_copy(fromNodePos, MOB_PATHFINDING_NODES[currentEdge[0]]);
  vector_copy(toNodePos, MOB_PATHFINDING_NODES[currentEdge[1]]);
  fromNodePos[3] = 0;
  toNodePos[3] = 0;
}

//--------------------------------------------------------------------------
float pathGetSegmentAlpha(Moby* moby, u8* currentEdge)
{
  VECTOR startNodeToEndNode, startNodeToMoby;

  if (!moby || !currentEdge) return 0;

  // get vectors from start node to end node and moby
  vector_subtract(startNodeToEndNode, MOB_PATHFINDING_NODES[currentEdge[0]], MOB_PATHFINDING_NODES[currentEdge[1]]);
  startNodeToEndNode[3] = 0;
  vector_subtract(startNodeToMoby, MOB_PATHFINDING_NODES[currentEdge[0]], moby->Position);
  startNodeToMoby[3] = 0;

  // project startToMoby on startToEnd
  float edgeLen = vector_length(startNodeToEndNode);
  float distOnEdge = vector_length(startNodeToMoby) * vector_innerproduct(startNodeToEndNode, startNodeToMoby);
  return clamp(distOnEdge / edgeLen, 0, 1);
}

//--------------------------------------------------------------------------
int pathCanStartNodeBeSkipped(Moby* moby)
{
  VECTOR mobyToStart, mobyToNext;
  if (!moby || !moby->PVar)
    return 0;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  int startEdgeIdx = pvars->MobVars.MoveVars.CurrentPath[0];
  u8* startEdge = MOB_PATHFINDING_EDGES[startEdgeIdx];

  // if the start node to next node is a jump
  // AND we're not after that jump
  // then circle back, we failed the jump
  // otherwise allow skipping
  float alpha = pathGetSegmentAlpha(moby, startEdge);
  float jumpAt = MOB_PATHFINDING_EDGES_JUMPPADAT[startEdgeIdx] / 255.0;
  if (MOB_PATHFINDING_EDGES_JUMPPADSPEED[startEdgeIdx] > 0 && alpha >= jumpAt)
    return 0;

  // if segment is required and we haven't completed the required section then circle back
  float requiredAt = MOB_PATHFINDING_EDGES_REQUIRED[startEdgeIdx] / 255.0;
  if (requiredAt > 0 && alpha <= requiredAt)
    return 0;

  // skip if start is in opposite direction to next target
  vector_subtract(mobyToStart, MOB_PATHFINDING_NODES[startEdge[0]], moby->Position);
  vector_subtract(mobyToNext, MOB_PATHFINDING_NODES[startEdge[1]], moby->Position);
  return vector_innerproduct_unscaled(mobyToNext, mobyToStart) < 0;
}

//--------------------------------------------------------------------------
int pathSegmentCanBeSkipped(Moby* moby, int segmentStartEdgeIdx, int segmentCount, float segmentStartAlpha)
{
  int i, edge;
  if (!moby || !moby->PVar)
    return 0;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // no path
  if (!pvars->MobVars.MoveVars.PathEdgeCount) {
    return 0;
  }

  // segment not in path
  if (segmentStartEdgeIdx >= pvars->MobVars.MoveVars.PathEdgeCount) {
    return 0;
  }

  // check if current edge is required or there is a jump we haven't reached
  edge = pvars->MobVars.MoveVars.CurrentPath[segmentStartEdgeIdx];
  float jumpAt = MOB_PATHFINDING_EDGES_JUMPPADAT[edge] / 255.0;
  float requiredAt = MOB_PATHFINDING_EDGES_REQUIRED[edge] / 255.0;
  if ((requiredAt > 0 && segmentStartAlpha <= requiredAt) || (MOB_PATHFINDING_EDGES_JUMPPADSPEED[edge] > 0 && segmentStartAlpha <= jumpAt))
    return 0;

  for (i = 1; i < segmentCount && (i+segmentStartEdgeIdx) < pvars->MobVars.MoveVars.PathEdgeCount; ++i) {
    edge = pvars->MobVars.MoveVars.CurrentPath[i + segmentStartEdgeIdx];
    if (PATH_EDGE_IS_EMPTY(edge))
      break;

    if (MOB_PATHFINDING_EDGES_REQUIRED[edge] > 0 || MOB_PATHFINDING_EDGES_JUMPPADSPEED[edge] > 0)
      return 0;
  }

  return 1;
}

//--------------------------------------------------------------------------
int pathCanBeSkippedForTarget(Moby* moby)
{
  int i, edge;
  if (!moby || !moby->PVar)
    return 1;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // pass to map
  // let map decide if path can't be skipped
  if (!mapPathCanBeSkippedForTarget(moby))
    return 0;

  // no path
  if (!pvars->MobVars.MoveVars.PathEdgeCount) {
    return 1;
  }

  // wait for grounding
  if (!pvars->MobVars.MoveVars.Grounded) {
    return 0;
  }

  // stuck
  if (pvars->MobVars.MoveVars.StuckCounter) {
    return 0;
  }

  // check if current edge is required or there is a jump we haven't reached
  edge = pvars->MobVars.MoveVars.CurrentPath[pvars->MobVars.MoveVars.PathEdgeCurrent];
  float jumpAt = MOB_PATHFINDING_EDGES_JUMPPADAT[edge] / 255.0;
  float requiredAt = MOB_PATHFINDING_EDGES_REQUIRED[edge] / 255.0;
  if ((requiredAt > 0 && pvars->MobVars.MoveVars.PathEdgeAlpha <= requiredAt) || (MOB_PATHFINDING_EDGES_JUMPPADSPEED[edge] > 0 && pvars->MobVars.MoveVars.LastPathEdgeAlphaForJump <= jumpAt))
    return 0;

  for (i = pvars->MobVars.MoveVars.PathEdgeCurrent+1; i < pvars->MobVars.MoveVars.PathEdgeCount; ++i) {
    edge = pvars->MobVars.MoveVars.CurrentPath[i];
    if (PATH_EDGE_IS_EMPTY(edge))
      break;

    if (MOB_PATHFINDING_EDGES_REQUIRED[edge] > 0 || MOB_PATHFINDING_EDGES_JUMPPADSPEED[edge] > 0) {
      DPRINTF("CANNOT SKIP PATH WITH JUMP %d=>%d\n", MOB_PATHFINDING_EDGES[edge][0], MOB_PATHFINDING_EDGES[edge][1]);
      return 0;
    }
  }

  return 1;
}

//--------------------------------------------------------------------------
int pathGetClosestNode(Moby* moby)
{
  int i;
  VECTOR position = {0,0,1,0};
  VECTOR delta;
  int closestNodeIdx = 0;
  float closestNodeDist = 10000000;

  // use center of moby
  vector_add(position, position, moby->Position);

  for (i = 0; i < MOB_PATHFINDING_NODES_COUNT; ++i) {

    vector_subtract(delta, MOB_PATHFINDING_NODES[i], position);
    delta[3] = 0;
    float dist = vector_length(delta);

    if (dist < closestNodeDist) {
      closestNodeDist = dist;
      closestNodeIdx = i;
    }
  }

  return closestNodeIdx;
}

//--------------------------------------------------------------------------
int pathGetClosestNodeInSight(Moby* moby, int * foundInSight)
{
  int i,j;
  VECTOR position = {0,0,1,0};
  VECTOR from;
  VECTOR delta;
  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  float collRadius = 0.5;
  if (pvars)
    collRadius = pvars->MobVars.Config.CollRadius;

  char orderedNodesByDist[CLOSEST_NODES_COLL_CHECK_SIZE];
  float nodeDists[MOB_PATHFINDING_NODES_COUNT];

  // init
  memset(orderedNodesByDist, -1, sizeof(orderedNodesByDist));
  memset(nodeDists, 0, sizeof(nodeDists));

  // use center of moby
  vector_add(position, position, moby->Position);

  //DPRINTF("get closest node in sight %08X %08X %04X\n", guberGetUID(moby), (u32)moby, moby->OClass);

  // find n closest nodes to moby
  for (i = 0; i < MOB_PATHFINDING_NODES_COUNT; ++i) {

    vector_subtract(delta, MOB_PATHFINDING_NODES[i], position);
    float radius = delta[3];
    delta[3] = 0;
    float dist = nodeDists[i] = maxf(0, vector_length(delta) - radius);

    // if obstructed then increase distance by factor
    //if (CollLine_Fix(position, MOB_PATHFINDING_NODES[i], COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL))
    //  dist *= 1000;
    
    for (j = 0; j < CLOSEST_NODES_COLL_CHECK_SIZE; ++j) {
      if (orderedNodesByDist[j] < 0 || dist < nodeDists[(u8)orderedNodesByDist[j]]) {
        int leftover = (CLOSEST_NODES_COLL_CHECK_SIZE - 1) - j;
        if (leftover > 0)
          memmove(&orderedNodesByDist[j+1], &orderedNodesByDist[j], sizeof(char) * leftover);
        
        orderedNodesByDist[j] = i;
        break;
      }
    }
  }

  // if more than 1 node found
  // then find the closest of those that is in sight
  if (orderedNodesByDist[1] >= 0) {
#if GATE
    gateSetCollision(0);
#endif
    
    for (i = 0; i < CLOSEST_NODES_COLL_CHECK_SIZE; ++i) {
      VECTOR nodePos;
      vector_copy(nodePos, MOB_PATHFINDING_NODES[(u8)orderedNodesByDist[i]]);
      float radius = nodePos[3];

      // moby to node
      // compute closest point on node to moby (flat)
      vector_subtract(delta, nodePos, position);
      delta[3] = 0; delta[2] = 0;
      vector_normalize(delta, delta);
      vector_scale(delta, delta, radius);
      vector_subtract(nodePos, nodePos, delta);

      // check if closest point is obstructed
      if (orderedNodesByDist[i] >= 0 && !CollLine_Fix(position, nodePos, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {
        if (foundInSight)
          *foundInSight = 1;

        return orderedNodesByDist[i];
      }
    }
  }

  // didn't find in sight
  if (foundInSight)
    *foundInSight = 0;
    
  return orderedNodesByDist[0];
}

//--------------------------------------------------------------------------
int pathRegisterTarget(Moby* moby)
{
  int i;

  for (i = 0; i < TARGETS_CACHE_COUNT; ++i) {
    if (!TargetsCache[i].Target) {
      TargetsCache[i].Target = moby;
      return TargetsCache[i].ClosestNodeIdx = pathGetClosestNodeInSight(moby, NULL);
    }
  }

  return -1;
}

//--------------------------------------------------------------------------
int pathTargetCacheGetClosestNodeIdx(Moby* moby)
{
  int i;

  for (i = 0; i < TARGETS_CACHE_COUNT; ++i) {
    if (TargetsCache[i].Target == moby) {
      return TargetsCache[i].ClosestNodeIdx;
    }
  }

  return pathRegisterTarget(moby);
}

//--------------------------------------------------------------------------
int pathShouldFindNewPath(Moby* moby)
{
  if (!moby || !moby->PVar)
    return 0;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (!pvars->MobVars.Target)
    return 0;

  if (!pvars->MobVars.MoveVars.PathEdgeCount) {
    return 1;
  }

  if (pvars->MobVars.MoveVars.IsStuck && pvars->MobVars.MoveVars.StuckCounter > MOB_MAX_STUCK_COUNTER_FOR_NEW_PATH) {
    return 1;
  }

  int lastEdge = pvars->MobVars.MoveVars.CurrentPath[pvars->MobVars.MoveVars.PathEdgeCount-1];
  if (PATH_EDGE_IS_EMPTY(lastEdge)) {
    return 1;
  }

  int closestNodeIdxToTarget = pathTargetCacheGetClosestNodeIdx(pvars->MobVars.Target);
  if (MOB_PATHFINDING_EDGES[lastEdge][1] != closestNodeIdxToTarget) {
    return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
void pathGetPath(Moby* moby)
{
  int i;
  int inSight = 0;
  if (!moby || !moby->PVar)
    return;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (!pvars->MobVars.Target)
    return;

  // target closest node should be calculated and cached per frame in pathTick
  int closestNodeIdxToTarget = pathTargetCacheGetClosestNodeIdx(pvars->MobVars.Target);

  // we should reuse both the last node we were at
  // and the node we're currently going towards for this depending on the final path
  int closestNodeIdxToMob = pathGetClosestNodeInSight(moby, &inSight);
  //DPRINTF("closest node to mob is %d (insight: %d)\n", closestNodeIdxToMob, inSight);

  int lastEdgeIdx = 255;
  if (pvars->MobVars.MoveVars.PathEdgeCount) {
    lastEdgeIdx = pvars->MobVars.MoveVars.CurrentPath[pvars->MobVars.MoveVars.PathEdgeCurrent];
    if (PATH_EDGE_IS_EMPTY(lastEdgeIdx))
      lastEdgeIdx = pvars->MobVars.MoveVars.CurrentPath[pvars->MobVars.MoveVars.PathEdgeCurrent - 1];
  }

  memcpy(pvars->MobVars.MoveVars.CurrentPath, pathGetPathAt(closestNodeIdxToMob, closestNodeIdxToTarget), sizeof(u8) * MOB_PATHFINDING_PATHS_MAX_PATH_LENGTH);
  pvars->MobVars.MoveVars.PathEdgeCurrent = 0;
  pvars->MobVars.MoveVars.PathEdgeAlpha = 0;
  pvars->MobVars.MoveVars.PathHasReachedStart = 0;
  pvars->MobVars.MoveVars.PathHasReachedEnd = 0;
  pvars->MobVars.MoveVars.PathStartEndNodes[0] = closestNodeIdxToTarget;
  pvars->MobVars.MoveVars.PathStartEndNodes[1] = closestNodeIdxToMob;
  
  // count path length
  for (i = 0; i < MOB_PATHFINDING_PATHS_MAX_PATH_LENGTH; ++i) {
    if (PATH_EDGE_IS_EMPTY(pvars->MobVars.MoveVars.CurrentPath[i]))
      break;
  }
  pvars->MobVars.MoveVars.PathEdgeCount = i;

  // check if we're on same segment as last
  int isOnSameSegment = 0;
  if (!PATH_EDGE_IS_EMPTY(lastEdgeIdx)) {
    isOnSameSegment = lastEdgeIdx == pvars->MobVars.MoveVars.CurrentPath[0];
  }

  // skip start if its backwards along path
  // and the segment can be skipped
  // or if we're already on this segment from the last path
  //if (i > 0 && (pathSegmentCanBeSkipped(moby, 0, 1, alpha) || isOnSameSegment)) {
  int canBeSkipped = pathCanStartNodeBeSkipped(moby);
  if (i > 0 && (isOnSameSegment || canBeSkipped)) {
    pvars->MobVars.MoveVars.PathHasReachedStart = 1;
  }

  // mark mob dirty to send path to others
  if (pvars->MobVars.Owner == gameGetMyClientId()) {
    pvars->MobVars.Dirty = 1;
  }

#if DEBUGPATH
  DPRINTF("NEW PATH GENERATED: (%d)\n", gameGetTime());
  DPRINTF("\tFROM NODE %d (skip:%d,%d,%d)\n", closestNodeIdxToMob, pvars->MobVars.MoveVars.PathHasReachedStart, canBeSkipped, isOnSameSegment);
  DPRINTF("\tTO NODE %d\n", closestNodeIdxToTarget);
  DPRINTF("\tNODES: ");
  
  // count path length
  for (i = 0; i < pvars->MobVars.MoveVars.PathEdgeCount; ++i) {
    int edgeIdx = pvars->MobVars.MoveVars.CurrentPath[i];
    u8 * edge = MOB_PATHFINDING_EDGES[edgeIdx];
    DPRINTF("%d->%d, ", edge[0], edge[1]);
  }
  DPRINTF("\n");
#endif
}

//--------------------------------------------------------------------------
void pathSetPath(Moby* moby, int fromNodeIdx, int toNodeIdx, int currentOnPath, int hasReachedStart, int hasReachedEnd)
{
  int i;
  if (!moby || !moby->PVar)
    return;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  if (!pvars->MobVars.Target)
    return;

  memcpy(pvars->MobVars.MoveVars.CurrentPath, pathGetPathAt(fromNodeIdx, toNodeIdx), sizeof(u8) * MOB_PATHFINDING_PATHS_MAX_PATH_LENGTH);
  if (pvars->MobVars.MoveVars.PathEdgeCurrent != currentOnPath) {
    pvars->MobVars.MoveVars.PathEdgeAlpha = 0;
  }
  pvars->MobVars.MoveVars.PathEdgeCurrent = currentOnPath;
  pvars->MobVars.MoveVars.PathHasReachedStart = hasReachedStart;
  pvars->MobVars.MoveVars.PathHasReachedEnd = hasReachedEnd;
  pvars->MobVars.MoveVars.PathStartEndNodes[0] = toNodeIdx;
  pvars->MobVars.MoveVars.PathStartEndNodes[1] = fromNodeIdx;
  
  // count path length
  for (i = 0; i < MOB_PATHFINDING_PATHS_MAX_PATH_LENGTH; ++i) {
    if (PATH_EDGE_IS_EMPTY(pvars->MobVars.MoveVars.CurrentPath[i]))
      break;
  }
  pvars->MobVars.MoveVars.PathEdgeCount = i;
}

//--------------------------------------------------------------------------
u8* pathGetCurrentEdge(Moby* moby)
{
  if (!moby || !moby->PVar)
    return NULL;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  
  int edgeIdx = pvars->MobVars.MoveVars.CurrentPath[pvars->MobVars.MoveVars.PathEdgeCurrent];
  if (PATH_EDGE_IS_EMPTY(edgeIdx))
    return NULL;

  return (u8*)MOB_PATHFINDING_EDGES[edgeIdx];
}

//--------------------------------------------------------------------------
int pathGetTargetNodeIdx(Moby* moby)
{
  if (!moby || !moby->PVar)
    return -1;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  
  int edgeIdx = pvars->MobVars.MoveVars.CurrentPath[pvars->MobVars.MoveVars.PathEdgeCurrent];
  if (PATH_EDGE_IS_EMPTY(edgeIdx))
    return -1;

  if (pvars->MobVars.MoveVars.PathEdgeCurrent == (pvars->MobVars.MoveVars.PathEdgeCount-1) && pvars->MobVars.MoveVars.PathHasReachedEnd)
    return -1;

  if (pvars->MobVars.MoveVars.PathEdgeCurrent == 0 && !pvars->MobVars.MoveVars.PathHasReachedStart) {
    return MOB_PATHFINDING_EDGES[edgeIdx][0];
  }

  return MOB_PATHFINDING_EDGES[edgeIdx][1];
}

//--------------------------------------------------------------------------
void pathGetClosestPointOnNode(VECTOR output, VECTOR from, VECTOR target, int currentNodeIdx, int nextEdgeIdx, float collRadius)
{
  VECTOR fit;
  VECTOR fromToCurrentNodeCenter;
  VECTOR currentNodeCenterToNextNode;

  float currentNodeRadius = maxf(0, MOB_PATHFINDING_NODES[currentNodeIdx][3] - collRadius);
  float currentNodeCornering = MOB_PATHFINDING_NODES_CORNERING[currentNodeIdx] / 255.0;
  vector_subtract(fromToCurrentNodeCenter, MOB_PATHFINDING_NODES[currentNodeIdx], from);
  
  if (!PATH_EDGE_IS_EMPTY(nextEdgeIdx)) {

    int nextNodeIdx = MOB_PATHFINDING_EDGES[nextEdgeIdx][1];
    float pathFit = MOB_PATHFINDING_EDGES_PATHFIT[nextEdgeIdx] / 255.0;
    vector_lerp(fit, from, MOB_PATHFINDING_NODES[nextNodeIdx], pathFit);
    vector_subtract(currentNodeCenterToNextNode, MOB_PATHFINDING_NODES[nextNodeIdx], MOB_PATHFINDING_NODES[currentNodeIdx]);
  } else {

    vector_copy(fit, from);
    vector_subtract(currentNodeCenterToNextNode, target, MOB_PATHFINDING_NODES[currentNodeIdx]);
  }

  // compute cornering factor
  // the sharper the corner, the tighter the radius
  currentNodeCenterToNextNode[3] = 0;
  fromToCurrentNodeCenter[3] = 0;
  float dot = fabsf(vector_innerproduct(fromToCurrentNodeCenter, currentNodeCenterToNextNode));
  float cornerRadius = lerpf(currentNodeCornering * currentNodeRadius, currentNodeRadius, powf(dot, 2.0));
  
  // compute point in node radius to target
  vector_subtract(output, MOB_PATHFINDING_NODES[currentNodeIdx], fit);
  vector_projectonhorizontal(output, output);
  output[3] = 0;
  float r = vector_length(output);
  if (r > cornerRadius)
    vector_scale(output, output, cornerRadius / r);
  
  // float dist = vector_length(output);
  vector_subtract(output, MOB_PATHFINDING_NODES[currentNodeIdx], output);
  output[3] = 0;

  //DPRINTF("edgeEmpty:%d dot:%f corner:%f cornerRadius:%f r:%f finalDist:%f\n", PATH_EDGE_IS_EMPTY(nextEdgeIdx), dot, currentNodeCornering, cornerRadius, r, dist);
}

//--------------------------------------------------------------------------
void pathGetTargetPos(VECTOR output, Moby* moby)
{
  VECTOR up = {0,0,1,0};
  VECTOR targetNodePos, delta;
  VECTOR from, to, edgeDir;
  if (!moby || !moby->PVar)
    return;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  /*
  // disable pathfinding
  if (pvars->MobVars.Target)
  {
    vector_copy(output, pvars->MobVars.Target->Position);
    vector_copy(pvars->MobVars.MoveVars.LastTargetPos, output);
    return;
  }
  */

  // reuse last calculated
  if (pvars->MobVars.MoveVars.PathTicks) {
    vector_copy(output, pvars->MobVars.MoveVars.LastTargetPos);
    return;
  }

  // delay next getTargetPos until next tick
  pvars->MobVars.MoveVars.PathTicks = 1;
  pvars->MobVars.MoveVars.PathEdgeAlpha = pathGetSegmentAlpha(moby, pathGetCurrentEdge(moby));

  // new path
  if (pvars->MobVars.MoveVars.PathNewTicks == 0 && pathShouldFindNewPath(moby)) {
    pathGetPath(moby);
    pvars->MobVars.MoveVars.PathNewTicks = 255;
  }

  // set default output
  if (pvars->MobVars.Target)
    vector_copy(output, pvars->MobVars.Target->Position);
  else
    vector_copy(output, moby->Position);

  // no path
  if (!pvars->MobVars.MoveVars.PathEdgeCount || !pvars->MobVars.Target) {
    vector_copy(pvars->MobVars.MoveVars.LastTargetPos, output);
    return;
  }

  // check if we can just go straight to the target
  if (!pvars->MobVars.MoveVars.PathCheckNearAndSeeTargetTicks && pvars->MobVars.Target && !pvars->MobVars.MoveVars.PathHasReachedEnd) {
  
    int lockOntoPlayer = 0;

    // if target is near and in sight
    // skip rest of path and go straight towards target
    vector_subtract(delta, pvars->MobVars.Target->Position, moby->Position);
    vector_add(from, moby->Position, up);
    vector_add(to, pvars->MobVars.Target->Position, up);

    // near and can see
    if (vector_sqrmag(delta) < (MOB_TARGET_DIST_IN_SIGHT_IGNORE_PATH*MOB_TARGET_DIST_IN_SIGHT_IGNORE_PATH) && !CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {

      // not in opposite direction of current edge
      u8* currentEdge = pathGetCurrentEdge(moby);
      if (currentEdge) {
        vector_subtract(edgeDir, MOB_PATHFINDING_NODES[currentEdge[1]], MOB_PATHFINDING_NODES[currentEdge[0]]);
        edgeDir[3] = 0;
        vector_normalize(edgeDir, edgeDir);
        vector_normalize(delta, delta);

        if (vector_innerproduct(edgeDir, delta) > 0.3) {
          lockOntoPlayer = 1;
        }
      }
      
      if (lockOntoPlayer && pathCanBeSkippedForTarget(moby)) {
        pvars->MobVars.MoveVars.PathEdgeCurrent = pvars->MobVars.MoveVars.PathEdgeCount;
      }
    } else if (pvars->MobVars.MoveVars.PathEdgeCurrent && pvars->MobVars.MoveVars.PathEdgeCurrent == pvars->MobVars.MoveVars.PathEdgeCount) {
      pathGetPath(moby);
    }

    pvars->MobVars.MoveVars.PathCheckNearAndSeeTargetTicks = TPS;
  }

  // check if we've reached the current node
  int targetNodeIdx = pathGetTargetNodeIdx(moby);
  if (targetNodeIdx >= 0) {
    vector_copy(targetNodePos, MOB_PATHFINDING_NODES[targetNodeIdx]);
    targetNodePos[3] = 0;
    float radius = MOB_PATHFINDING_NODES[targetNodeIdx][3];
    
    vector_subtract(delta, targetNodePos, moby->Position);
    vector_projectonhorizontal(delta, delta);
    float hDist = vector_length(delta);

    vector_subtract(delta, pvars->MobVars.MoveVars.LastTargetPos, moby->Position);
    vector_projectonhorizontal(delta, delta);
    float tDist = vector_length(delta);

    // reached target node
    //DPRINTF("r:%f dist:%f 3:%f\n", radius, hDist, delta[3]);
    if (hDist < (radius + 0.5) && tDist < (0.5 + pvars->MobVars.Config.CollRadius)) {
      if (pvars->MobVars.MoveVars.PathEdgeCurrent == 0 && !pvars->MobVars.MoveVars.PathHasReachedStart) {
        pvars->MobVars.MoveVars.PathHasReachedStart = 1;
      //} else if (pvars->MobVars.MoveVars.PathEdgeCurrent == (pvars->MobVars.MoveVars.PathEdgeCount-1) && !pvars->MobVars.MoveVars.PathHasReachedEnd) {
      //  pvars->MobVars.MoveVars.PathHasReachedEnd = 1;
      } else {
        pvars->MobVars.MoveVars.PathEdgeCurrent++;
        pvars->MobVars.MoveVars.PathEdgeAlpha = 0;
        //DPRINTF("hit target nodeIdx %d, new edgeIdx %d\n", targetNodeIdx, pvars->MobVars.MoveVars.PathEdgeCurrent);
      }
    }
  }
  
  // skip end if its backwards along path
  // and we can see the target
  if (!pvars->MobVars.MoveVars.PathCheckSkipEndTicks && pvars->MobVars.MoveVars.PathEdgeCurrent == (pvars->MobVars.MoveVars.PathEdgeCount-1)) {
    u8* lastEdge = pathGetCurrentEdge(moby);
    if (lastEdge && pathCanBeSkippedForTarget(moby)) {
      VECTOR targetToStart, targetToNext;
      vector_subtract(targetToStart, MOB_PATHFINDING_NODES[lastEdge[0]], pvars->MobVars.Target->Position);
      vector_subtract(targetToNext, MOB_PATHFINDING_NODES[lastEdge[1]], pvars->MobVars.Target->Position);
      if (vector_innerproduct(targetToNext, targetToStart) < 0) {
        VECTOR up = {0,0,1,0};
        VECTOR from, to;
        vector_add(from, up, moby->Position);
        vector_add(to, up, MOB_PATHFINDING_NODES[lastEdge[1]]);
        if (!CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {
          pvars->MobVars.MoveVars.PathHasReachedEnd = 1;
        }
      }
    }

    pvars->MobVars.MoveVars.PathCheckSkipEndTicks = TPS;
  }

  targetNodeIdx = pathGetTargetNodeIdx(moby);
  if (targetNodeIdx < 0) {
    vector_copy(pvars->MobVars.MoveVars.LastTargetPos, output);
    return;
  }

  // get point
  pathGetClosestPointOnNode(output, moby->Position, pvars->MobVars.Target->Position, targetNodeIdx, pvars->MobVars.MoveVars.CurrentPath[pvars->MobVars.MoveVars.PathEdgeCurrent+1], pvars->MobVars.Config.CollRadius);
  vector_copy(pvars->MobVars.MoveVars.LastTargetPos, output);
}

//--------------------------------------------------------------------------
void pathTick(void)
{
  static int initialized = 0;
  static int lastTargetUpdatedIdx = 0;
  int i;
  int hasAlreadyCheckedANode = 0;

  if (!initialized)
  {
    memset(TargetsCache, 0, sizeof(TargetsCache));
    initialized = 1;
  }

  // update target cache
  // since pathGetClosestNodeInSight uses collision to check if a node is in sight
  // and collision testing is very expensive
  // we only do one update per tick
  // and we put a cooldown
  for (i = 0; i < TARGETS_CACHE_COUNT; ++i) {
    int idx = (lastTargetUpdatedIdx + i) % TARGETS_CACHE_COUNT;
    struct TargetCache *cache = &TargetsCache[idx];

    if (!cache->Target)
      continue;

    if (mobyIsDestroyed(cache->Target)) {
      cache->Target = NULL;
    } else if (cache->DelayNextCheckTicks <= 0 && !hasAlreadyCheckedANode) {
      cache->ClosestNodeIdx = pathGetClosestNodeInSight(cache->Target, NULL);
      cache->DelayNextCheckTicks = TPS * 0.2;
      hasAlreadyCheckedANode = 1;
      lastTargetUpdatedIdx = idx + 1;
    } else if (cache->DelayNextCheckTicks) {
      cache->DelayNextCheckTicks--;
    }
  }
}

//--------------------------------------------------------------------------
float pathGetJumpSpeed(Moby* moby)
{
  if (!moby || !moby->PVar)
    return 0;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;

  // no path
  if (!pvars->MobVars.MoveVars.PathEdgeCount || !pvars->MobVars.Target) {
    return 0;
  }

  // get and check current edge exists
  int edgeIdx = pvars->MobVars.MoveVars.CurrentPath[pvars->MobVars.MoveVars.PathEdgeCurrent];
  if (PATH_EDGE_IS_EMPTY(edgeIdx)) {
    return 0;
  }

  return MOB_PATHFINDING_EDGES_JUMPPADSPEED[edgeIdx];
}

//--------------------------------------------------------------------------
int pathShouldJump(Moby* moby)
{
  if (!moby || !moby->PVar)
    return 0;

  struct MobPVar* pvars = (struct MobPVar*)moby->PVar;
  
  // no path
  if (!pvars->MobVars.MoveVars.PathEdgeCount || !pvars->MobVars.Target) {
    return 0;
  }

  // reached end
  if (pvars->MobVars.MoveVars.PathHasReachedEnd) {
    return 0;
  }

  u8* currentEdge = pathGetCurrentEdge(moby);
  if (currentEdge) {

    // check if edge has jump
    int edgeIdx = pvars->MobVars.MoveVars.CurrentPath[pvars->MobVars.MoveVars.PathEdgeCurrent];
    float jumpSpeed = MOB_PATHFINDING_EDGES_JUMPPADSPEED[edgeIdx];
    float jumpAt = MOB_PATHFINDING_EDGES_JUMPPADAT[edgeIdx] / 255.0;
    float lastDistOnEdge = pvars->MobVars.MoveVars.LastPathEdgeAlphaForJump;
    
    // get segment alpha if we haven't refreshed the path this tick
    if (!pvars->MobVars.MoveVars.PathTicks)
      pvars->MobVars.MoveVars.PathEdgeAlpha = pathGetSegmentAlpha(moby, currentEdge);

    // update
    pvars->MobVars.MoveVars.LastPathEdgeAlphaForJump = pvars->MobVars.MoveVars.PathEdgeAlpha;

    // we've stepped over threshold for when to jump in the last frame
    if (jumpSpeed > 0 && lastDistOnEdge <= jumpAt && pvars->MobVars.MoveVars.PathEdgeAlpha > jumpAt) {
#if DEBUGPATH
      DPRINTF("jump %f (%f) speed:%f\n", pvars->MobVars.MoveVars.PathEdgeAlpha, jumpAt, jumpSpeed);
#endif
      return 1;
    }
  }

  return 0;
}
