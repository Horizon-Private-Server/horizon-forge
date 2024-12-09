/***************************************************
 * FILENAME :		laser.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the lasers.
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
#include <libdl/hud.h>
#include "common.h"
#include "maputils.h"
#include "shared.h"
#include "laser.h"
#include "laserbeam.h"
#include "mob.h"
#include "game.h"

#define DLOG(moby, format, ...) if (((struct LaserPVar*)moby->PVar)->Log) { DPRINTF(format, ##__VA_ARGS__); }

//--------------------------------------------------------------------------
void laserUpdateBeam(Moby* moby)
{
  VECTOR dir;
  struct LaserPVar* pvars = (struct LaserPVar*)moby->PVar;
  Moby* beamMoby = pvars->BeamMoby;
  if (!beamMoby) return;

  u32 damageFlags = 0;
  if (pvars->DamageType & LASER_DAMAGE_TYPE_PLAYERS) damageFlags |= 1;
  if (pvars->DamageType & LASER_DAMAGE_TYPE_MOBS) damageFlags |= 0x40;

  float difficulty = MapConfig.State ? MapConfig.State->Difficulty : 1;
  float damage = pvars->Damage * (1 + (MOB_BASE_DAMAGE_SCALE * pvars->DamageScale * difficulty));
  if (pvars->DamageMax > 0 && damage > pvars->DamageMax)
    damage = pvars->DamageMax;

  beamMoby->State = LASERBEAM_STATE_ACTIVATED;
  u32 beamColor = hudGetTeamColor(pvars->TeamColor, 0);
  beamColor = colorSetChannel(colorScale(beamColor, 255.0 / colorGetMax(beamColor)), 3, 0x80);
  u32 glowColor = colorSetChannel(hudGetTeamColor(pvars->TeamColor, 1), 3, 0x80);
  u32 particleColor = glowColor;
  vector_fromyaw(dir, moby->Rotation[2]);

  // update state
  int state = beamMoby->SubState ? LASER_STATE_ON_HIT_MOBY : LASER_STATE_ON;
  if (moby->State != state)
    mobySetState(moby, state, -1);

  laserbeamSet(beamMoby, moby->Position, dir, pvars->BeamLength, pvars->BeamWidth, damage, damageFlags, beamColor, glowColor, 0, particleColor, pvars->BeamTexId, pvars->BeamGlowTexId);
}

//--------------------------------------------------------------------------
void laserOnStateChanged(Moby* moby)
{
  struct LaserPVar* pvars = (struct LaserPVar*)moby->PVar;

}

//--------------------------------------------------------------------------
void laserUpdate(Moby* moby)
{
  int i;
  struct LaserPVar* pvars = (struct LaserPVar*)moby->PVar;

  // detect when state was changed
  if ((moby->Triggers & 1) == 0) {
    laserOnStateChanged(moby);
    moby->Triggers |= 1;
  }

  // create/destroy beam
  if (moby->State == LASER_STATE_OFF) {
    if (pvars->BeamMoby) {
      mobyDestroy(pvars->BeamMoby);
      pvars->BeamMoby = NULL;
    }
  } else {
    if (!pvars->BeamMoby) {
      pvars->BeamMoby = laserbeamCreate(moby);
    }
  }

  if (moby->State == LASER_STATE_OFF)
    return;
  
  moby->Bolts = -1; // indicate to raids mode that we can damage mobys
  laserUpdateBeam(moby);
}

//--------------------------------------------------------------------------
void laserStart(void)
{
  
}

//--------------------------------------------------------------------------
void laserInit(void)
{
  int i;

  // set update functions
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, LASER_OCLASS)))
	{
    struct LaserPVar* pvars = (struct LaserPVar*)moby->PVar;
		if (!mobyIsDestroyed(moby) && moby->PVar) {
      DLOG(moby, "found laser %08X\n", (u32)moby);
      moby->PUpdate = &laserUpdate;
      moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;
      mobySetState(moby, pvars->DefaultState, -1);
    }

		++moby;
	}

  DPRINTF("laser pvar size %d\n", sizeof(struct LaserPVar));
}
