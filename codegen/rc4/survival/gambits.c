#include <libdl/utils.h>
#include <libdl/net.h>
#include <libdl/moby.h>
#include <libdl/stdio.h>
#include "shared.h"

extern SurvivalBakedConfig_t bakedConfig;
extern struct MysteryBoxItemWeight MysteryBoxItemProbabilities[];
extern const int MysteryBoxItemProbabilitiesCount;
extern char MysteryBoxRespawnImmediately;
extern GambitDef_t gambitDefs[];
extern const int gambitDefsCount;

struct {
  char FinishedSetup;
  char PrintedGambit;
} GambitsState;

//--------------------------------------------------------------------------
int gambitsGetActiveValue(void)
{
  return PATCH_INTEROP->GameConfig->survivalConfig.gambit;
}

//--------------------------------------------------------------------------
GambitDef_t* gambitsGetActive(void)
{
  int idx = PATCH_INTEROP->GameConfig->survivalConfig.gambit - 1;
  if (idx < 0 || idx >= gambitDefsCount)
    return NULL;

  return &gambitDefs[idx];
}

//--------------------------------------------------------------------------
void gambitsSetupDisableRevives(void)
{
  int i;

  // disable revive items from mystery box
  for (i = 0; i < MysteryBoxItemProbabilitiesCount; ++i) {
    if (MysteryBoxItemProbabilities[i].Item == MYSTERY_BOX_ITEM_REVIVE_TOTEM) {
      MysteryBoxItemProbabilities[i].Probability = 0;
    }
    if (MysteryBoxItemProbabilities[i].Item == MYSTERY_BOX_ITEM_EMP_HEALTH_GUN) {
      MysteryBoxItemProbabilities[i].Probability = 0;
    }
  }
}

//--------------------------------------------------------------------------
void gambitsTickDisableRevives(void)
{
  // remove self revive from players (solo)
  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    if (MapConfig.State->PlayerStates[i].State.Item == MYSTERY_BOX_ITEM_REVIVE_TOTEM) {
      MapConfig.State->PlayerStates[i].State.Item = 0;
    }

    // immediately "kill" downed player
    if (MapConfig.State->PlayerStates[i].ReviveCooldownTicks > 0)
      MapConfig.State->PlayerStates[i].ReviveCooldownTicks = 0;
  }
}

//--------------------------------------------------------------------------
void gambitsSetupDisableVendor(void)
{
  Moby* m = mobyListGetStart();
  while ((m = mobyFindNextByOClass(m, VENDOR_MOBY_OCLASS))) {
    m->DrawDist = 0;
    m->CollActive = -1;
    ++m;
  }
}

//--------------------------------------------------------------------------
void gambitsSetupDisableStackables(void)
{
  int i;

  // disable perk boxes
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, STACK_BOX_OCLASS)))
	{
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      if (guberGetObjectByMoby(moby)) {
        guberMobyDestroy(moby);
      } else {
        mobyDestroy(moby);
      }
    }

		++moby;
	}
}

//--------------------------------------------------------------------------
void gambitsSetupInstantRespawnMysteryBox(void)
{
  MysteryBoxRespawnImmediately = 1;
}

//--------------------------------------------------------------------------
void gambitsTickDisableBank(void)
{
  // disable bank
  if (MapConfig.State->Bankbox) {
    MapConfig.State->Bankbox->DrawDist = 0;
    MapConfig.State->Bankbox->CollActive = -1;
    MapConfig.State->Bankbox->ModeBits |= MOBY_MODE_BIT_DISABLED;
    MapConfig.State->Bankbox = NULL;
  }
}

//--------------------------------------------------------------------------
void gambitsTickDisablePrestigeMachine(void)
{
  // disable
  if (MapConfig.State->PrestigeMachine) {
    MapConfig.State->PrestigeMachine->DrawDist = 0;
    MapConfig.State->PrestigeMachine->CollActive = -1;
    MapConfig.State->PrestigeMachine->ModeBits |= MOBY_MODE_BIT_DISABLED;
    MapConfig.State->PrestigeMachine = NULL;
  }
}

//--------------------------------------------------------------------------
void gambitsSetupSingleWeaponRestriction(int weaponId)
{
  GameOptions* gameOptions = gameGetOptions();
  gameOptions->WeaponFlags.DualVipers = weaponId == WEAPON_ID_VIPERS ? 1 : 0;
  gameOptions->WeaponFlags.MagmaCannon = weaponId == WEAPON_ID_MAGMA_CANNON ? 1 : 0;
  gameOptions->WeaponFlags.Arbiter = weaponId == WEAPON_ID_ARBITER ? 1 : 0;
  gameOptions->WeaponFlags.FusionRifle = weaponId == WEAPON_ID_FUSION_RIFLE ? 1 : 0;
  gameOptions->WeaponFlags.MineLauncher = weaponId == WEAPON_ID_MINE_LAUNCHER ? 1 : 0;
  gameOptions->WeaponFlags.B6 = weaponId == WEAPON_ID_B6 ? 1 : 0;
  gameOptions->WeaponFlags.Holoshield = weaponId == WEAPON_ID_OMNI_SHIELD ? 1 : 0;
  gameOptions->WeaponFlags.Flail = weaponId == WEAPON_ID_FLAIL ? 1 : 0;

  int i;
  for (i = 0; i < GAME_MAX_LOCALS; ++i) {
    Player* player = playerGetFromSlot(i);
    if (!playerIsValid(player)) continue;

    playerGiveWeapon(player->GadgetBox, weaponId, 0, 1);
  }
}

