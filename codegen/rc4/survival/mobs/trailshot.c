#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/graphics.h>
#include <libdl/moby.h>
#include <libdl/random.h>
#include <libdl/radar.h>
#include <libdl/color.h>

#include "game.h"
#include "trailshot.h"
#include "mob.h"
#include "utils.h"
#include "maputils.h"
#include "shared.h"

VECTOR trailshotCenterOffset = {0, 0, 1, 0};
int trailshotGlobalIndex = 0;

typedef struct TrailshotPVar
{
	VECTOR Velocity;
	VECTOR TrailParticlePositions[TRAILSHOT_MAX_TRAIL_PARTICLES];
	int Id;
	int ShotLifeTicks;
	int TrailParticleLifeTicks;
	float DistanceSinceLastTrailParticle;
	float Damage;
	int TrailParticleCount;
} TrailshotPVar_t;

enum TrailshotState
{
	TRAILSHOT_ACTIVE,
	TRAILSHOT_WAITING_TO_DIE
};

SoundDef trailshotSoundDef =
		{
				0.0,	 // MinRange
				50.0,	 // MaxRange
				100,	 // MinVolume
				4000,	 // MaxVolume
				0,		 // MinPitch
				0,		 // MaxPitch
				0,		 // Loop
				0x10,	 // Flags
				0x106, // Id
				3			 // Bank
};

//--------------------------------------------------------------------------
void trailshotDamage(Moby *moby, Moby *hitMoby, int damageFlags, float amount)
{
	MobyColDamageIn in;

	vector_write(in.Momentum, 0);
	in.Damager = moby;
	in.DamageFlags = damageFlags;
	in.DamageClass = 0;
	in.DamageStrength = 1;
	in.DamageIndex = moby->OClass;
	in.Flags = 1;
	in.DamageHp = amount;

	mobyCollDamageDirect(hitMoby, &in);
}

//--------------------------------------------------------------------------
void trailshotPlayFireSound(Moby *moby)
{
	// if (trailshotFireSoundId < 0) return;

	// trailshotSoundDef.Index = trailshotFireSoundId;
	// soundPlay(&trailshotSoundDef, 0, moby, 0, 0x400);
}

//--------------------------------------------------------------------------
void trailshotPlayExplosionSound(Moby *moby)
{
	// if (trailshotExplodeSoundId < 0) return;

	// trailshotSoundDef.Index = trailshotExplodeSoundId;
	// soundPlay(&trailshotSoundDef, 0, moby, 0, 0x400);
}

//--------------------------------------------------------------------------
void trailshotOnImpact(Moby *moby)
{
	TrailshotPVar_t *pvars = (TrailshotPVar_t *)moby->PVar;

	u32 color = moby->PrimaryColor;
	float expScale = 1.5;
	float damageRadius = 1.5;
	Moby *mob = moby->PParent;

	// SpawnMoby_5025
	trailshotPlayExplosionSound(moby);
	mobySpawnExplosion(
			vector_read(moby->Position), 0, 0, 5, 0, 5, 5, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			color, color, color, color, color, color, color, color, color,
			0, 0, moby->PParent, 0,
			expScale, 0, 0, expScale);

	// damage
	if (CollMobysSphere_Fix(moby->Position, COLLISION_FLAG_IGNORE_NONE, mob, NULL, damageRadius) > 0)
	{
		Moby **hitMobies = CollMobysSphere_Fix_GetHitMobies();
		Moby *hitMoby;
		while ((hitMoby = *hitMobies++))
		{
			if (mobyIsMob(hitMoby))
				continue;

			trailshotDamage(mob, hitMoby, 0x00008841, pvars->Damage);
		}
	}
}

//--------------------------------------------------------------------------
void trailshotDrawTrailParticles(Moby *moby)
{
	int i, j;
	VECTOR velocity = {0, 0, 0.1, 0};
	VECTOR pos;
	float scale = 1.0;
	TrailshotPVar_t *pvars = (TrailshotPVar_t *)moby->PVar;
	Moby *mob = moby->PParent;

	for (i = 0; i < pvars->TrailParticleCount; ++i)
	{

		// check for hits
		if (CollMobysSphere_Fix(pvars->TrailParticlePositions[i], COLLISION_FLAG_IGNORE_NONE, moby->PParent, NULL, scale * 0.5) > 0)
		{
			Moby **hitMobies = CollMobysSphere_Fix_GetHitMobies();
			Moby *hitMoby;
			while ((hitMoby = *hitMobies++))
			{
				if (!mobyIsMob(hitMoby))
				{
					trailshotDamage(mob, hitMoby, 0x00008841, TRAILSHOT_TRAIL_DAMAGE);
				}
			}
		}

		// draw
		j = 0;
		do
		{
			pos[0] = randRange(-1, 1) * 0.5;
			pos[1] = randRange(-1, 1) * 0.5;
			pos[2] = 0;
			vector_add(pos, pos, pvars->TrailParticlePositions[i]);

			((void (*)(float, float, float, VECTOR position, VECTOR velocity, int life, u32 color, int spinSpeed, int emitCount, int t2, int texId))0x00539328)(scale * 210000, scale * 210000, 0, pos, velocity, 10, moby->PrimaryColor, 3, 20, 0, 13);
			++j;
		} while (j < 2);
	}
}

