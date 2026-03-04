/***************************************************
 * FILENAME :		launchstream.c
 * 
 * DESCRIPTION :
 * 		Handles logic for the launch stream.
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
#include <libdl/spline.h>
#include <libdl/color.h>
#include <libdl/radar.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include "maputils.h"
#include "launchstream.h"

#if DEBUG
#define DLOG(moby, format, ...) if (((struct LaunchStreamPVar*)moby->PVar)->Log) { DPRINTF("uid:%d " format, (moby)->UID, ##__VA_ARGS__); }
#else
#define DLOG(moby, format, ...) 
#endif

#define GRAVITY_MAG                 (9.81)
#define MAX_SPEED                   (1)

//--------------------------------------------------------------------------
void launchstreamUpdatePlayerTrajectory(Moby* moby, Player* player, struct LaunchStreamPlayerState* playerState)
{
  // struct LaunchStreamPVar* pvars = (struct LaunchStreamPVar*)moby->PVar;
  // float distance = pvars->Speed * MATH_DT;
  // VECTOR delta = {0,0,0,0};

  if (!playerState->Active) return;

  // calculate velocity
  VECTOR velocity = {0,0,0,0};
  memcpy(velocity, playerState->Velocity, 12);
  vector_scale(velocity, velocity, 1 - (0.1 * MATH_DT)); // drag

  VECTOR gravity = {0,0,-GRAVITY_MAG*MATH_DT,0};
  vector_add(velocity, velocity, gravity);

  // save
  memcpy(playerState->Velocity, velocity, 12);

  // clamp max speed
  float velocityMag = vector_length(velocity);
  if (velocityMag > MAX_SPEED) {
    vector_scale(velocity, velocity, MAX_SPEED / velocityMag);
  }

  // hit ground
  if (player->Ground.dist < 0.1 && playerState->Ticks > TPS) {
    playerState->Active = 0;
    DLOG(moby, "player hit ground\n");
  }

  // move player by delta
  //vector_add(player->PlayerPosition, player->PlayerPosition, delta);
  //vector_add(player->PlayerMoby->Position, player->PlayerMoby->Position, delta);
  float* playerVel = (float*)((void*)player + 0x130);
  float* playerExtVel = (float*)((void*)player + 0x140);
  float* playerVelActual = (float*)((void*)player + 0x140);
  float* playerCam = player->Camera->pos;
  //vector_copy(playerVel, delta);

  float stickStrength = *(float*)((void*)player + 0x2e08);
  float stickAngle = *(float*)((void*)player + 0x2e0c);
  VECTOR stick;
  vector_fromyaw(stick, stickAngle);
  vector_scale(stick, stick, stickStrength * player->Speed * 5 * MATH_DT * 0);
  vector_add(playerVel, stick, velocity);
  vector_copy(playerVel, velocity);
  vector_write(playerVelActual, 0);
  vector_write(playerExtVel, 0);
  vector_add(playerCam, playerCam, velocity);

  //DLOG(moby, "stickStr:%f stickAng:%f playerSpeed:%f\n", stickStrength, stickAngle, player->Speed);
  //DLOG(moby, "%d vel %f,%f,%f\n", playerState->Ticks, velocity[0], velocity[1], velocity[2]);

  ++playerState->Ticks;
  if (player->timers.state > 10) {
    player->timers.state = 10;
  }
  if (player->timers.subState > 10) {
    player->timers.subState = 10;
  }
}

//--------------------------------------------------------------------------
void launchstreamGetInitialVelocity(VECTOR out, float pitchDeg, float range)
{
  float yaw = 0;
  float pitch = pitchDeg * MATH_DEG2RAD;
  float sin2Pitch = sinf(2.0 * pitch);
  if (fabsf(sin2Pitch) < 1e-6)
    return;
  
  float speed = sqrtf((range * GRAVITY_MAG) / sin2Pitch);
  float cosPitch = cosf(pitch);

  out[0] = speed * cosPitch * sinf(yaw);
  out[1] = speed * cosPitch * cosf(yaw);
  out[2] = speed * sinf(pitch);
}

//--------------------------------------------------------------------------
void launchstreamBeginPlayerTrajectory(Moby* moby, Player* player, struct LaunchStreamPlayerState* playerState)
{
  struct LaunchStreamPVar* pvars = (struct LaunchStreamPVar*)moby->PVar;

  VECTOR startVelocity;
  launchstreamGetInitialVelocity(startVelocity, pvars->Angle, pvars->Speed);

  MATRIX m;
  matrix_unit(m);
  if (pvars->Omnidirectional) {
    VECTOR norm;
    vector_normalize(norm, player->Velocity);
    float yaw = clampAngle(atan2f(norm[1], norm[0]) - (MATH_PI/2));
    matrix_rotate_z(m, m, yaw);
  } else {
    matrix_rotate_z(m, m, moby->Rotation[2]);
  }

  // rotate start velocity around Z axis
  vector_apply(startVelocity, startVelocity, m);

  playerState->Active = 1;
  playerState->Ticks = 0;
  memcpy(playerState->Velocity, startVelocity, 12);
  playerGetVTable(player)->UpdateState(player, PLAYER_STATE_MOON_JUMP, 1, 0, 0);

  // todo
  // hook player move logic in moonjump dobehavior function
  *(Moby**)((void*)player + 0x1f10) = moby;
}

//--------------------------------------------------------------------------
void launchstreamOnHeroDoJumpBehavior(Player* player)
{
  // call base
  ((void (*)(Player*))0x005fe298)(player);
  
  Moby* launchstreamMoby = *(Moby**)((void*)player + 0x1f10);
  if (launchstreamMoby && !mobyIsDestroyed(launchstreamMoby) && launchstreamMoby->PVar && launchstreamMoby->OClass == LAUNCHSTREAM_OCLASS) {
    struct LaunchStreamPVar* pvars = (struct LaunchStreamPVar*)launchstreamMoby->PVar;
    launchstreamUpdatePlayerTrajectory(launchstreamMoby, player, &pvars->PlayerStates[player->PlayerId]);
  }
}

//--------------------------------------------------------------------------
void launchstreamUpdate(Moby* moby)
{
  struct LaunchStreamPVar* pvars = (struct LaunchStreamPVar*)moby->PVar;

  if (moby->State != 0) return;
  if (pvars->Speed <= 0) return;

  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = playerGetFromIndex(i);
    if (!playerIsValid(player)) continue;

    int pid = player->PlayerId;
    
    // deactivate when player exits state
    if (pvars->PlayerStates[pid].Active && pvars->PlayerStates[pid].Ticks > TPS && player->PlayerState != PLAYER_STATE_MOON_JUMP) {
      pvars->PlayerStates[pid].Active = 0;
    }
    
    if (!pvars->PlayerStates[pid].Active) {
      pvars->PlayerStates[pid].Active = 
           (pvars->CuboidIdx >= 0 && spawnPointIsPointInside(spawnPointGet(pvars->CuboidIdx), player->PlayerPosition, NULL))
        || (pvars->TriggerMoby && player->Ground.pMoby == pvars->TriggerMoby)
        ;

      if (pvars->PlayerStates[pid].Active) {
        launchstreamBeginPlayerTrajectory(moby, player, &pvars->PlayerStates[pid]);
      }
    }
  }
}

//--------------------------------------------------------------------------
void launchstreamInit(void)
{
  // set update function for launchstreams
  Moby* moby = mobyListGetStart();
	while ((moby = mobyFindNextByOClass(moby, LAUNCHSTREAM_OCLASS)))
	{
		if (!mobyIsDestroyed(moby)) {
      struct LaunchStreamPVar* pvars = (struct LaunchStreamPVar*)moby->PVar;
      DPRINTF("found launchstream %08X\n", (u32)moby);
      
      pvars->TriggerMoby = mobyGetFromIdxOrNull((int)pvars->TriggerMoby);
      moby->PUpdate = launchstreamUpdate;
      moby->ModeBits &= ~MOBY_MODE_BIT_NO_UPDATE;
      mobySetState(moby, pvars->DefaultState, -1);
    }

		++moby;
	}

  DPRINTF("launchstream pvar size %d\n", sizeof(struct LaunchStreamPVar));

  HOOK_JAL(0x005F93D0, &launchstreamOnHeroDoJumpBehavior);
}