//--------------------------------------------------------------------------
void gambitsSetupInitialBoltsTokens(int bolts, int tokens)
{
  int i;

  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    MapConfig.State->PlayerStates[i].State.Bolts = bolts;
    MapConfig.State->PlayerStates[i].State.CurrentTokens = tokens;
  }
}

//--------------------------------------------------------------------------
void gambitsDropCreate(VECTOR position, enum DropType dropType, int destroyAtTime, int team)
{
  GambitDef_t* gambit = gambitsGetActive();

  // intercept health drops
  if (gambit && gambit->DisableRevives && dropType == DROP_HEALTH) {
    return;
  }

  dropCreate(position, dropType, destroyAtTime, team);
}

//--------------------------------------------------------------------------
void gambitsOnRoundComplete(int roundNo)
{
  GambitDef_t* gambit = gambitsGetActive();
  if (!gambit) return;

  // invoke gambit callback
  if (gambit->CustomOnRoundComplete) gambit->CustomOnRoundComplete(roundNo);

  if (roundNo == gambit->CompleteAfterRound) {
    mapSendSendGambitCompletedMessage(gambitsGetActiveValue());
  }
}

//--------------------------------------------------------------------------
void gambitsSetup(void)
{
  int i;
  GameOptions* gameOptions = gameGetOptions();
  Moby* mStart = mobyListGetStart();

  GambitDef_t* gambit = gambitsGetActive();
  if (!gambit) return;
  if (!MapConfig.State) return;

  // multipliers
  bakedConfig.Difficulty *= gambit->DifficultyMultiplier;
  bakedConfig.BoltMultiplier *= gambit->BoltMultiplier;
  bakedConfig.XpMultiplier *= gambit->XpMultiplier;

  // mob scale factors
  for (i = 0; i < MapConfig.DefaultSpawnParamsCount; ++i) {
    MapConfig.DefaultSpawnParams[i].Config.DamageScale *= gambit->MobDamageScaleMultiplier;
    MapConfig.DefaultSpawnParams[i].Config.SpeedScale *= gambit->MobSpeedScaleMultiplier;
    MapConfig.DefaultSpawnParams[i].Config.HealthScale *= gambit->MobHealthScaleMultiplier;
  }

  if (gambit->DisableRevives) gambitsSetupDisableRevives();
  if (gambit->DisableVendor) gambitsSetupDisableVendor();
  if (gambit->DisableStackables) gambitsSetupDisableStackables();
  if (gambit->InstantRespawnMysteryBox) gambitsSetupInstantRespawnMysteryBox();
  if (gambit->ForceWeaponId) gambitsSetupSingleWeaponRestriction(gambit->ForceWeaponId);
  gambitsSetupInitialBoltsTokens(gambit->InitialBolts, gambit->InitialTokens);
  if (gambit->CustomInit) gambit->CustomInit();

  MapConfig.CreateMobDropFunc = &gambitsDropCreate;
  GambitsState.FinishedSetup = 1;
}

//--------------------------------------------------------------------------
void gambitsTick(void)
{
  GambitDef_t* gambit = gambitsGetActive();
  if (!gambit) return;
  if (!MapConfig.State) return;

  // make sure we've initialized
  if (!GambitsState.FinishedSetup) gambitsSetup();

  // print ready
  if (GambitsState.FinishedSetup && !GambitsState.PrintedGambit && MapConfig.ClientsReady) {
    GambitsState.PrintedGambit = 1;
    mapPrintGambit(gambitsGetActiveValue());
  }

  if (gambit->DisableRevives) gambitsTickDisableRevives();
  if (gambit->DisableBank) gambitsTickDisableBank();
  if (gambit->DisablePrestigeMachine) gambitsTickDisablePrestigeMachine();
  if (gambit->ForceWeaponId) mapEnforceSingleWeaponRestriction(gambit->ForceWeaponId);
  if (gambit->CustomTick) gambit->CustomTick();
}

//--------------------------------------------------------------------------
void gambitsInit(void)
{
  memset(&GambitsState, 0, sizeof(GambitsState));
  gambitsSetup();
}
