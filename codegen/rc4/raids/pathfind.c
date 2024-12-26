/***************************************************
 * FILENAME :		pathfind.c
 * 
 * DESCRIPTION :
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <libdl/collision.h>
#include "pathfind.h"

#define PATH_EDGE_IS_EMPTY(x)               (x == 255)
#define CLOSEST_NODES_COLL_CHECK_SIZE       (3)

#if GATE
void gateSetCollision(int collActive);
#endif

int mapPathCanBeSkippedForTarget(struct PathGraph* path, Moby* moby);

//--------------------------------------------------------------------------
int pathUseTargetMoby(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars)
{
  return moveVars->Target && !moveVars->ForceUseTargetPosition;
}

//--------------------------------------------------------------------------
struct PathGraph* pathGetMobyPathGraph(Moby* moby, struct MobMoveVars* moveVars)
{
  if (!moby || !moveVars)
    return NULL;

  if (moveVars->PathGraphIdx >= 0 && moveVars->PathGraphIdx < PathsCount)
    return &Paths[(int)moveVars->PathGraphIdx];

  return NULL;
}

//--------------------------------------------------------------------------
void pathGetNodePosition(struct PathGraph* path, int nodeIdx, float preferredHeight, VECTOR output)
{
  vector_write(output, 0);
  if (!path || nodeIdx < 0 || nodeIdx >= path->NumNodes) return;

  vector_copy(output, path->Nodes[nodeIdx]);
  output[3] = 0;
  output[2] += minf(preferredHeight, path->Heights[nodeIdx]);
}

//--------------------------------------------------------------------------
u8* pathGetPathAt(struct PathGraph* path, int fromNodeIdx, int toNodeIdx)
{
  int pathIdx = (fromNodeIdx * path->NumNodes) + toNodeIdx;
  u32 addr = (u32)path->Paths;

  return (u8*)(addr + pathIdx * path->MaxPathNodeCount);
}

//--------------------------------------------------------------------------
u8* pathGetEdge(struct PathGraph* path, u8 edge)
{
  if (!path)
    return NULL;

  if (PATH_EDGE_IS_EMPTY(edge))
    return NULL;

  return (u8*)path->Edges[edge];
}

//--------------------------------------------------------------------------
void pathGetSegment(struct PathGraph* path, u8* currentEdge, VECTOR fromNodePos, VECTOR toNodePos)
{
  if (!currentEdge || !path) return;

  vector_copy(fromNodePos, path->Nodes[currentEdge[0]]);
  vector_copy(toNodePos, path->Nodes[currentEdge[1]]);
  fromNodePos[3] = 0;
  toNodePos[3] = 0;
}

//--------------------------------------------------------------------------
float pathGetSegmentAlpha(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars, u8* currentEdge, float* outHeightLimit)
{
  VECTOR startNodeToEndNode, startNodeToMoby;

  if (!moby || !currentEdge || !path) return 0;

  // get vectors from start node to end node and moby
  vector_subtract(startNodeToEndNode, path->Nodes[currentEdge[0]], path->Nodes[currentEdge[1]]);
  startNodeToEndNode[3] = 0;
  vector_subtract(startNodeToMoby, path->Nodes[currentEdge[0]], moby->Position);
  startNodeToMoby[3] = 0;

  // project startToMoby on startToEnd
  float edgeLen = vector_length(startNodeToEndNode);
  float distOnEdge = vector_length(startNodeToMoby) * vector_innerproduct(startNodeToEndNode, startNodeToMoby);
  float alpha = clamp(distOnEdge / edgeLen, 0, 1);

  if (outHeightLimit) {
    float heightAlpha = alpha = clamp((distOnEdge+path->Nodes[currentEdge[1]][3]) / edgeLen, 0, 1);
    float fromHeight = minf(path->Heights[currentEdge[0]] / 256.0, moveVars->PreferredHeight);
    float toHeight = minf(path->Heights[currentEdge[1]] / 256.0, moveVars->PreferredHeight);
    float limit = lerpf(fromHeight, toHeight, clamp(heightAlpha + 0.25, 0, 1));
    *outHeightLimit = maxf(0, limit - 1); // need to factor in mob height, so that this limit is the ceiling, not the floor of the mob
    //DPRINTF("%d=>%d (%f) -> %f\n", currentEdge[0], currentEdge[1], alpha, limit);
  }
  return alpha;
}

//--------------------------------------------------------------------------
int pathCanStartNodeBeSkipped(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars)
{
  VECTOR mobyToStart, mobyToNext;
  if (!moby || !moveVars || !path)
    return 0;

  int startEdgeIdx = moveVars->CurrentPath[0];
  u8* startEdge = path->Edges[startEdgeIdx];

  // if the start node to next node is a jump
  // AND we're not after that jump
  // then circle back, we failed the jump
  // otherwise allow skipping
  float alpha = pathGetSegmentAlpha(path, moby, moveVars, startEdge, NULL);
  float jumpAt = path->EdgesJumpAt[startEdgeIdx] / 255.0;
  if (path->EdgesJumpSpeed[startEdgeIdx] > 0 && alpha >= jumpAt)
    return 0;

  // if segment is required and we haven't completed the required section then circle back
  float requiredAt = path->EdgesRequired[startEdgeIdx] / 255.0;
  if (requiredAt > 0 && alpha <= requiredAt)
    return 0;

  // skip if start is in opposite direction to next target
  vector_subtract(mobyToStart, path->Nodes[startEdge[0]], moby->Position);
  vector_subtract(mobyToNext, path->Nodes[startEdge[1]], moby->Position);
  return vector_innerproduct_unscaled(mobyToNext, mobyToStart) < 0;
}

//--------------------------------------------------------------------------
int pathSegmentCanBeSkipped(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars, int segmentStartEdgeIdx, int segmentCount, float segmentStartAlpha)
{
  int i, edge;
  if (!moby || !moveVars || !path)
    return 0;

  // no path
  if (!moveVars->PathEdgeCount) {
    return 0;
  }

  // segment not in path
  if (segmentStartEdgeIdx >= moveVars->PathEdgeCount) {
    return 0;
  }

  // check if current edge is required or there is a jump we haven't reached
  edge = moveVars->CurrentPath[segmentStartEdgeIdx];
  float jumpAt = path->EdgesJumpAt[edge] / 255.0;
  float requiredAt = path->EdgesRequired[edge] / 255.0;
  if ((requiredAt > 0 && segmentStartAlpha <= requiredAt) || (path->EdgesJumpSpeed[edge] > 0 && segmentStartAlpha <= jumpAt))
    return 0;

  for (i = 1; i < segmentCount && (i+segmentStartEdgeIdx) < moveVars->PathEdgeCount; ++i) {
    edge = moveVars->CurrentPath[i + segmentStartEdgeIdx];
    if (PATH_EDGE_IS_EMPTY(edge))
      break;

    if (path->EdgesRequired[edge] > 0 || path->EdgesJumpSpeed[edge] > 0)
      return 0;
  }

  return 1;
}

//--------------------------------------------------------------------------
int pathCanBeSkippedForTarget(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars)
{
  int i, edge;
  if (!moby || !moveVars || !path)
    return 1;

  // pass to map
  // let map decide if path can't be skipped
  if (!mapPathCanBeSkippedForTarget(path, moby))
    return 0;

  // no path
  if (!moveVars->PathEdgeCount) {
    return 1;
  }

  // wait for grounding
  if (!moveVars->Grounded) {
    return 0;
  }

  // stuck
  if (moveVars->StuckCounter) {
    return 0;
  }

  // check if current edge is required or there is a jump we haven't reached
  edge = moveVars->CurrentPath[moveVars->PathEdgeCurrent];
  float jumpAt = path->EdgesJumpAt[edge] / 255.0;
  float requiredAt = path->EdgesRequired[edge] / 255.0;
  if ((requiredAt > 0 && moveVars->PathEdgeAlpha <= requiredAt) || (path->EdgesJumpSpeed[edge] > 0 && moveVars->LastPathEdgeAlphaForJump <= jumpAt))
    return 0;

  for (i = moveVars->PathEdgeCurrent+1; i < moveVars->PathEdgeCount; ++i) {
    edge = moveVars->CurrentPath[i];
    if (PATH_EDGE_IS_EMPTY(edge))
      break;

    if (path->EdgesRequired[edge] > 0 || path->EdgesJumpSpeed[edge] > 0) {
#if DEBUGPATH
      DPRINTF("CANNOT SKIP PATH WITH REQUIRED/JUMP %d=>%d\n", path->Edges[edge][0], path->Edges[edge][1]);
#endif
      return 0;
    }
  }

  return 1;
}

//--------------------------------------------------------------------------
int pathGetClosestNode(struct PathGraph* path, Moby* moby)
{
  int i;
  VECTOR position = {0,0,1,0};
  VECTOR delta;
  int closestNodeIdx = 0;
  float closestNodeDist = 10000000;

  // use center of moby
  vector_add(position, position, moby->Position);

  for (i = 0; i < path->NumNodes; ++i) {

    vector_subtract(delta, path->Nodes[i], position);
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
int pathGetClosestNodeInSight(struct PathGraph* path, Moby* moby, int * foundInSight)
{
  int i,j;
  VECTOR position = {0,0,1,0};
  VECTOR delta;

  char orderedNodesByDist[CLOSEST_NODES_COLL_CHECK_SIZE];
  float nodeDists[PATHGRAPH_MAX_NUM_NODES];

  // init
  memset(orderedNodesByDist, -1, sizeof(orderedNodesByDist));
  memset(nodeDists, 0, sizeof(nodeDists));

  // use center of moby
  vector_add(position, position, moby->Position);

  //DPRINTF("get closest node in sight %08X %08X %04X\n", guberGetUID(moby), (u32)moby, moby->OClass);

  // find n closest nodes to moby
  for (i = 0; i < path->NumNodes && i < PATHGRAPH_MAX_NUM_NODES; ++i) {

    vector_subtract(delta, path->Nodes[i], position);
    float radius = delta[3];
    delta[3] = 0;
    float dist = nodeDists[i] = maxf(0, vector_length(delta) - radius);

    // if obstructed then increase distance by factor
    //if (CollLine_Fix(position, path->Nodes[i], COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL))
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
      vector_copy(nodePos, path->Nodes[(u8)orderedNodesByDist[i]]);
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
int pathRegisterTarget(struct PathGraph* path, Moby* moby)
{
  int i;

  if (!path || !moby) return -1;

  for (i = 0; i < TARGETS_CACHE_COUNT; ++i) {
    if (!path->TargetsCache[i].Target) {
      path->TargetsCache[i].Target = moby;
      return path->TargetsCache[i].ClosestNodeIdx = pathGetClosestNodeInSight(path, moby, NULL);
    }
  }

  return -1;
}

//--------------------------------------------------------------------------
int pathGetClosestNodeIdx(struct PathGraph* path, VECTOR pos)
{
  int i;
  VECTOR dt;
  int bestSqrDist = 1000000;
  int bestIdx = 0;

  if (!path || !pos) return -1;

  for (i = 0; i < path->NumNodes; ++i) {
    vector_subtract(dt, pos, path->Nodes[i]);
    dt[3] = 0;

    float sqrDist = vector_sqrmag(dt);
    if (sqrDist < bestSqrDist) {
      bestSqrDist = sqrDist;
      bestIdx = i;
    }
  }

  return bestIdx;
}

//--------------------------------------------------------------------------
int pathTargetCacheGetClosestNodeIdx(struct PathGraph* path, Moby* moby)
{
  int i;

  if (!path || !moby) return -1;

  for (i = 0; i < TARGETS_CACHE_COUNT; ++i) {
    if (path->TargetsCache[i].Target == moby) {
      return path->TargetsCache[i].ClosestNodeIdx;
    }
  }

  return pathRegisterTarget(path, moby);
}

//--------------------------------------------------------------------------
int pathShouldFindNewPath(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars)
{
  if (!moby || !moveVars || !path)
    return 0;

  if (!moveVars->PathEdgeCount) {
    return 1;
  }

  if (moveVars->IsStuck && moveVars->StuckCounter > MOB_MAX_STUCK_COUNTER_FOR_NEW_PATH) {
    return 1;
  }

  int lastEdge = moveVars->CurrentPath[moveVars->PathEdgeCount-1];
  if (PATH_EDGE_IS_EMPTY(lastEdge)) {
    return 1;
  }

  int closestNodeIdxToTarget = 0;
  if (pathUseTargetMoby(path, moby, moveVars)) {
    closestNodeIdxToTarget = pathTargetCacheGetClosestNodeIdx(path, moveVars->Target);
  } else {
    closestNodeIdxToTarget = pathGetClosestNodeIdx(path, moveVars->TargetPosition);
  }

  if (path->Edges[lastEdge][1] != closestNodeIdxToTarget) {
    return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
int pathBuildPath(struct PathGraph* path, int fromNodeIdx, int toNodeIdx, u8* outPath, int maxLength)
{
  // aggregate path by connecting segments from start to end
  int currentNodeIdx = fromNodeIdx;
  int lastCurrentNodeIdx = -1;
  int i = 0;
  int j = 0;
  memset(outPath, -1, maxLength);
  while (currentNodeIdx != toNodeIdx) {
    if (j >= maxLength) break;
    if (lastCurrentNodeIdx == currentNodeIdx) break; // prevent inf loop

    lastCurrentNodeIdx = currentNodeIdx;
    u8* pathSegment = pathGetPathAt(path, currentNodeIdx, toNodeIdx);
    for (i = 0; j < maxLength && i < path->MaxPathNodeCount; ++i) {
      u8 edge = pathSegment[i];
      if (PATH_EDGE_IS_EMPTY(edge))
        break;

      outPath[j++] = edge;
      u8* segment = pathGetEdge(path, edge);
      currentNodeIdx = segment[1];
    }
  }

  return j;
}

//--------------------------------------------------------------------------
void pathSetPath(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars, int fromNodeIdx, int toNodeIdx, int currentOnPath, int hasReachedStart, int hasReachedEnd)
{
  int i;
  if (!moby || !moveVars || !path)
    return;

  int maxLength = sizeof(moveVars->CurrentPath) / sizeof(u8);
  moveVars->PathEdgeCount = pathBuildPath(path, fromNodeIdx, toNodeIdx, moveVars->CurrentPath, maxLength);
  //memcpy(moveVars->CurrentPath, pathGetPathAt(path, fromNodeIdx, toNodeIdx), sizeof(u8) * path->MaxPathNodeCount);
  if (moveVars->PathEdgeCurrent != currentOnPath) {
    moveVars->PathEdgeAlpha = 0;
  }
  
  moveVars->PathEdgeCurrent = currentOnPath;
  moveVars->PathHasReachedStart = hasReachedStart;
  moveVars->PathHasReachedEnd = hasReachedEnd;
  moveVars->PathStartEndNodes[0] = toNodeIdx;
  moveVars->PathStartEndNodes[1] = fromNodeIdx;
  moveVars->PathTicks = 0;
  moveVars->WasStuckTicks = moveVars->IsStuck ? (TPS * 4) : 0;
  
  // update height
  moveVars->CurrentHeightLimit = moveVars->PreferredHeight;
  u8* currentEdge = pathGetCurrentEdge(path, moby, moveVars);
  if (currentEdge) {
    moveVars->PathEdgeAlpha = pathGetSegmentAlpha(path, moby, moveVars, currentEdge, &moveVars->CurrentHeightLimit);
  }

#if DEBUGPATH && DEBUG
  DPRINTF("NEW PATH GENERATED: (%d) for %08X\n", gameGetTime(), (u32)moby);
  DPRINTF("\tFROM NODE %d (skip:%d,%d)\n", fromNodeIdx, moveVars->PathHasReachedStart, moveVars->IsStuck);
  DPRINTF("\tTO NODE %d\n", toNodeIdx);
  DPRINTF("\tNODES: ");
  
  // count path length
  for (i = 0; i < moveVars->PathEdgeCount; ++i) {
    int edgeIdx = moveVars->CurrentPath[i];
    u8 * edge = path->Edges[edgeIdx];
    DPRINTF("%d->%d, ", edge[0], edge[1]);
  }
  DPRINTF("\n");
#endif

}

//--------------------------------------------------------------------------
int pathGetPath(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars)
{
  int i,j;
  int inSight = 0;
  if (!moby || !moveVars || !path)
    return 0;

  // target closest node should be calculated and cached per frame in pathTick
  int closestNodeIdxToTarget = 0;
  if (pathUseTargetMoby(path, moby, moveVars)) {
    closestNodeIdxToTarget = pathTargetCacheGetClosestNodeIdx(path, moveVars->Target);
  } else {
    closestNodeIdxToTarget = pathGetClosestNodeIdx(path, moveVars->TargetPosition);
  }

  // we should reuse both the last node we were at
  // and the node we're currently going towards for this depending on the final path
  int closestNodeIdxToMob = pathGetClosestNodeInSight(path, moby, &inSight);
  //DPRINTF("closest node to mob is %d (insight: %d)\n", closestNodeIdxToMob, inSight);

  int lastEdgeIdx = 255;
  if (moveVars->PathEdgeCount) {
    lastEdgeIdx = moveVars->CurrentPath[moveVars->PathEdgeCurrent];
    if (PATH_EDGE_IS_EMPTY(lastEdgeIdx))
      lastEdgeIdx = moveVars->CurrentPath[moveVars->PathEdgeCurrent - 1];
  }

  // aggregate path by connecting segments from start to end
  int maxLength = sizeof(moveVars->CurrentPath) / sizeof(u8);
  moveVars->PathEdgeCount = pathBuildPath(path, closestNodeIdxToMob, closestNodeIdxToTarget, moveVars->CurrentPath, maxLength);
  moveVars->PathEdgeCurrent = 0;
  moveVars->PathEdgeAlpha = 0;
  moveVars->PathHasReachedStart = 0;
  moveVars->PathHasReachedEnd = 0;
  moveVars->PathStartEndNodes[0] = closestNodeIdxToTarget;
  moveVars->PathStartEndNodes[1] = closestNodeIdxToMob;
  moveVars->WasStuckTicks = moveVars->IsStuck ? (TPS * 4) : 0;
   
  // check if we're on same segment as last
  int isOnSameSegment = 0;
  if (!PATH_EDGE_IS_EMPTY(lastEdgeIdx)) {
    isOnSameSegment = lastEdgeIdx == moveVars->CurrentPath[0];
  }

  // if the target is close, the new path can sometimes cause the mob to circle back to the last node it just reached/left
  // if the new edge is required
  // in that case we check to see if we're already walking along the segment
  // and if we are, skip the required flag
  if (!isOnSameSegment) {
    VECTOR segmentTangent, mobyToStartNode, mobyToEndNode;
    struct PathGraph* pathGraph = pathGetMobyPathGraph(moby, moveVars);
    int edgeIdx = moveVars->CurrentPath[0];
    u8 * edge = pathGraph->Edges[edgeIdx];
    vector_subtract(segmentTangent, pathGraph->Nodes[edge[1]], pathGraph->Nodes[edge[0]]);
    vector_subtract(mobyToStartNode, moby->Position, pathGraph->Nodes[edge[0]]);
    vector_subtract(mobyToEndNode, moby->Position, pathGraph->Nodes[edge[1]]);
    float dot = vector_innerproduct(segmentTangent, mobyToStartNode);
    if (dot > 0 && fabsf(acosf(dot)) < (15 * MATH_DEG2RAD) && vector_innerproduct_unscaled(mobyToEndNode, mobyToStartNode) < 0) {
      isOnSameSegment = 1;
    }
  }

  // skip start if its backwards along path
  // and the segment can be skipped
  // or if we're already on this segment from the last path
  //if (i > 0 && (pathSegmentCanBeSkipped(moby, 0, 1, alpha) || isOnSameSegment)) {
  int canBeSkipped = pathCanStartNodeBeSkipped(path, moby, moveVars);
  //DPRINTF("len:%d onSame:%d canSkip:%d lastEdgeIdx:%d newEdgeIdx:%d\n", i, isOnSameSegment, canBeSkipped, lastEdgeIdx, moveVars->CurrentPath[0]);
  if (i > 0 && !moveVars->IsStuck && (isOnSameSegment || canBeSkipped)) {
    moveVars->PathHasReachedStart = 1;
  }

  // update height
  moveVars->CurrentHeightLimit = moveVars->PreferredHeight;
  u8* currentEdge = pathGetCurrentEdge(path, moby, moveVars);
  if (currentEdge) {
    moveVars->PathEdgeAlpha = pathGetSegmentAlpha(path, moby, moveVars, currentEdge, &moveVars->CurrentHeightLimit);
  }

  // mark mob dirty to send path to others
  // if (pvars->MobVars.Owner == gameGetMyClientId()) {
  //   pvars->MobVars.Dirty = 1;
  // }

#if DEBUGPATH && DEBUG
  DPRINTF("NEW PATH GENERATED: (%d) for %08X\n", gameGetTime(), (u32)moby);
  DPRINTF("\tFROM NODE %d (skip:%d,%d,%d,%d)\n", closestNodeIdxToMob, moveVars->PathHasReachedStart, canBeSkipped, isOnSameSegment, moveVars->IsStuck);
  DPRINTF("\tTO NODE %d\n", closestNodeIdxToTarget);
  DPRINTF("\tNODES: ");
  
  // count path length
  struct PathGraph* pathGraph = pathGetMobyPathGraph(moby, moveVars);
  for (i = 0; i < moveVars->PathEdgeCount; ++i) {
    int edgeIdx = moveVars->CurrentPath[i];
    u8 * edge = pathGraph->Edges[edgeIdx];
    DPRINTF("%d->%d, ", edge[0], edge[1]);
  }
  DPRINTF("\n");
#endif

  return 1;
}

//--------------------------------------------------------------------------
u8* pathGetCurrentEdge(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars)
{
  if (!moby || !moveVars || !path)
    return NULL;

  int edgeIdx = moveVars->CurrentPath[moveVars->PathEdgeCurrent];
  if (PATH_EDGE_IS_EMPTY(edgeIdx))
    return NULL;

  return (u8*)path->Edges[edgeIdx];
}

//--------------------------------------------------------------------------
int pathGetTargetNodeIdx(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars)
{
  if (!moby || !moveVars || !path)
    return -1;

  if (!moveVars->PathEdgeCount) {
    if (!moveVars->PathHasReachedStart) return moveVars->PathStartEndNodes[1];
    if (!moveVars->PathHasReachedEnd) return moveVars->PathStartEndNodes[0];
    return -1;
  }

  int edgeIdx = moveVars->CurrentPath[moveVars->PathEdgeCurrent];
  if (PATH_EDGE_IS_EMPTY(edgeIdx))
    return -1;

  if (moveVars->PathEdgeCurrent == (moveVars->PathEdgeCount-1) && moveVars->PathHasReachedEnd)
    return -1;

  if (moveVars->PathEdgeCurrent == 0 && !moveVars->PathHasReachedStart) {
    return path->Edges[edgeIdx][0];
  }

  return path->Edges[edgeIdx][1];
}

//--------------------------------------------------------------------------
void pathGetClosestPointOnNode(struct PathGraph* path, VECTOR output, VECTOR from, VECTOR target, int currentNodeIdx, int nextEdgeIdx, float collRadius)
{
  VECTOR fit;
  VECTOR fromToCurrentNodeCenter;
  VECTOR currentNodeCenterToNextNode;

  float currentNodeRadius = maxf(0, path->Nodes[currentNodeIdx][3] - collRadius);
  float currentNodeCornering = path->Cornering[currentNodeIdx] / 255.0;
  vector_subtract(fromToCurrentNodeCenter, path->Nodes[currentNodeIdx], from);
  
  if (!PATH_EDGE_IS_EMPTY(nextEdgeIdx)) {

    int nextNodeIdx = path->Edges[nextEdgeIdx][1];
    float pathFit = path->EdgesPathFit[nextEdgeIdx] / 255.0;
    vector_lerp(fit, from, path->Nodes[nextNodeIdx], pathFit);
    vector_subtract(currentNodeCenterToNextNode, path->Nodes[nextNodeIdx], path->Nodes[currentNodeIdx]);
  } else {

    vector_copy(fit, from);
    vector_subtract(currentNodeCenterToNextNode, target, path->Nodes[currentNodeIdx]);
  }

  // compute cornering factor
  // the sharper the corner, the tighter the radius
  currentNodeCenterToNextNode[3] = 0;
  fromToCurrentNodeCenter[3] = 0;
  float dot = fabsf(vector_innerproduct(fromToCurrentNodeCenter, currentNodeCenterToNextNode));
  float cornerRadius = lerpf(currentNodeCornering * currentNodeRadius, currentNodeRadius, powf(dot, 2.0));
  
  // compute point in node radius to target
  vector_subtract(output, path->Nodes[currentNodeIdx], fit);
  vector_projectonhorizontal(output, output);
  output[3] = 0;
  float r = vector_length(output);
  if (r > cornerRadius)
    vector_scale(output, output, cornerRadius / r);
  
  // float dist = vector_length(output);
  vector_subtract(output, path->Nodes[currentNodeIdx], output);
  output[3] = 0;

  //DPRINTF("edgeEmpty:%d dot:%f corner:%f cornerRadius:%f r:%f finalDist:%f\n", PATH_EDGE_IS_EMPTY(nextEdgeIdx), dot, currentNodeCornering, cornerRadius, r, dist);
}

//--------------------------------------------------------------------------
int pathGetTargetPos(struct PathGraph* path, VECTOR output, Moby* moby, struct MobMoveVars* moveVars)
{
  int newPath = 0;
  VECTOR up = {0,0,1,0};
  VECTOR heightOffset;
  VECTOR targetNodePos, delta;
  VECTOR from, to, edgeDir;
  if (!moby || !moveVars)
    return 0;

  vector_write(heightOffset, 0);

  // no path, go straight to target
  if (!path) {
    if (moveVars->Target && !moveVars->ForceUseTargetPosition) {
      vector_copy(output, moveVars->Target->Position);
    } else {
      vector_copy(output, moveVars->TargetPosition);
    }
    //vector_scale(heightOffset, up, moveVars->PreferredHeight);
    vector_add(output, output, heightOffset);
    vector_copy(moveVars->LastTargetPos, output);
    moveVars->CurrentHeightLimit = PATHGRAPH_MAX_HEIGHT_LIMIT;
    return 0;
  }

  int isStuck = (moveVars->IsStuck && moveVars->StuckCounter > 1) || moveVars->WasStuckTicks;

  /*
  // disable pathfinding
  if (moveVars->Target)
  {
    vector_copy(output, moveVars->Target->Position);
    vector_copy(moveVars->LastTargetPos, output);
    return;
  }
  */

  // reuse last calculated
  if (moveVars->PathTicks) {
    vector_copy(output, moveVars->LastTargetPos);
    return 0;
  }

  // delay next getTargetPos until next tick
  moveVars->PathTicks = 1;
  moveVars->PathEdgeAlpha = pathGetSegmentAlpha(path, moby, moveVars, pathGetCurrentEdge(path, moby, moveVars), &moveVars->CurrentHeightLimit);
  //vector_scale(heightOffset, up, minf(moveVars->PreferredHeight, moveVars->CurrentHeightLimit));

  // new path
  if (moveVars->PathNewTicks == 0 && moveVars->IsOwner && pathShouldFindNewPath(path, moby, moveVars)) {
    newPath = pathGetPath(path, moby, moveVars);
    moveVars->PathNewTicks = 255;
  }

  // set default output
  if (pathUseTargetMoby(path, moby, moveVars)) {
    vector_copy(moveVars->TargetPosition, moveVars->Target->Position);
  }
  
  //vector_scale(heightOffset, up, minf(moveVars->PreferredHeight, moveVars->CurrentHeightLimit));
  vector_add(output, moveVars->TargetPosition, heightOffset);

  // no path
  if (!moveVars->PathEdgeCount && (!isStuck || moveVars->PathHasReachedStart)) {
    vector_copy(moveVars->LastTargetPos, output);
    return newPath;
  }

  // check if we can just go straight to the target
  if (!isStuck && moveVars->PathEdgeCount > 0 && !moveVars->PathCheckNearAndSeeTargetTicks && !moveVars->PathHasReachedEnd) {
  
    int lockOntoPlayer = 0;

    // if target is near and in sight
    // skip rest of path and go straight towards target
    vector_subtract(delta, moveVars->TargetPosition, moby->Position);
    vector_add(from, moby->Position, up);
    vector_add(to, moveVars->TargetPosition, up);
    vector_add(to, to, heightOffset);

    // near and can see
    if (vector_sqrmag(delta) < (MOB_TARGET_DIST_IN_SIGHT_IGNORE_PATH*MOB_TARGET_DIST_IN_SIGHT_IGNORE_PATH) && !CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {

      // not in opposite direction of current edge
      u8* currentEdge = pathGetCurrentEdge(path, moby, moveVars);
      if (currentEdge) {
        vector_subtract(edgeDir, path->Nodes[currentEdge[1]], path->Nodes[currentEdge[0]]);
        edgeDir[3] = 0;
        vector_normalize(edgeDir, edgeDir);
        vector_normalize(delta, delta);

        if (vector_innerproduct(edgeDir, delta) > 0.3) {
          lockOntoPlayer = 1;
        }
      }
      
      if (lockOntoPlayer && pathCanBeSkippedForTarget(path, moby, moveVars)) {
        moveVars->PathEdgeCurrent = moveVars->PathEdgeCount;
        //moveVars->CurrentHeightLimit = minf(moveVars->PreferredHeight, path->Heights[moveVars->PathStartEndNodes[0]]/256.0);
      }
    } else if (moveVars->PathEdgeCurrent && moveVars->PathEdgeCurrent == moveVars->PathEdgeCount) {
      newPath = pathGetPath(path, moby, moveVars);
    }

    moveVars->PathCheckNearAndSeeTargetTicks = TPS;
  }

  // check if we've reached the current node
  int targetNodeIdx = pathGetTargetNodeIdx(path, moby, moveVars);
  if (targetNodeIdx >= 0) {
    vector_copy(targetNodePos, path->Nodes[targetNodeIdx]);
    targetNodePos[3] = 0;
    float radius = path->Nodes[targetNodeIdx][3];
    
    vector_subtract(delta, targetNodePos, moby->Position);
    vector_projectonhorizontal(delta, delta);
    float hDist = vector_length(delta);

    vector_subtract(delta, moveVars->LastTargetPos, moby->Position);
    vector_projectonhorizontal(delta, delta);
    float tDist = vector_length(delta);

    // reached target node
    //DPRINTF("r:%f dist:%f 3:%f\n", radius, hDist, delta[3]);
    if (hDist < (radius + 0.5) && tDist < (0.5 + moveVars->CollRadius)) {
      if (moveVars->PathEdgeCurrent == 0 && !moveVars->PathHasReachedStart) {
        moveVars->PathHasReachedStart = 1;
      //} else if (moveVars->PathEdgeCurrent == (moveVars->PathEdgeCount-1) && !moveVars->PathHasReachedEnd) {
      //  moveVars->PathHasReachedEnd = 1;
      } else {
        moveVars->PathEdgeCurrent++;
        moveVars->PathEdgeAlpha = 0;
        moveVars->WasStuckTicks = 0;
        //DPRINTF("hit target nodeIdx %d, new edgeIdx %d\n", targetNodeIdx, moveVars->PathEdgeCurrent);
      }
    }
  }
  
  // skip end if its backwards along path
  // and we can see the target
  if (!isStuck && moveVars->PathEdgeCount > 0 && !moveVars->PathCheckSkipEndTicks && moveVars->PathEdgeCurrent == (moveVars->PathEdgeCount-1)) {
    u8* lastEdge = pathGetCurrentEdge(path, moby, moveVars);
    if (lastEdge && pathCanBeSkippedForTarget(path, moby, moveVars)) {
      VECTOR targetToStart, targetToNext;
      vector_subtract(targetToStart, path->Nodes[lastEdge[0]], moveVars->TargetPosition);
      vector_subtract(targetToNext, path->Nodes[lastEdge[1]], moveVars->TargetPosition);
      if (vector_innerproduct(targetToNext, targetToStart) < 0) {
        VECTOR up = {0,0,1,0};
        VECTOR from, to;
        vector_add(from, up, moby->Position);
        vector_add(to, up, path->Nodes[lastEdge[1]]);
        if (!CollLine_Fix(from, to, COLLISION_FLAG_IGNORE_DYNAMIC, moby, NULL)) {
          moveVars->PathHasReachedEnd = 1;
        }
      }
    }

    moveVars->PathCheckSkipEndTicks = TPS;
  }

  targetNodeIdx = pathGetTargetNodeIdx(path, moby, moveVars);
  if (targetNodeIdx < 0) {
    vector_copy(moveVars->LastTargetPos, output);
    return newPath;
  }

  // lerp height
  //float fromHeight = path->Heights[targetNodeIdx]/256.0;
  //float toHeight = path->Heights[moveVars->CurrentPath[moveVars->PathEdgeCurrent+1]]/256.0;
  //moveVars->CurrentHeightLimit = lerpf(fromHeight, toHeight, moveVars->PathEdgeAlpha);
  //vector_scale(heightOffset, up, minf(moveVars->PreferredHeight, moveVars->CurrentHeightLimit));

  // get point
  pathGetClosestPointOnNode(path, output, moby->Position, moveVars->TargetPosition, targetNodeIdx, moveVars->CurrentPath[moveVars->PathEdgeCurrent+1], moveVars->CollRadius);
  vector_add(output, output, heightOffset);
  vector_copy(moveVars->LastTargetPos, output);
  return newPath;
}

