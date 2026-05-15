#include <tamtypes.h>
#include <libdl/game.h>
#include <libdl/utils.h>
#include <libdl/string.h>
#include <libdl/stdio.h>
#include "cgm.h"
#include "cgm_score.h"

/*CUSTOM_FUNC_DECLS*/

// set by map
struct CgmMapConfig MapConfig __attribute__((section(".config"))) = {
		.Magic = MAP_CONFIG_MAGIC,
};

//--------------------------------------------------------------------------
int cgmGetParameter(int paramIdx)
{
	if (paramIdx < 0 || paramIdx >= sizeof(PATCH_INTEROP->GameConfig->forgeCgmConfig.params))
		return 0;

	return PATCH_INTEROP->GameConfig->forgeCgmConfig.params[paramIdx];
}

//--------------------------------------------------------------------------
int cgmDisablePvP_WhoKilledMeHook(Player *player, Moby *moby, int b)
{
	return 0;
}

//--------------------------------------------------------------------------
void cgmDisablePvP(void)
{
	// disable team based holoshield toggling
	// enables players to shoot through eachothers shields
	POKE_U32(0x005A3830, 0x2402FFFF);
	POKE_U32(0x005A3834, 0x14500018);

	// patch who killed me to prevent damaging others
	HOOK_JAL(0x005E07C8, &cgmDisablePvP_WhoKilledMeHook);
	HOOK_JAL(0x005E11B0, &cgmDisablePvP_WhoKilledMeHook);

	// disable targeting players
	POKE_U32(0x005F8A80, 0x10A20002);
	POKE_U32(0x005F8A84, 0x0000102D);
	POKE_U32(0x005F8A88, 0x24440001);
}

//--------------------------------------------------------------------------
void cgmTickFrame(PatchStateContainer_t *gameState)
{
	/*TICK_FRAME_BODY*/
}

//--------------------------------------------------------------------------
void cgmTickGame(PatchStateContainer_t *gameState)
{
	/*TICK_GAME_BODY*/
}

//--------------------------------------------------------------------------
void cgmUpdateGameState(PatchStateContainer_t *gameState)
{
	/*UPDATE_GAME_STATE_BODY*/

	// update game state (used by helga bot for tracking game state)
	if (gameState->UpdateGameState)
	{
		gameState->GameStateUpdate.RoundNumber = 0;
	}

	// pass to cgm score
	cgmScoreUpdateGameState(gameState);
}

//--------------------------------------------------------------------------
void cgmInit(void)
{
	// init map config
	MapConfig.MapFunctions.TickFrame = &cgmTickFrame;
	MapConfig.MapFunctions.TickGame = &cgmTickGame;
	MapConfig.MapFunctions.UpdateGameState = &cgmUpdateGameState;

#ifdef FORGE_CGM_NO_PVP
	cgmDisablePvP();
#endif

	/*INIT_BODY*/
}
