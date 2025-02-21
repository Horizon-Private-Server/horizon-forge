#ifndef RAIDS_LASER_H
#define RAIDS_LASER_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define LASER_OCLASS                         (0x400A)

enum LaserState {
	LASER_STATE_OFF,
	LASER_STATE_ON,
	LASER_STATE_ON_HIT_MOBY,
};

enum LaserDamageType {
	LASER_DAMAGE_TYPE_NONE = 0,
	LASER_DAMAGE_TYPE_MOBS = 1,
	LASER_DAMAGE_TYPE_PLAYERS = 2
};

struct LaserPVar
{
  char DefaultState;
  char Log;
  char TeamColor;
  char DamageType;
  int BeamTexId;
  int BeamGlowTexId;
  float BeamLength;
  float BeamWidth;
  float Damage;
  float DamageMax;
  float DamageScale;
  Moby* BeamMoby;
};

void laserStart(void);
void laserInit(void);

#endif // RAIDS_LASER_H