//--------------------------------------------------------------------------
void pathTick(struct PathGraph* path)
{
  int i;
  int hasAlreadyCheckedANode = 0;

  if (path->LastTargetUpdatedIdx < 0)
  {
    memset(path->TargetsCache, 0, sizeof(path->TargetsCache));
    path->LastTargetUpdatedIdx = 0;
  }

  // update target cache
  // since pathGetClosestNodeInSight uses collision to check if a node is in sight
  // and collision testing is very expensive
  // we only do one update per tick
  // and we put a cooldown
  for (i = 0; i < TARGETS_CACHE_COUNT; ++i) {
    int idx = (path->LastTargetUpdatedIdx + i) % TARGETS_CACHE_COUNT;
    struct TargetCache *cache = &path->TargetsCache[idx];

    if (!cache->Target)
      continue;

    if (mobyIsDestroyed(cache->Target)) {
      cache->Target = NULL;
    } else if (cache->DelayNextCheckTicks <= 0 && !hasAlreadyCheckedANode) {
      cache->ClosestNodeIdx = pathGetClosestNodeInSight(path, cache->Target, NULL);
      cache->DelayNextCheckTicks = TPS * 0.2;
      hasAlreadyCheckedANode = 1;
      path->LastTargetUpdatedIdx = idx + 1;
    } else if (cache->DelayNextCheckTicks) {
      cache->DelayNextCheckTicks--;
    }
  }
}

