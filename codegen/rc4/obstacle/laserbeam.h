#ifndef OBSTACLE_LASERBEAM_H
#define OBSTACLE_LASERBEAM_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define LASERBEAM_OCLASS                      (0x4009)

enum LaserbeamState {
	LASERBEAM_STATE_DEACTIVATED,
	LASERBEAM_STATE_ACTIVATED,
};

struct LaserbeamPVar
{
  VECTOR Direction;
  float MaxLength;
  float Width;
  float Damage;
  u32 DamageFlags;
  u32 ColorBeam;
  u32 ColorGlow;
  u32 ColorParticleStart;
  u32 ColorParticleEnd;
  int BeamTexId;
  int GlowTexId;
  float Length;
  int Hit;
  Moby* HitMoby;
};

void laserbeamSet(Moby* moby, VECTOR position, VECTOR direction, float maxLength, float width, float damage, u32 damageFlags, u32 colorBeam, u32 colorGlow, u32 colorParticleStart, u32 colorParticleEnd, int beamTexId, int glowTexId);
void laserbeamDestroy(Moby* moby);
Moby* laserbeamCreate(Moby* parent);

#endif // OBSTACLE_LASERBEAM_H
