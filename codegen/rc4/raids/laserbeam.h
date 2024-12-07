#ifndef RAIDS_LASERBEAM_H
#define RAIDS_LASERBEAM_H

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
  int BeamTexId;
  int GlowTexId;
  Moby* ParticleStartMoby;
  Moby* ParticleEndMoby;
};

void laserbeamSet(Moby* moby, VECTOR position, VECTOR direction, float maxLength, float width, float damage, u32 damageFlags, u32 colorBeam, u32 colorGlow, int beamTexId, int glowTexId);
void laserbeamDestroy(Moby* moby);
Moby* laserbeamCreate(Moby* parent);

#endif // RAIDS_LASERBEAM_H