//--------------------------------------------------------------------------
float pathGetJumpSpeed(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars)
{
  if (!moby || !moveVars || !path)
    return 0;

  // no path
  if (!moveVars->PathEdgeCount) {
    return 0;
  }

  // get and check current edge exists
  int edgeIdx = moveVars->CurrentPath[moveVars->PathEdgeCurrent];
  if (PATH_EDGE_IS_EMPTY(edgeIdx)) {
    return 0;
  }

  return path->EdgesJumpSpeed[edgeIdx];
}

//--------------------------------------------------------------------------
int pathShouldJump(struct PathGraph* path, Moby* moby, struct MobMoveVars* moveVars)
{
  if (!moby || !moveVars || !path)
    return 0;

  // no path
  if (!moveVars->PathEdgeCount) {
    return 0;
  }

  // reached end
  if (moveVars->PathHasReachedEnd) {
    return 0;
  }

  u8* currentEdge = pathGetCurrentEdge(path, moby, moveVars);
  if (currentEdge) {

    // check if edge has jump
    int edgeIdx = moveVars->CurrentPath[moveVars->PathEdgeCurrent];
    float jumpSpeed = path->EdgesJumpSpeed[edgeIdx];
    float jumpAt = path->EdgesJumpAt[edgeIdx] / 255.0;
    float lastDistOnEdge = moveVars->LastPathEdgeAlphaForJump;
    
    // get segment alpha if we haven't refreshed the path this tick
    if (!moveVars->PathTicks)
      moveVars->PathEdgeAlpha = pathGetSegmentAlpha(path, moby, moveVars, currentEdge, &moveVars->CurrentHeightLimit);

    // update
    moveVars->LastPathEdgeAlphaForJump = moveVars->PathEdgeAlpha;

    // we've stepped over threshold for when to jump in the last frame
    if (jumpSpeed > 0 && lastDistOnEdge <= jumpAt && moveVars->PathEdgeAlpha > jumpAt) {
#if DEBUGPATH
      DPRINTF("jump %f (%f) speed:%f\n", moveVars->PathEdgeAlpha, jumpAt, jumpSpeed);
#endif
      return 1;
    }
  }

  return 0;
}
