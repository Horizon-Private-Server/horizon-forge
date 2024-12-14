/***************************************************
 * FILENAME :		controller.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the controllers.
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

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
#include <libdl/spawnpoint.h>
#include <libdl/color.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include "maputils.h"
#include "shared.h"
#include "spawner.h"
#include "checkpoint.h"
#include "gate.h"
#include "mover.h"
#include "controller.h"
#include "npc.h"
#include "mob.h"
#include "game.h"

#define DLOG(moby, format, ...) if (((struct ControllerPVar*)moby->PVar)->Log) { DPRINTF(format, ##__VA_ARGS__); }

int controllerInitialized = 0;

//--------------------------------------------------------------------------
int controllerAnyTriggerActivated(Moby* moby)
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  
  return pvars->State.TriggersActivated;
}

//--------------------------------------------------------------------------
int controllerIsMobyStateConditionTrue(Moby* moby, int conditionIdx)
{
  int result = 0;
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  struct ControllerCondition* condition = &pvars->Conditions[conditionIdx];
  
  // invalid moby
  Moby* target = condition->Moby;
  if (!target || mobyIsDestroyed(target)) {
    condition->Moby = NULL;
    return 0;
  }

  switch (condition->MobyState.StateInteractType) {
    case CONTROLLER_MOBY_INTERACT_EQUAL: result = target->State == condition->MobyState.State; break;
    case CONTROLLER_MOBY_INTERACT_NOTEQUAL: result = target->State != condition->MobyState.State; break;
    case CONTROLLER_MOBY_INTERACT_LESS: result = target->State < condition->MobyState.State; break;
    case CONTROLLER_MOBY_INTERACT_LEQUAL: result = target->State <= condition->MobyState.State; break;
    case CONTROLLER_MOBY_INTERACT_GREATER: result = target->State > condition->MobyState.State; break;
    case CONTROLLER_MOBY_INTERACT_GEQUAL: result = target->State >= condition->MobyState.State; break;
  }

  return result;
}

//--------------------------------------------------------------------------
int controllerIsCuboidConditionTrue(Moby* moby, int conditionIdx, char validPlayers[GAME_MAX_PLAYERS])
{
  int j;
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  struct ControllerCondition* condition = &pvars->Conditions[conditionIdx];
  Player** players = playerGetAll();
  int cuboidIdx = condition->Cuboid.CuboidIdx;
  SpawnPoint* triggerCuboid = spawnPointGet(cuboidIdx);
  int pSucceeded = 0, pCount = 0;
  int npcSucceeded = 0, npcCount = 0;

  // no trigger by
  if (cuboidIdx < 0) return 0;
  if (!condition->Cuboid.TriggerBy) return 0;

  // check for players
  if (condition->Cuboid.TriggerBy & CONTROLLER_CUBOID_TRIGGER_BY_CHECK_PLAYER) {
    for (j = 0; j < GAME_MAX_PLAYERS; ++j) {
      Player* p = players[j];
      if (!playerIsValid(p) || playerIsDead(p)) continue;

      // check if player is inside the cuboid
      int isInside = spawnPointIsPointInside(triggerCuboid, p->PlayerPosition, NULL);
      if (isInside != condition->Cuboid.InteractType && validPlayers[j]) {
        ++pSucceeded;
        pvars->State.TriggeredByMoby = p->PlayerMoby;
      } else {
        validPlayers[j] = 0;
      }

      ++pCount;
    }
  }

  // check for npcs
  if (condition->Cuboid.TriggerBy & CONTROLLER_CUBOID_TRIGGER_BY_CHECK_NPC) {
    Moby* npcMoby = mobyListGetStart();
    Moby* mEnd = mobyListGetEnd();
    while (npcMoby < mEnd) {
      if (!mobyIsDestroyed(npcMoby) && mobyIsMob(npcMoby)) {
          
        // check if moby is inside the cuboid
        int isInside = spawnPointIsPointInside(triggerCuboid, npcMoby->Position, NULL);
        if (isInside != condition->Cuboid.InteractType) {
          ++npcSucceeded;
          pvars->State.TriggeredByMoby = npcMoby;
        }

        ++npcCount;
      }

      ++npcMoby;
    }
  }

  int hasAnyPlayer = pSucceeded > 0 && pCount > 0;
  int hasAllPlayers = pSucceeded > 0 && pSucceeded == pCount;
  int hasNoPlayers = pSucceeded == 0 && pCount > 0;
  int hasAnyNpc = npcSucceeded > 0 && npcCount > 0;
  int hasAllNpcs = npcSucceeded > 0 && npcSucceeded == npcCount;
  int hasNoNpcs = npcSucceeded == 0; // && npcCount > 0;
  int succeeded = 0, count = 0;

  for (j = 0; j < 8; ++j) {
    int bit = 1 << j;
    if ((condition->Cuboid.TriggerBy & bit)) {
      switch (bit) {
        case CONTROLLER_CUBOID_TRIGGER_BY_ANY_PLAYER: succeeded += hasAnyPlayer; break;
        case CONTROLLER_CUBOID_TRIGGER_BY_ALL_PLAYERS: succeeded += hasAllPlayers; break;
        case CONTROLLER_CUBOID_TRIGGER_BY_NO_PLAYERS: succeeded += hasNoPlayers; break;
        case CONTROLLER_CUBOID_TRIGGER_BY_ANY_NPC: succeeded += hasAnyNpc; break;
        case CONTROLLER_CUBOID_TRIGGER_BY_ALL_NPCS: succeeded += hasAllNpcs; break;
        case CONTROLLER_CUBOID_TRIGGER_BY_NO_NPCS: succeeded += hasNoNpcs; break;
      }
      ++count;
    }
  }

  return succeeded > 0 && (!condition->Cuboid.TriggerByAll || succeeded == count);
}

//--------------------------------------------------------------------------
int controllerIsPlayerButtonConditionTrue(Moby* moby, int conditionIdx, char validPlayers[GAME_MAX_PLAYERS])
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  struct ControllerCondition* condition = &pvars->Conditions[conditionIdx];
  Player** players = playerGetAll();
  int j;
  int succeeded = 0;

  for (j = 0; j < GAME_MAX_PLAYERS; ++j) {
    Player* p = players[j];
    if (!playerIsValid(p) || playerIsDead(p)) continue;

    // check if player is inside the cuboid
    int hasButtonMask = playerPadGetButton(p, condition->PlayerButtons.PadMask);
    if (hasButtonMask && validPlayers[j]) {
      ++succeeded;
      pvars->State.TriggeredByMoby = p->PlayerMoby;
    } else {
      validPlayers[j] = 0;
    }
  }

  return succeeded > 0;
}

//--------------------------------------------------------------------------
int controllerIsDelayConditionTrue(Moby* moby, int conditionIdx)
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  struct ControllerCondition* condition = &pvars->Conditions[conditionIdx];
  
  int startTime = pvars->State.DelayStartTime[conditionIdx];
  if (startTime <= 0) pvars->State.DelayStartTime[conditionIdx] = startTime = gameGetTime();

  if (condition->Delay.Milliseconds < (gameGetTime() - startTime)) {
    //pvars->State.DelayStartTime[conditionIdx] = startTime = gameGetTime();
    return 1;
  }

  return 0;
}

//--------------------------------------------------------------------------
int controllerIsXORConditionTrue(Moby* moby, int conditionIdx)
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;

  return !pvars->State.TriggersActivated;
}

//--------------------------------------------------------------------------
int controllerIsNpcTargetConditionTrue(Moby* moby, int conditionIdx)
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  struct ControllerCondition* condition = &pvars->Conditions[conditionIdx];
  int cuboidIdx = condition->NPCTarget.CuboidIdx;
  if (cuboidIdx < 0) return 0;

  Moby* npcMoby = condition->Moby;
  SpawnPoint* triggerCuboid = spawnPointGet(cuboidIdx);

  if (!npcMoby) return 0;
  if (!triggerCuboid) return 0;
  if (mobyIsDestroyed(npcMoby) || !mobyIsNpc(npcMoby)) return 0;

  struct NpcPVar* npcPvars = npcGetPVars(npcMoby);

  if (!npcPvars) return 0;
  Moby* npcTargetMoby = npcPvars->Mob.MobVars.MoveVars.Target;
  if (!npcTargetMoby) return 0;

  int isInside = spawnPointIsPointInside(triggerCuboid, npcTargetMoby->Position, NULL);
  return condition->NPCTarget.InteractType != isInside;
}

//--------------------------------------------------------------------------
int controllerDifficultyConditionTrue(Moby* moby, int conditionIdx)
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  struct ControllerCondition* condition = &pvars->Conditions[conditionIdx];

  return MapConfig.State && (condition->Difficulty.Mask & (1 << MapConfig.State->DifficultyStars)) != 0;
}

//--------------------------------------------------------------------------
int controllerCheckpointConditionTrue(Moby* moby, int conditionIdx)
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  struct ControllerCondition* condition = &pvars->Conditions[conditionIdx];
  
  // invalid moby
  Moby* target = condition->Moby;
  if (!target || mobyIsDestroyed(target) || target->OClass != CHECKPOINT_OCLASS) {
    condition->Moby = NULL;
    return 0;
  }

  return target->State == condition->Checkpoint.IsActive;
}

//--------------------------------------------------------------------------
void controllerUpdateTriggers(Moby* moby)
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  int i,j;
  int succeeded = 0;
  int count = 0;
  char validPlayers[GAME_MAX_PLAYERS];
  memset(validPlayers, 1, sizeof(validPlayers));

  for (i = 0; i < CONTROLLER_MAX_CONDITIONS; ++i) {
    switch (pvars->Conditions[i].ConditionType) {
      case CONTROLLER_CONDITION_TYPE_CUBOID: count += 1; succeeded += controllerIsCuboidConditionTrue(moby, i, validPlayers); break;
      case CONTROLLER_CONDITION_TYPE_MOBY_STATE: count += 1; succeeded += controllerIsMobyStateConditionTrue(moby, i); break;
      case CONTROLLER_CONDITION_TYPE_PLAYER_BUTTON: count += 1; succeeded += controllerIsPlayerButtonConditionTrue(moby, i, validPlayers); break;
      case CONTROLLER_CONDITION_TYPE_DELAY: count += 1; succeeded += controllerIsDelayConditionTrue(moby, i); break;
      case CONTROLLER_CONDITION_TYPE_XOR: count += 1; succeeded += controllerIsXORConditionTrue(moby, i); break;
      case CONTROLLER_CONDITION_TYPE_NPC_TARGET: count += 1; succeeded += controllerIsNpcTargetConditionTrue(moby, i); break;
      case CONTROLLER_CONDITION_TYPE_DIFFICULTY: count += 1; succeeded += controllerDifficultyConditionTrue(moby, i); break;
      case CONTROLLER_CONDITION_TYPE_CHECKPOINT: count += 1; succeeded += controllerCheckpointConditionTrue(moby, i); break;
      default: break;
    }

    // stop when condition fails
    if (pvars->TriggerIfAllTrue && succeeded < count) {
      
      // also reset any delays that weren't yet hit
      // if the condition we stopped on was not a delay
      if (pvars->Conditions[i].ConditionType != CONTROLLER_CONDITION_TYPE_DELAY) {
        for (j = i+1; j < CONTROLLER_MAX_CONDITIONS; ++j) {
          if (pvars->Conditions[j].ConditionType == CONTROLLER_CONDITION_TYPE_DELAY)
            pvars->State.DelayStartTime[j] = 0;
        }
      }
      
      break;
    }
  }

  // update activate
  pvars->State.TriggersActivated = (succeeded == 0 && count == 0)
                                || (succeeded > 0 && pvars->TriggerIfAllTrue == 0)
                                || (succeeded > 0 && succeeded == count && pvars->TriggerIfAllTrue == 1);
}

//--------------------------------------------------------------------------
int controllerControlMobyState(Moby* moby, struct ControllerTarget* target)
{
  int state = target->Moby.State;
  Moby* targetMoby = target->Moby.Moby;
  if (!targetMoby || mobyIsDestroyed(targetMoby)) return 0;

  if (target->TargetUpdateType == CONTROLLER_TARGET_UPDATE_TYPE_MOBY_STATE_ADDITIVE)
    state += targetMoby->State;

  DLOG(moby, "controller set %08X state %d=>%d\n", (u32)targetMoby, targetMoby->State, state);

  // only when state changes
  if (targetMoby->State == state) return 0;

  // negative is destroyed
  if (state < 0) {
    guberMobyDestroy(targetMoby);
    return 1;
  }

  // handle special cases
  switch (targetMoby->OClass) {
    case SPAWNER_OCLASS: if (gameAmIHost()) { spawnerBroadcastNewState(targetMoby, state); }; break;
    case MOVER_OCLASS: if (gameAmIHost()) { moverBroadcastNewState(targetMoby, state); } break;
    case CONTROLLER_OCLASS: if (gameAmIHost()) { controllerBroadcastNewState(targetMoby, state); } break;
#if GATE
    case GATE_OCLASS: if (gameAmIHost()) { gateBroadcastNewState(targetMoby, state); } break;
#endif
    case MOBY_ID_BOLT_CRANK_MP: if (targetMoby->PVar && targetMoby->State == 5) { POKE_U32(targetMoby->PVar, 0); } mobySetState(targetMoby, state, -1); break;
    default: mobySetState(targetMoby, state, -1); break;
  }

  return 1;
}

//--------------------------------------------------------------------------
int controllerControlMobyAnimation(Moby* moby, struct ControllerTarget* target)
{
  int animId = target->Moby.AnimId;
  Moby* targetMoby = target->Moby.Moby;
  if (!targetMoby || mobyIsDestroyed(targetMoby) || !targetMoby->PClass) return 0;

  // bad anim id
  int seqCount = *(char*)(targetMoby->PClass + 0x0C);
  if (animId >= seqCount) return 0;
  if (targetMoby->AnimSeqId == animId) return 0;

  DLOG(moby, "set anim %08X => %d\n", (u32)targetMoby, animId);
  mobyAnimTransition(targetMoby, animId, 0, 0);
  return 1;
}

//--------------------------------------------------------------------------
int controllerControlMobyEnabled(Moby* moby, struct ControllerTarget* target)
{
  Moby* targetMoby = target->Moby.Moby;
  if (!targetMoby || mobyIsDestroyed(targetMoby)) return 0;
  
  if (target->Moby.Enabled) {
    if (targetMoby->CollActive == 0 && (targetMoby->ModeBits & MOBY_MODE_BIT_DISABLED) == 0) return 0;
    targetMoby->CollActive = 0;
    targetMoby->ModeBits &= ~MOBY_MODE_BIT_DISABLED;
  } else {
    if (targetMoby->CollActive && (targetMoby->ModeBits & MOBY_MODE_BIT_DISABLED)) return 0;
    targetMoby->CollActive = -1;
    targetMoby->ModeBits |= MOBY_MODE_BIT_DISABLED;
  }

  return 1;
}

//--------------------------------------------------------------------------
int controllerControlMobySetCheckpoint(Moby* moby, struct ControllerTarget* target)
{
  Moby* targetMoby = target->Moby.Moby;
  if (!targetMoby || mobyIsDestroyed(targetMoby) || targetMoby->OClass != CHECKPOINT_OCLASS) return 0;
  
  return checkpointSetActive(targetMoby);
}

//--------------------------------------------------------------------------
int controllerControlCuboidMove(Moby* moby, struct ControllerTarget* target)
{
  int srcIdx = target->Cuboid.SrcIdx;
  int dstIdx = target->Cuboid.DestIdx;
  if (srcIdx < 0 || dstIdx < 0) return 0;
  
  SpawnPoint* src = spawnPointGet(srcIdx);
  SpawnPoint* dst = spawnPointGet(dstIdx);
  if (memcmp(src, dst, sizeof(SpawnPoint)) == 0) return 0;

  memcpy(dst, src, sizeof(SpawnPoint));
  return 1;
}

//--------------------------------------------------------------------------
int controllerControlNPCTarget(Moby* moby, struct ControllerTarget* target)
{
  Moby* npcMoby = target->Moby.Moby;
  Moby* npcTargetMoby = target->Moby.NPCTargetMoby;
  if (!npcMoby || mobyIsDestroyed(npcMoby) || !npcMoby->PVar) return 0;

  struct NpcPVar* npcPvars = npcGetPVars(npcMoby);
  if (npcPvars && npcPvars->Parameters.TargetMoby != npcTargetMoby) {
    npcPvars->Parameters.TargetMoby = npcTargetMoby;
    DLOG(moby, "set npc target %08X => %08X\n", (u32)npcMoby, (u32)npcTargetMoby);
    return 1;
  }
  return 0;
}

//--------------------------------------------------------------------------
int controllerControlNPCTargetTriggered(Moby* moby, struct ControllerTarget* target)
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  Moby* npcMoby = target->Moby.Moby;
  Moby* npcTargetMoby = pvars->State.TriggeredByMoby;
  if (!npcMoby || mobyIsDestroyed(npcMoby) || !npcMoby->PVar) return 0;

  struct NpcPVar* npcPvars = npcGetPVars(npcMoby);
  if (npcPvars && npcPvars->Parameters.TargetMoby != npcTargetMoby) {
    npcPvars->Parameters.TargetMoby = npcTargetMoby;
    DLOG(moby, "set npc target %08X => %08X\n", (u32)npcMoby, (u32)npcTargetMoby);
    return 1;
  }
  return 0;
}

//--------------------------------------------------------------------------
int controllerControlGivePlayerAmmo(Moby* moby, struct ControllerTarget* target)
{
  if (!target->GivePlayer.PlayerMask) return 0;

  Player** players = playerGetAll();
  int i;
  int count = 0;
  int acceptsHost = (target->RespawnPlayer.PlayerMask & CONTROLLER_PLAYER_MASK_HOST) && gameAmIHost();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!player || !player->PlayerMoby || !player->GadgetBox) continue;

    int bit = 1 << i;
    if ((target->RespawnPlayer.PlayerMask & bit) != 0 || (acceptsHost && player->IsLocal)) {
      if (target->GivePlayer.LivingOnly && playerIsDead(player)) continue;
      
      int j;
      for (j = 1; j < WEAPON_SLOT_COUNT; ++j) {
        int gadgetId = weaponSlotToId(j);
        int maxAmmo = playerGetWeaponMaxAmmo(player->GadgetBox, gadgetId);
        int newAmmo = player->GadgetBox->Gadgets[gadgetId].Ammo;

        if (target->GivePlayer.Amount == 0) newAmmo = maxAmmo;
        else newAmmo += target->GivePlayer.Amount;
        if (newAmmo < 0) newAmmo = 0;
        else if (newAmmo > maxAmmo) newAmmo = maxAmmo;
        
        player->GadgetBox->Gadgets[gadgetId].Ammo = newAmmo;
      }
      count++;
    }
  }

  return count;
}

//--------------------------------------------------------------------------
int controllerControlGivePlayerHealth(Moby* moby, struct ControllerTarget* target)
{
  if (!target->GivePlayer.PlayerMask) return 0;

  Player** players = playerGetAll();
  int i;
  int count = 0;
  int acceptsHost = (target->RespawnPlayer.PlayerMask & CONTROLLER_PLAYER_MASK_HOST) && gameAmIHost();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!player || !player->PlayerMoby) continue;

    int bit = 1 << i;
    if ((target->RespawnPlayer.PlayerMask & bit) != 0 || (acceptsHost && player->IsLocal)) {
      if (target->GivePlayer.LivingOnly && playerIsDead(player)) continue;
      
      if (target->GivePlayer.Amount >= 0 && playerIsDead(player)) playerRespawn(player);
      if (target->GivePlayer.Amount == 0) playerSetHealth(player, player->MaxHealth);
      else playerSetHealth(player, clamp(player->Health + target->GivePlayer.Amount, 0, player->MaxHealth));
      
      count++;
    }
  }
  
  return count;
}

//--------------------------------------------------------------------------
int controllerControlRespawnPlayer(Moby* moby, struct ControllerTarget* target)
{
  if (!target->RespawnPlayer.PlayerMask) return 0;

  Player** players = playerGetAll();
  int i;
  int count = 0;
  int acceptsHost = (target->RespawnPlayer.PlayerMask & CONTROLLER_PLAYER_MASK_HOST) && gameAmIHost();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!player || !player->PlayerMoby) continue;

    int bit = 1 << i;
    if ((target->RespawnPlayer.PlayerMask & bit) != 0 || (acceptsHost && player->IsLocal)) {
      if (target->RespawnPlayer.DeadOnly && !playerIsDead(player)) continue;
      
      playerRespawn(player);
      count++;
    }
  }
  
  return count;
}

//--------------------------------------------------------------------------
int controllerControlCompleteMission(Moby* moby, struct ControllerTarget* target)
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  if (!MapConfig.State) return 0;
  if (MapConfig.State->MissionComplete) return 0;
  
  MapConfig.State->MissionComplete = 1;
  MapConfig.State->MissionCompleteTime = gameAmIHost() ? gameGetTime() : pvars->State.RemoteIterationTime;
  DLOG(moby, "mission end\n");
  return 1;
}

//--------------------------------------------------------------------------
int controllerControlSetAmmoDropProbability(Moby* moby, struct ControllerTarget* target)
{
  if (!MapConfig.State) return 0;
  if (MapConfig.State->AmmoDropChance == target->Value.FloatValue) return 0;

  MapConfig.State->AmmoDropChance = target->Value.FloatValue;
  return 1;
}

//--------------------------------------------------------------------------
int controllerControlSetRefillAmmoCostMultiplier(Moby* moby, struct ControllerTarget* target)
{
  if (!MapConfig.State) return 0;
  if (MapConfig.State->AmmoRefillCostMultiplier == target->Value.FloatValue) return 0;

  MapConfig.State->AmmoRefillCostMultiplier = target->Value.FloatValue;
  return 1;
}

//--------------------------------------------------------------------------
int controllerIterate(Moby* moby)
{
  int i;
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  int changed = 0;

  for (i = 0; i < CONTROLLER_MAX_TARGETS; ++i) {
    if (MapConfig.State && (pvars->Targets[i].DifficultyMask & (1 << MapConfig.State->DifficultyStars)) == 0) continue;

    switch (pvars->Targets[i].TargetUpdateType) {
      case CONTROLLER_TARGET_UPDATE_TYPE_MOBY_STATE: changed += controllerControlMobyState(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_MOBY_STATE_ADDITIVE: changed += controllerControlMobyState(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_MOBY_ANIMATION: changed += controllerControlMobyAnimation(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_MOBY_ENABLED: changed += controllerControlMobyEnabled(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_MOVE_CUBOID: changed += controllerControlCuboidMove(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_NPC_CONTROLLER_TARGET: changed += controllerControlNPCTarget(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_NPC_CONTROLLER_TARGET_TO_TRIGGERED: changed += controllerControlNPCTargetTriggered(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_GIVE_PLAYER_AMMO: changed += controllerControlGivePlayerAmmo(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_GIVE_PLAYER_HEALTH: changed += controllerControlGivePlayerHealth(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_RESPAWN: changed += controllerControlRespawnPlayer(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_COMPLETE_MISSION: changed += controllerControlCompleteMission(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_MOBY_SET_CHECKPOINT: changed += controllerControlMobySetCheckpoint(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_SET_AMMO_DROP_PROBABILITY: changed += controllerControlSetAmmoDropProbability(moby, &pvars->Targets[i]); break;
      case CONTROLLER_TARGET_UPDATE_TYPE_SET_REFILL_AMMO_COST_MULTIPLIER: changed += controllerControlSetRefillAmmoCostMultiplier(moby, &pvars->Targets[i]); break;
    }
  }
  
  pvars->State.Iterations++;
  return changed;
}

//--------------------------------------------------------------------------
void controllerOnStateChanged(Moby* moby)
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;

  switch (moby->State)
  {
    case CONTROLLER_STATE_DEACTIVATED:
    case CONTROLLER_STATE_COMPLETED:
    {
      // reset runtime stats when deactivating
      memset(&pvars->State, 0, sizeof(pvars->State));
      break;
    }
    case CONTROLLER_STATE_IDLE:
    {
      break;
    }
    case CONTROLLER_STATE_ACTIVATED:
    {
      break;
    }
  }
}

//--------------------------------------------------------------------------
void controllerBroadcastNewState(Moby* moby, enum ControllerState state)
{
	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, CONTROLLER_EVENT_SET_STATE);
  if (guberEvent) {
    guberEventWrite(guberEvent, &state, 4);
  }
}

//--------------------------------------------------------------------------
void controllerBroadcastIterate(Moby* moby)
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  u32 triggeredByUid = guberGetUID(pvars->State.TriggeredByMoby);
  int time = gameGetTime();

	// create event
	GuberEvent * guberEvent = guberCreateEvent(moby, CONTROLLER_EVENT_ITERATE);
  if (guberEvent) {
    guberEventWrite(guberEvent, &pvars->State.Iterations, 4);
    guberEventWrite(guberEvent, &triggeredByUid, 4);
    guberEventWrite(guberEvent, &time, 4);
    DLOG(moby, "broadcast iterate triggeredby:%08X %08X\n", (u32)triggeredByUid, (u32)pvars->State.TriggeredByMoby);
  }
}

//--------------------------------------------------------------------------
void controllerUpdate(Moby* moby)
{
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;

  // initialize by sending first state
  if (!pvars->Init) {
    if (gameAmIHost() && controllerInitialized) {
      controllerBroadcastNewState(moby, pvars->DefaultState);
    }
    
    return;
  }

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    controllerOnStateChanged(moby);
    moby->Triggers |= 1;
  }

  if (!gameAmIHost()) return;
  if (!missionIsActive()) return;
  if (moby->State == CONTROLLER_STATE_DEACTIVATED) return;
  if (moby->State == CONTROLLER_STATE_COMPLETED) return;
  if (MapConfig.State && MapConfig.State->MissionComplete) return;

  // update triggers
  controllerUpdateTriggers(moby);

  // check if we've reached the iteration count
  if (pvars->Repeat > 0 && pvars->State.Iterations >= pvars->Repeat) {
    DLOG(moby, "controller %08X complete\n", (u32)moby);
    controllerBroadcastNewState(moby, CONTROLLER_STATE_COMPLETED);
    return;
  }

  // check if we should exit idle
  if (moby->State == CONTROLLER_STATE_IDLE && controllerAnyTriggerActivated(moby)) {
    DLOG(moby, "controller %08X exit idle\n", (u32)moby);
    controllerBroadcastNewState(moby, CONTROLLER_STATE_ACTIVATED);
  } else if (moby->State == CONTROLLER_STATE_ACTIVATED && !controllerAnyTriggerActivated(moby)) {
    DLOG(moby, "controller %08X idle\n", (u32)moby);
    controllerBroadcastNewState(moby, CONTROLLER_STATE_IDLE);
    return;
  } else if (moby->State != CONTROLLER_STATE_ACTIVATED) {
    return;
  }

  // each iteration is broadcasted to everyone
  // but only if the iteration changed any target(s)
  // otherwise we assume the same would be true on other clients and skip broadcasting
  // this guarantees sync (for the most part)
  // at the cost of network bandwidth if configured to run every tick
  if (controllerIterate(moby)) {
    controllerBroadcastIterate(moby);
  }

  memset(pvars->State.DelayStartTime, 0, sizeof(pvars->State.DelayStartTime));
}

//--------------------------------------------------------------------------
void controllerOnGuberCreated(Moby* moby)
{
  int i;
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;

  moby->PUpdate = &controllerUpdate;
  moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;

  memset(&pvars->State, 0, sizeof(pvars->State));
  pvars->Init = 0;
  
  // update pvars target references
  for (i = 0; i < CONTROLLER_MAX_TARGETS; ++i) {
    DLOG(moby, "controller TargetUpdateType %d\n", pvars->Targets[i].TargetUpdateType);
    switch (pvars->Targets[i].TargetUpdateType)
    {
      case CONTROLLER_TARGET_UPDATE_TYPE_NPC_CONTROLLER_TARGET:
      case CONTROLLER_TARGET_UPDATE_TYPE_NPC_CONTROLLER_TARGET_TO_TRIGGERED:
      {
        pvars->Targets[i].Moby.NPCTargetMoby = mobyGetFromIdxOrNull((int)pvars->Targets[i].Moby.NPCTargetMoby);
        DLOG(moby, "controller %08X found npc target %d %08X\n", (u32)moby, i, (u32)pvars->Targets[i].Moby.NPCTargetMoby);
      }
      case CONTROLLER_TARGET_UPDATE_TYPE_MOBY_STATE:
      case CONTROLLER_TARGET_UPDATE_TYPE_MOBY_ANIMATION:
      case CONTROLLER_TARGET_UPDATE_TYPE_MOBY_ENABLED:
      case CONTROLLER_TARGET_UPDATE_TYPE_MOBY_STATE_ADDITIVE:
      case CONTROLLER_TARGET_UPDATE_TYPE_MOBY_SET_CHECKPOINT:
      {
        pvars->Targets[i].Moby.Moby = mobyGetFromIdxOrNull((int)pvars->Targets[i].Moby.Moby);
        DLOG(moby, "controller %08X found moby target %d %08X\n", (u32)moby, i, (u32)pvars->Targets[i].Moby.Moby);
        break;
      }
      default: break;
    }
  }

  for (i = 0; i < CONTROLLER_MAX_CONDITIONS; ++i) {
    pvars->Conditions[i].Moby = mobyGetFromIdxOrNull((int)pvars->Conditions[i].Moby);
    DLOG(moby, "controller %08X found condition moby %d %08X\n", (u32)moby, i, (u32)pvars->Conditions[i].Moby);
  }
}

//--------------------------------------------------------------------------
int controllerHandleEvent_SetState(Moby* moby, GuberEvent* event)
{
  int state;
  if (!moby || !moby->PVar)
    return 0;

  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  
	// read event
	guberEventRead(event, &state, 4);
  pvars->Init = 1;
  mobySetState(moby, state, -1);
  return 0;
}

//--------------------------------------------------------------------------
int controllerHandleEvent_Iterate(Moby* moby, GuberEvent* event)
{
  if (!moby || !moby->PVar)
    return 0;

  u32 triggeredByUid;
  int time;
  struct ControllerPVar* pvars = (struct ControllerPVar*)moby->PVar;
  
	guberEventRead(event, &pvars->State.Iterations, 4);
	guberEventRead(event, &triggeredByUid, 4);
	guberEventRead(event, &time, 4);
  
  Guber* triggeredByGuber = guberGetObjectByUID(triggeredByUid);
  if (triggeredByGuber && triggeredByGuber->VTable && triggeredByGuber->VTable->GetMoby) {
    pvars->State.TriggeredByMoby = triggeredByGuber->VTable->GetMoby(triggeredByGuber);
  } else {
    pvars->State.TriggeredByMoby = NULL;
  }
  
	// iterate
  if (!gameAmIHost()) {
    pvars->State.RemoteIterationTime = time;
    controllerIterate(moby);
  }

  DLOG(moby, "controller iterate %d triggeredby:%08X %08X %08X\n", pvars->State.Iterations, triggeredByUid, (u32)triggeredByGuber, (u32)pvars->State.TriggeredByMoby);
  return 0;
}

//--------------------------------------------------------------------------
struct Guber* controllerGetGuber(Moby* moby)
{
	if (moby->OClass == CONTROLLER_OCLASS && moby->PVar)
		return moby->Guber;
	
	return 0;
}

//--------------------------------------------------------------------------
int controllerHandleEvent(Moby* moby, GuberEvent* event)
{
	if (!moby || !event)
		return 0;

	if (isInGame() && !mobyIsDestroyed(moby) && moby->OClass == CONTROLLER_OCLASS && moby->PVar) {
		u32 upgradeEvent = event->NetEvent.EventID;

		switch (upgradeEvent)
		{
      case CONTROLLER_EVENT_SET_STATE: { return controllerHandleEvent_SetState(moby, event); }
      case CONTROLLER_EVENT_ITERATE: { return controllerHandleEvent_Iterate(moby, event); }
			default:
			{
				DLOG(moby, "unhandle controller event %d\n", upgradeEvent);
				break;
			}
		}
	}

	return 0;
}

//--------------------------------------------------------------------------
void controllerStart(void)
{
  controllerInitialized = 1;
}

//--------------------------------------------------------------------------
void controllerInit(void)
{
  Moby* temp = mobySpawn(CONTROLLER_OCLASS, 0);
  if (!temp)
    return;

  // set vtable callbacks
  MobyFunctions* mobyFunctionsPtr = mobyGetFunctions(temp);
  if (mobyFunctionsPtr) {
    mapInstallMobyFunctions(mobyFunctionsPtr);
    DPRINTF("CONTROLLER oClass:%04X mClass:%02X func:%08X getGuber:%08X handleEvent:%08X\n", temp->OClass, temp->MClass, (u32)mobyFunctionsPtr, *(u32*)(mobyFunctionsPtr + 0x04), *(u32*)(mobyFunctionsPtr + 0x14));
  }
  mobyDestroy(temp);
  
  // create gubers for controllers
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, CONTROLLER_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      struct Guber* guber = guberGetOrCreateObjectByMoby(moby, -1, 1);
      DLOG(moby, "found controller %08X %08X\n", (u32)moby, (u32)guber);
      if (guber) {
        controllerOnGuberCreated(moby);
      }
    }

		++moby;
	}

  DPRINTF("controller pvar size %d\n", sizeof(struct ControllerPVar));
}