//--------------------------------------------------------------------------
void trailshotUpdateTrailParticles(Moby *moby)
{
	TrailshotPVar_t *pvars = (TrailshotPVar_t *)moby->PVar;

	if (pvars->DistanceSinceLastTrailParticle > 2.0 && pvars->TrailParticleCount < TRAILSHOT_MAX_TRAIL_PARTICLES)
	{
		vector_subtract(pvars->TrailParticlePositions[pvars->TrailParticleCount], moby->Position, trailshotCenterOffset);
		pvars->TrailParticleCount++;
		pvars->DistanceSinceLastTrailParticle = 0;
	}

	// register draw function
	if (pvars->TrailParticleCount > 0)
	{
		gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&trailshotDrawTrailParticles, moby);
	}
}

//--------------------------------------------------------------------------
void trailshotPostDraw(Moby *moby)
{
	int i = 0;
	float scale = 0.65 + randRange(-0.1, 0.1);
	VECTOR velocity = {0, 0, 0, 0};

	do
	{
		VECTOR offset;
		vector_copy(offset, moby->Position);
		offset[0] += randRange(-1, 1) * 0.25;
		offset[1] += randRange(-1, 1) * 0.25;
		offset[2] += randRange(-1, 1) * 0.5;

		((void (*)(float, float, float, VECTOR position, VECTOR velocity, int life, u32 color, int spinSpeed, int emitCount, int t2, int texId))0x00539328)(
				scale * 210000, scale * 210000, 0, offset, velocity, 10, moby->PrimaryColor, 0x23, 100, 0, 6);

		++i;
	} while (i < 6);
}

//--------------------------------------------------------------------------
void trailshotUpdate(Moby *moby)
{
	VECTOR next, down;
	VECTOR hitNormal, velNormal;
	VECTOR delta;
	TrailshotPVar_t *pvars = (TrailshotPVar_t *)moby->PVar;

	// destroy if we're old
	if (pvars->Id < (trailshotGlobalIndex - TRAILSHOT_MAX_ACTIVE_AT_ONCE))
	{
		mobyDestroy(moby);
		return;
	}

	// destroy if parent is destroy
	if (mobyIsDestroyed(moby->PParent))
	{
		mobyDestroy(moby);
		return;
	}

	//
	--pvars->TrailParticleLifeTicks;
	trailshotUpdateTrailParticles(moby);

	if (moby->State == TRAILSHOT_WAITING_TO_DIE)
	{
		if (pvars->TrailParticleCount <= 0 || pvars->TrailParticleLifeTicks <= 0)
		{
			mobyDestroy(moby);
		}

		return;
	}

	// register draw function
	gfxRegisterDrawFunction((void **)0x0022251C, (gfxDrawFuncDef *)&trailshotPostDraw, moby);

	// move
	if (pvars->ShotLifeTicks)
	{
		pvars->ShotLifeTicks--;

		vector_add(next, moby->Position, pvars->Velocity);
		weaponTurnOnHoloshields(-1);
		int hit = CollLine_Fix(moby->Position, next, COLLISION_FLAG_IGNORE_NONE, moby->PParent, NULL);
		weaponTurnOffHoloshields();

		if (hit)
		{
			vector_copy(next, CollLine_Fix_GetHitPosition());

			// get hit wall slope
			vector_normalize(hitNormal, CollLine_Fix_GetHitNormal());
			vector_normalize(velNormal, pvars->Velocity);
			float slope = getSignedSlope(velNormal, hitNormal);
			if (fabsf(slope) > (25 * MATH_DEG2RAD))
			{
				trailshotOnImpact(moby);
				mobySetState(moby, TRAILSHOT_WAITING_TO_DIE, -1);
				return;
			}
		}

		vector_subtract(down, next, trailshotCenterOffset);
		if (CollLine_Fix(next, down, COLLISION_FLAG_IGNORE_NONE, moby->PParent, NULL))
		{
			vector_add(next, CollLine_Fix_GetHitPosition(), trailshotCenterOffset);
		}

		vector_subtract(delta, next, moby->Position);
		pvars->DistanceSinceLastTrailParticle += vector_length(delta);

		vector_copy(moby->Position, next);
	}
	else
	{
		mobySetState(moby, TRAILSHOT_WAITING_TO_DIE, -1);
	}
}

//--------------------------------------------------------------------------
void trailshotSpawn(Moby *creatorMoby, VECTOR position, VECTOR velocity, u32 color, float damage, int lifeTicks)
{
	Moby *moby = mobySpawn(0x01F8, sizeof(TrailshotPVar_t));
	if (!moby)
		return;

	mobySetState(moby, TRAILSHOT_ACTIVE, -1);

	TrailshotPVar_t *pvars = (TrailshotPVar_t *)moby->PVar;

	moby->PUpdate = &trailshotUpdate;
	moby->PrimaryColor = color;
	moby->UpdateDist = 0xFF;
	moby->ModeBits = 0x04;
	moby->Opacity = 0x80;
	moby->PParent = creatorMoby;
	vector_copy(moby->Position, position);

	pvars->Id = trailshotGlobalIndex++;
	pvars->ShotLifeTicks = lifeTicks;
	pvars->TrailParticleLifeTicks = TPS * 60 * 1;
	pvars->Damage = damage;
	pvars->DistanceSinceLastTrailParticle = 0;
	vector_copy(pvars->Velocity, velocity);

	trailshotPlayFireSound(moby);
}
