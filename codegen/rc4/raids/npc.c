/***************************************************
 * FILENAME :		npc.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the NPC controller.
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/graphics.h>
#include <libdl/moby.h>
#include <libdl/random.h>
#include <libdl/sha1.h>
#include <libdl/radar.h>
#include <libdl/color.h>
#include <libdl/spawnpoint.h>

#include "game.h"
#include "mob.h"
#include "npc.h"
#include "pathfind.h"
#include "spawner.h"
#include "maputils.h"
#include "shared.h"

#define DLOG(moby, format, ...) if (npcGetPVars(moby)->Parameters.Log) { DPRINTF(format, ##__VA_ARGS__); }

void npcPreUpdate(Moby* moby);
void npcPostUpdate(Moby* moby);
void npcPostDraw(Moby* moby);
void npcMove(Moby* moby);
void npcOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e);
void npcOnDestroy(Moby* moby, int killedByPlayerId, int weaponId);
void npcOnDamage(Moby* moby, struct MobDamageEventArgs* e);
int npcOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e);
void npcOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e);
Moby* npcGetNextTarget(Moby* moby);
int npcGetPreferredAction(Moby* moby, int * delayTicks);
void npcDoAction(Moby* moby);
void npcDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire);
void npcForceLocalAction(Moby* moby, int action);
short npcGetArmor(Moby* moby);
int npcIsAttacking(Moby* moby);
int npcCanNonOwnerTransitionToAction(Moby* moby, int action);
int npcShouldForceStateUpdateOnAction(Moby* moby, int action);

int npcIsRoaming(struct NpcPVar* pvars);
int npcIsIdling(struct NpcPVar* pvars);

struct MobVTable NpcVTable = {
  .PreUpdate = &npcPreUpdate,
  .PostUpdate = &npcPostUpdate,
  .PostDraw = &npcPostDraw,
  .Move = &npcMove,
  .OnSpawn = &npcOnSpawn,
  .OnDestroy = &npcOnDestroy,
  .OnDamage = &npcOnDamage,
  .OnLocalDamage = &npcOnLocalDamage,
  .OnStateUpdate = &npcOnStateUpdate,
  .GetNextTarget = &npcGetNextTarget,
  .GetPreferredAction = &npcGetPreferredAction,
  .ForceLocalAction = &npcForceLocalAction,
  .DoAction = &npcDoAction,
  .DoDamage = &npcDoDamage,
  .GetArmor = &npcGetArmor,
  .IsAttacking = &npcIsAttacking,
  .CanNonOwnerTransitionToAction = &npcCanNonOwnerTransitionToAction,
  .ShouldForceStateUpdateOnAction = &npcShouldForceStateUpdateOnAction,
};

//--------------------------------------------------------------------------
struct NpcPVar* npcGetPVars(Moby* moby)
{
  return (struct NpcPVar*)(moby->PVar - OFFSET_OF(struct NpcPVar, Mob));
}

//--------------------------------------------------------------------------
void npcTransAnimLerp(Moby* moby, int animId, int lerpFrames, float startOff)
{
  struct NpcPVar* pvars = npcGetPVars(moby);
  Moby* targetMoby = pvars->Parameters.NpcMoby;
	if (targetMoby->AnimSeqId != animId) {
		mobyAnimTransition(targetMoby, animId, lerpFrames, startOff);

		pvars->Mob.MobVars.AnimationReset = 1;
		pvars->Mob.MobVars.AnimationLooped = 0;
	} else {
		
		// get current t
		// if our stored start is uninitialized, then set current t as start
		float t = targetMoby->AnimSeqT;
		float end = *(u8*)((u32)targetMoby->AnimSeq + 0x10) - (float)(lerpFrames * 0.5);
		if (t >= end && pvars->Mob.MobVars.AnimationReset) {
			pvars->Mob.MobVars.AnimationLooped++;
			pvars->Mob.MobVars.AnimationReset = 0;
		} else if (t < end) {
			pvars->Mob.MobVars.AnimationReset = 1;
		}
	}
}

//--------------------------------------------------------------------------
void npcTransAnim(Moby* moby, int animId, float startOff)
{
	npcTransAnimLerp(moby, animId, 10, startOff);
}

//--------------------------------------------------------------------------
int npcGetCurrentAnimId(Moby* moby)
{
  struct NpcPVar* pvars = npcGetPVars(moby);
  return pvars->Parameters.NpcMoby->AnimSeqId;
}

//--------------------------------------------------------------------------
void npcPreUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct NpcPVar* pvars = npcGetPVars(moby);

  // decrement path target pos ticker
  decTimerU8(&pvars->Mob.MobVars.MoveVars.PathTicks);
  decTimerU8(&pvars->Mob.MobVars.MoveVars.PathCheckNearAndSeeTargetTicks);
  decTimerU8(&pvars->Mob.MobVars.MoveVars.PathCheckSkipEndTicks);
  decTimerU8(&pvars->Mob.MobVars.MoveVars.PathNewTicks);
  pvars->Mob.MobVars.ScoutCooldownTicks = 0;

  mobPreUpdate(moby);
}

//--------------------------------------------------------------------------
void npcPostUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  struct NpcPVar* pvars = npcGetPVars(moby);
  Moby* targetMoby = pvars->Parameters.NpcMoby;

  // reset to start
  if (pvars->Mob.MobVars.Respawn && gameAmIHost()) {
    vector_copy(moby->Position, pvars->Parameters.SpawnPosition);
    vector_copy(pvars->Mob.MobVars.MoveVars.NextPosition, pvars->Parameters.SpawnPosition);
    vector_copy(pvars->Mob.MobVars.MoveVars.LastPosition, pvars->Parameters.SpawnPosition);
    vector_write(pvars->Mob.MobVars.MoveVars.Velocity, 0);
    pvars->Mob.MobVars.Respawn = 0;
    pvars->Mob.MobVars.Dirty = 1;
    pvars->Mob.MobVars.MoveVars.IsStuck = 0;
    pvars->Mob.MobVars.MoveVars.StuckCounter = 0;
  }

  // adjust animSpeed by speed and by animation
  float baseAnimSpeed = pvars->Parameters.AnimSpeed;
	float animSpeed = baseAnimSpeed * pvars->Parameters.WalkAnim.Speed;
  int curAnimId = npcGetCurrentAnimId(moby);
  if (pvars->Mob.MobVars.Action == NPC_ACTION_JUMP && curAnimId == pvars->Parameters.JumpAnim.Id) {
    animSpeed = baseAnimSpeed * pvars->Parameters.JumpAnim.Speed * (1 - powf(moby->AnimSeqT / pvars->Parameters.JumpAnim.Length, 2));
    if (pvars->Mob.MobVars.MoveVars.Grounded) {
      animSpeed = baseAnimSpeed;
    }
  } else if (npcIsIdling(pvars)) {
    animSpeed = baseAnimSpeed * pvars->Parameters.IdleAnim.Speed;
  }

  targetMoby->AnimSpeed = animSpeed;
  
  // move to target
  vector_copy(targetMoby->Position, moby->Position);
  vector_copy(targetMoby->Rotation, moby->Rotation);

  // update attached moby
  if (pvars->Parameters.AttachedMoby && !mobyIsDestroyed(pvars->Parameters.AttachedMoby)) {
    vector_add(pvars->Parameters.AttachedMoby->Position, moby->Position, pvars->Parameters.AttachedMobyOffset);
    pvars->Parameters.AttachedMoby->Rotation[2] = moby->Rotation[2] + pvars->Parameters.AttachedMobyYawOffset;
  } else if (pvars->Parameters.AttachedMoby) {
    pvars->Parameters.AttachedMoby = NULL;
  }

  // update attached cuboid
  if (pvars->Parameters.AttachedCuboidIdx >= 0) {
    SpawnPoint* sp = spawnPointGet(pvars->Parameters.AttachedCuboidIdx);
    vector_add(&sp->M0[12], moby->Position, pvars->Parameters.AttachedCuboidOffset);
  }
}

//--------------------------------------------------------------------------
void npcPostDraw(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;
    
  //struct NpcPVar* pvars = npcGetPVars(moby);
  //u32 color = NPC_LOD_COLOR | (moby->Opacity << 24);
  //mobPostDrawQuad(moby, 127, color, 0);
}

//--------------------------------------------------------------------------
void npcMove(Moby* moby)
{
  if (moby->State == NPC_STATE_OFF) return;
  
	struct NpcPVar* pvars = npcGetPVars(moby);
  if (pvars->Parameters.Speed == 0) return;

  mobMove(moby);
}

//--------------------------------------------------------------------------
void npcOnSpawn(Moby* moby, VECTOR position, float yaw, u32 spawnFromUID, char random, struct MobSpawnEventArgs* e)
{
	struct NpcPVar* pvars = npcGetPVars(moby);

  // targeting
	pvars->Mob.TargetVars.targetHeight = 1 + (moby->Scale * 0.25);
	pvars->Mob.TargetVars.team = pvars->Parameters.Team;
  pvars->Mob.MobVars.BlipType = pvars->Parameters.BlipType;
  pvars->Mob.MobVars.BlipTeam = pvars->Parameters.Team;

  // default move step
  pvars->Mob.MobVars.MoveVars.MoveStep = MOB_MOVE_SKIP_TICKS;
  vector_copy(pvars->Mob.MobVars.MoveVars.TargetPosition, moby->Position);
}

//--------------------------------------------------------------------------
void npcOnDestroy(Moby* moby, int killedByPlayerId, int weaponId)
{
  if (!moby || !moby->PVar)
    return;
}

//--------------------------------------------------------------------------
void npcOnDamage(Moby* moby, struct MobDamageEventArgs* e)
{
  struct NpcPVar* pvars = npcGetPVars(moby);
	float damage = e->DamageQuarters / 4.0;
  float newHp = pvars->Mob.MobVars.Health - damage;

	int canFlinch = pvars->Mob.MobVars.FlinchCooldownTicks == 0;

#if ALWAYS_FLINCH
  canFlinch = 1;
#endif

  //int isShock = e->DamageFlags & 0x40;
  int isShortFreeze = e->DamageFlags & 0x40000000;

	// destroy
	if (newHp <= 0) {
    npcForceLocalAction(moby, NPC_ACTION_DIE);
	}

	// knockback
  // npcs always have knockback
  if (e->Knockback.Power < 3) e->Knockback.Power = 3;
	if (e->Knockback.Power > 0 && (canFlinch || e->Knockback.Force))
	{
		memcpy(&pvars->Mob.MobVars.Knockback, &e->Knockback, sizeof(struct Knockback));
	}

  // flinch
	if (mobAmIOwner(moby))
	{
		//float damageRatio = damage / pvars->Mob.MobVars.Config.Health;
    //float powerFactor = NPC_FLINCH_PROBABILITY_PWR_FACTOR * e->Knockback.Power;

    // auto aggro
    if (!pvars->Mob.MobVars.MoveVars.Target) {
      Guber * guber = guberGetObjectByUID(e->SourceUID);
      Moby* moby = guber ? guber->VTable->GetMoby(guber) : NULL;
      Player* target = mobyGetPlayer(moby);
      if (target) {
        Moby* targetMoby = playerGetTargetMoby(target);
        if (targetMoby) {
          pvars->Mob.MobVars.MoveVars.Target = targetMoby;
          pvars->Mob.MobVars.Dirty = 1;
        }
      }
    }
	}

  // short freeze
  if (isShortFreeze && pvars->Mob.MobVars.SlowTicks < MOB_SHORT_FREEZE_DURATION_TICKS) {
    pvars->Mob.MobVars.SlowTicks = MOB_SHORT_FREEZE_DURATION_TICKS;
    mobResetMoveStep(moby);
  }
}

//--------------------------------------------------------------------------
int npcOnLocalDamage(Moby* moby, struct MobLocalDamageEventArgs* e)
{
  // don't filter local damage
  return 1;
}

//--------------------------------------------------------------------------
void npcOnStateUpdate(Moby* moby, struct MobStateUpdateEventArgs* e)
{
  mobOnStateUpdate(moby, e);
}

//--------------------------------------------------------------------------
Moby* npcGetNextTarget(Moby* moby)
{
  // check state if we should have a target
  if (moby->State != NPC_STATE_LOOK_AT_TARGET && moby->State != NPC_STATE_WALK_TO_TARGET)
    return NULL;

  struct NpcPVar* pvars = npcGetPVars(moby);
  if (pvars->Parameters.TargetMoby) return pvars->Parameters.TargetMoby;

  return NULL;
}

//--------------------------------------------------------------------------
int npcGetPreferredAction(Moby* moby, int * delayTicks)
{
	struct NpcPVar* pvars = npcGetPVars(moby);
	VECTOR t;

  switch (moby->State)
  {
    case NPC_STATE_OFF: return -1;
    case NPC_STATE_IDLE: return NPC_ACTION_IDLE;
    case NPC_STATE_LOOK_AT_TARGET: return NPC_ACTION_LOOK_AT_TARGET;
  }

	if (pvars->Mob.MobVars.Action == NPC_ACTION_JUMP && pvars->Mob.MobVars.CurrentActionForTicks > 1 && pvars->Mob.MobVars.MoveVars.Grounded) {
		return NPC_ACTION_WALK;
  }

  // jump if we've hit a slope and are grounded
  if (pvars->Mob.MobVars.MoveVars.Grounded && pvars->Mob.MobVars.MoveVars.HitWall && pvars->Mob.MobVars.MoveVars.WallSlope > NPC_MAX_WALKABLE_SLOPE) {
    return NPC_ACTION_JUMP;
  }

  // jump if we've hit a jump point on the path
  if (pvars->Mob.MobVars.MoveVars.QueueJumpSpeed) {
    return NPC_ACTION_JUMP;
  }

	// prevent action changing too quickly
	if (pvars->Mob.MobVars.ActionCooldownTicks)
		return -1;

	// get next target
	Moby * target = npcGetNextTarget(moby);
	if (target) {
    return NPC_ACTION_WALK;
	}
	
  // if roaming, then we want to periodically stop or reroute
  if (npcIsRoaming(pvars)) {

    // check how close we are to target
    vector_subtract(t, pvars->Mob.MobVars.MoveVars.TargetPosition, moby->Position);
    float dist = vector_length(t);
    
    // idle if near target or randomly
    if (dist < pvars->Mob.MobVars.Config.AttackRadius || rand(10007) == 0) {
      return NPC_ACTION_IDLE;
    }
  }

  // idle for 3 seconds
  if (npcIsIdling(pvars) && pvars->Mob.MobVars.CurrentActionForTicks < TPS*3) {
    return NPC_ACTION_IDLE;
  }
	
	return NPC_ACTION_ROAM;
}

//--------------------------------------------------------------------------
#if DEBUGPATH
void npcRenderPath(Moby* moby)
{
  int x,y;
  int i;

  struct NpcPVar* pvars = npcGetPVars(moby);
  struct PathGraph* pathGraph = pathGetMobyPathGraph(moby, &pvars->Mob.MobVars.MoveVars);
  u8* path = (u8*)pvars->Mob.MobVars.MoveVars.CurrentPath;
  int pathLen = pvars->Mob.MobVars.MoveVars.PathEdgeCount;
  int pathIdx = pvars->Mob.MobVars.MoveVars.PathEdgeCurrent;

  if (pathLen > 0) {
    u8* edge = pathGraph->Edges[path[0]];
    if (gfxWorldSpaceToScreenSpace(pathGraph->Nodes[edge[0]], &x, &y)) {
      gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, i == pathIdx ? "o" : "-", -1, 4);
    }
  }
  for (i = 0; i < pathLen; ++i) {
    u8* edge = pathGraph->Edges[path[i]];
    if (gfxWorldSpaceToScreenSpace(pathGraph->Nodes[edge[1]], &x, &y)) {
      gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, i == pathIdx ? "o" : "-", -1, 4);
    }
  }

  VECTOR t;
  if (pathGetTargetPos(pathGraph, t, moby, &pvars->Mob.MobVars.MoveVars) && mobAmIOwner(moby))
    pvars->Mob.MobVars.Dirty = 1;
  if (gfxWorldSpaceToScreenSpace(t, &x, &y)) {
    gfxScreenSpaceText(x, y, 1, 1, 0x80FFFFFF, "+", -1, 4);
  }
}
#endif

//--------------------------------------------------------------------------
void npcDoAction(Moby* moby)
{
  if (moby->State == NPC_STATE_OFF) return;

  struct NpcPVar* pvars = npcGetPVars(moby);
  struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->Mob.MobVars.MoveVars);
	Moby* target = pvars->Mob.MobVars.MoveVars.Target;
	VECTOR t;
  float difficulty = 1;
  float turnSpeed = pvars->Parameters.TurnSpeed * (pvars->Mob.MobVars.MoveVars.Grounded ? NPC_TURN_RADIANS_PER_SEC : NPC_TURN_AIR_RADIANS_PER_SEC);
  float acceleration = pvars->Parameters.Acceleration * (pvars->Mob.MobVars.MoveVars.Grounded ? 1 : 0.2);
  int curAnimId = npcGetCurrentAnimId(moby);

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

#if DEBUGPATH
  gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&npcRenderPath, moby);
#endif

	switch (pvars->Mob.MobVars.Action)
	{
		case NPC_ACTION_IDLE:
		{
			npcTransAnim(moby, pvars->Parameters.IdleAnim.Id, 0);
      mobStand(moby);
			break;
		}
		case NPC_ACTION_LOOK_AT_TARGET:
    {
      mobStand(moby);
      if (target)
        mobTurnTowards(moby, target->Position, turnSpeed);
      npcTransAnim(moby, pvars->Parameters.LookAtPlayerAnim.Id, 0);
      break;
    }
		case NPC_ACTION_JUMP:
			{
        // move
        if (pathGetTargetPos(path, t, moby, &pvars->Mob.MobVars.MoveVars) && mobAmIOwner(moby))
          pvars->Mob.MobVars.Dirty = 1; // new path, sync with other clients
        mobTurnTowards(moby, t, turnSpeed);
        mobGetVelocityToTarget(moby, pvars->Mob.MobVars.MoveVars.Velocity, moby->Position, t, pvars->Mob.MobVars.Config.Speed, acceleration);

        // handle jumping
        if (pvars->Mob.MobVars.MoveVars.Grounded) {
			    npcTransAnim(moby, pvars->Parameters.JumpAnim.Id, 5);

          // check if we're near last jump pos
          // if so increment StuckJumpCount
          if (pvars->Mob.MobVars.MoveVars.IsStuck) {
            if (pvars->Mob.MobVars.MoveVars.StuckJumpCount < 255)
              pvars->Mob.MobVars.MoveVars.StuckJumpCount++;
          }

          // use delta height between target as base of jump speed
          // with min speed
          float jumpSpeed = pvars->Mob.MobVars.MoveVars.QueueJumpSpeed;
          if (jumpSpeed <= 0) {
            jumpSpeed = pvars->Parameters.JumpSpeed; //clamp(0 + (target->Position[2] - moby->Position[2]) * fabsf(pvars->Mob.MobVars.MoveVars.WallSlope) * 1, 3, 15);
          }

          //DLOG(moby, "jump %f\n", jumpSpeed);
          pvars->Mob.MobVars.MoveVars.Velocity[2] = jumpSpeed * MATH_DT;
          pvars->Mob.MobVars.MoveVars.Grounded = 0;
          pvars->Mob.MobVars.MoveVars.QueueJumpSpeed = 0;
        }
				break;
			}
    case NPC_ACTION_ROAM:
    case NPC_ACTION_WALK:
		{
      int walkAnimId = pvars->Parameters.WalkAnim.Id;
      float dir = 0;
      if (target) {
        dir = ((pvars->Mob.MobVars.ActionId + pvars->Mob.MobVars.Random) % 3) - 1;
      }

      // determine next position
      if (pathGetTargetPos(path, t, moby, &pvars->Mob.MobVars.MoveVars) && mobAmIOwner(moby))
        pvars->Mob.MobVars.Dirty = 1; // new path, sync with other clients
      mobMoveTowards(moby, t, pvars->Mob.MobVars.Config.Speed, turnSpeed, acceleration, dir);

			// 
      if (curAnimId == pvars->Parameters.JumpAnim.Id && !pvars->Mob.MobVars.MoveVars.Grounded) {
        // wait for jump to land
      } else if (pvars->Mob.MobVars.MoveVars.QueueJumpSpeed) {
        npcForceLocalAction(moby, NPC_ACTION_JUMP);
      } else if (!mobHasVelocity(&pvars->Mob)) {
				npcTransAnim(moby, pvars->Parameters.IdleAnim.Id, 0);
      } else if (curAnimId != walkAnimId) { // || pvars->Mob.MobVars.AnimationLooped) {
				npcTransAnim(moby, walkAnimId, 0);
      }
			break;
		}
    case NPC_ACTION_DIE:
    {
      mobStand(moby);
      break;
    }
	}

  pvars->Mob.MobVars.CurrentActionForTicks ++;
}

//--------------------------------------------------------------------------
void npcDoDamage(Moby* moby, float radius, float amount, int damageFlags, int friendlyFire)
{
  //mobDoDamage(moby, moby, radius, amount, damageFlags, friendlyFire, NPC_SUBSKELETON_JOINT_JAW, 1, 0);
}

//--------------------------------------------------------------------------
void npcForceLocalAction(Moby* moby, int action)
{
  struct NpcPVar* pvars = npcGetPVars(moby);
  float difficulty = 1;

  if (MapConfig.State)
    difficulty = MapConfig.State->Difficulty;

	// from
	switch (pvars->Mob.MobVars.Action)
	{
    case NPC_ACTION_DIE:
    {
      // can't undie
      return;
    }
	}

	// to
	switch (action)
	{
    case NPC_ACTION_ROAM:
    {
      struct PathGraph* path = pathGetMobyPathGraph(moby, &pvars->Mob.MobVars.MoveVars);
      if (path && path->NumNodes > 0 && mobAmIOwner(moby)) {

        int r = rand(path->NumNodes);
        int count = 0;

        // if we're in a spawner
        // then try and find a node thats in a habitable cuboid
        if (moby->PParent && moby->PParent->OClass == SPAWNER_OCLASS) {
          while (count < path->NumNodes && !spawnerOnChildConsiderRoamTarget(moby->PParent, moby, pvars->Mob.MobVars.Userdata, path->Nodes[r])) {
            r = (r + 1) % path->NumNodes;
            ++count;
          }
        }

        vector_copy(pvars->Mob.MobVars.MoveVars.TargetPosition, path->Nodes[r]);
        pvars->Mob.MobVars.MoveVars.TargetPosition[3] = 0;
      }
      break;
    }
		case NPC_ACTION_WALK:
		{
			
			break;
		}
		case NPC_ACTION_DIE:
		{
			// disable collision
			moby->CollActive = 1;
			break;
		}
		default:
		{
			break;
		}
	}

	// 
  if (action != pvars->Mob.MobVars.Action)
    pvars->Mob.MobVars.CurrentActionForTicks = 0;

	pvars->Mob.MobVars.Action = action;
	pvars->Mob.MobVars.NextAction = -1;
	pvars->Mob.MobVars.ActionCooldownTicks = NPC_ACTION_COOLDOWN_TICKS;
}

//--------------------------------------------------------------------------
short npcGetArmor(Moby* moby)
{
  struct NpcPVar* pvars = npcGetPVars(moby);
  return pvars->Mob.MobVars.Config.Bangles;
}

//--------------------------------------------------------------------------
int npcIsAttacking(Moby* moby)
{
  return 0;
}

//--------------------------------------------------------------------------
int npcCanNonOwnerTransitionToAction(Moby* moby, int action)
{
  // always let non-owners simulate an action unless its the death action
  if (action == NPC_ACTION_DIE) return 0;

  return 1;
}

//--------------------------------------------------------------------------
int npcShouldForceStateUpdateOnAction(Moby* moby, int action)
{
  struct NpcPVar* pvars = npcGetPVars(moby);

  // only send state updates at regular intervals, unless dying
  // or if we're entering/leaving the roaming state
  if (action == NPC_ACTION_DIE) return 1;
  if (pvars->Mob.MobVars.Action == NPC_ACTION_ROAM || action == NPC_ACTION_ROAM) return 1;

  return 0;
}

//--------------------------------------------------------------------------
int npcIsRoaming(struct NpcPVar* pvars)
{
	return pvars->Mob.MobVars.Action == NPC_ACTION_ROAM;
}

//--------------------------------------------------------------------------
int npcIsIdling(struct NpcPVar* pvars)
{
	return pvars->Mob.MobVars.Action == NPC_ACTION_IDLE;
}

//--------------------------------------------------------------------------
void npcOnGuberCreated(Moby* moby)
{
  struct NpcPVar* pvars = (struct NpcPVar*)moby->PVar;

  // move pvars up for mob handler
  moby->PVar = &pvars->Mob;
  moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;
  
  // initialize mobvars
  pvars->Mob.MobVars.Config.Damage = 0;
  pvars->Mob.MobVars.Config.Speed = pvars->Parameters.Speed;
  pvars->Mob.MobVars.Config.Health = pvars->Parameters.Health;
  pvars->Mob.MobVars.Config.AttackRadius = pvars->Parameters.InteractRange;
  pvars->Mob.MobVars.Config.HitRadius = 1;
  pvars->Mob.MobVars.Config.CollRadius = pvars->Parameters.CollRadius;
  pvars->Mob.MobVars.Config.AutoAggroMaxRange = 0;
  pvars->Mob.MobVars.Config.VisionRange = pvars->Parameters.InteractRange;
  pvars->Mob.MobVars.Config.PeripheryRangeTheta = 90 * MATH_DEG2RAD;
  pvars->Mob.MobVars.Config.Bolts = 0;
  pvars->Mob.MobVars.Config.Xp = 0;
  pvars->Mob.MobVars.Config.ReactionTickCount = 0;
  pvars->Mob.MobVars.Config.AttackCooldownTickCount = 10;
  pvars->Mob.MobVars.Config.OutOfSightDeAggroTickCount = 0;
  pvars->Mob.MobVars.Config.Bangles = pvars->Parameters.Bangles;
  pvars->Mob.MobVars.MoveVars.PathGraphIdx = pvars->Parameters.PathGraphIdx;
  pvars->Mob.VTable = &NpcVTable;
  pvars->Mob.MobVars.Action = NPC_ACTION_ROAM;
  
  // update pvars target references
  pvars->Parameters.NpcMoby = mobyGetFromIdxOrNull((int)pvars->Parameters.NpcMoby);
  pvars->Parameters.TargetMoby = mobyGetFromIdxOrNull((int)pvars->Parameters.TargetMoby);
  pvars->Parameters.AttachedMoby = mobyGetFromIdxOrNull((int)pvars->Parameters.AttachedMoby);
  DLOG(moby, "npc %08X found npc moby %08X\n", (u32)moby, (u32)pvars->Parameters.NpcMoby);
  DLOG(moby, "npc %08X found target moby %08X\n", (u32)moby, (u32)pvars->Parameters.TargetMoby);
  DLOG(moby, "npc %08X found attached moby %08X\n", (u32)moby, (u32)pvars->Parameters.AttachedMoby);

  // no npc moby, destroy
  if (!pvars->Parameters.NpcMoby) {
    guberMobyDestroy(moby);
    return;
  }

  // move to target
  vector_copy(moby->Position, pvars->Parameters.NpcMoby->Position);
  vector_copy(moby->Rotation, pvars->Parameters.NpcMoby->Rotation);
  vector_copy(pvars->Parameters.SpawnPosition, moby->Position);

  // calculate attached moby offset
  if (pvars->Parameters.AttachedMoby) {
    vector_subtract(pvars->Parameters.AttachedMobyOffset, pvars->Parameters.AttachedMoby->Position, moby->Position);
    pvars->Parameters.AttachedMobyYawOffset = pvars->Parameters.AttachedMoby->Rotation[2] - moby->Rotation[2];
  }

  // calculate attached cuboid offset
  if (pvars->Parameters.AttachedCuboidIdx >= 0) {
    SpawnPoint* sp = spawnPointGet(pvars->Parameters.AttachedCuboidIdx);
    vector_subtract(pvars->Parameters.AttachedCuboidOffset, &sp->M0[12], moby->Position);
  }

  // register
  if (MapConfig.RegisterNpcFunc)
    MapConfig.RegisterNpcFunc(moby);

  mobySetState(moby, pvars->Parameters.DefaultState, -1);
}

//--------------------------------------------------------------------------
void npcStart(void)
{
  //npcInitialized = 1;
}

//--------------------------------------------------------------------------
void npcInit(void)
{
  Moby* temp = mobySpawn(NPC_MOBY_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("NPC oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);

  // create gubers for npcs
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, NPC_MOBY_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DLOG(moby, "found npc %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        npcOnGuberCreated(moby);
      }
    }

		++moby;
	}

  DPRINTF("npc pvar size %d\n", sizeof(struct NpcPVar));
}
