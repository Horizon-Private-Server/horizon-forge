#include <libdl/stdio.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/graphics.h>
#include <libdl/moby.h>
#include <libdl/random.h>
#include <libdl/radar.h>
#include <libdl/color.h>

#include "laserbeam.h"

void* laserbeamSpawnParticle(VECTOR pos, VECTOR vel, int frames, int rgb, int rotSp, int initAlpha, int endAlpha, float initScale, float endScale, float gravity, int texId)
{
  return ((void* (*)(VECTOR pos, VECTOR vel, int frames, int rgb, int rotSp, int initAlpha, int endAlpha, float initScale, float endScale, float gravity, int texId))0x00539240)
    (pos, vel, frames, rgb, rotSp, initAlpha, endAlpha, initScale * 210000.0, endScale * 210000.0, gravity, texId);
}

//--------------------------------------------------------------------------
void laserbeamPostDraw(Moby* moby)
{
  VECTOR up = {0,0,1,0};
  float speed = 5;
  float rot = MATH_PI / 2;
  float t = speed * (gameGetTime() / (float)TIME_SECOND);
  int hit = 0;

  float u0 = 0, u1 = 1;
  float v0 = 0, v1 = 1;
  if (!moby || !moby->PVar)
    return;
    
	struct LaserbeamPVar* pvars = (struct LaserbeamPVar*)moby->PVar;
  volatile float distance = pvars->MaxLength;
  struct QuadDef quad = {
    .Tex1 =  0xFF9000000260,
    .Alpha = 0x00FF00000044,
    .Clamp = 0,
  };

  volatile VECTOR fireFrom, fireTo, fireForward, fireRight, camRight;
  vector_copy(fireForward, pvars->Direction);
  vector_outerproduct(fireRight, fireForward, up);
  vector_copy(fireFrom, moby->Position);

  vector_scale(fireTo, fireForward, distance);
  vector_add(fireTo, fireTo, fireFrom);
  if (CollLine_Fix(fireFrom, fireTo, 0, moby->PParent, 0)) {
    vector_copy(fireTo, CollLine_Fix_GetHitPosition());
    distance = vector_distance(fireTo, fireFrom);
    hit = 1;
  }

	GameCamera* camera = cameraGetGameCamera(0);
	if (!camera)
		return;

  vector_subtract(camRight, camera->pos, fireFrom);
  vector_normalize(camRight, camRight);
  vector_outerproduct(camRight, fireForward, camRight);
  vector_normalize(camRight, camRight);

  // draw impact particles
  VECTOR partVelEmpty = {0,0,0,0};
  VECTOR partVel = {randRange(-1,1),randRange(-1,1),0,0};
  vector_scale(partVel, partVel, MATH_DT * 3);
  laserbeamSpawnParticle(fireTo, partVel, randRangeInt(5, 15), pvars->ColorGlow, randRangeInt(2, 5), 0x7f, 0x7f, 1.0 * randRange(0.4, 0.6), 0.01, 0.00444444, 1);
  char* p1 = (char*)laserbeamSpawnParticle(fireTo, partVel, randRangeInt(5, 15), 0xff20, randRangeInt(2, 5) * -2, 0x30, 0x30, 3.0 * randRange(0.4, 0.6), 0.02, 0.00444444, 0);
  if (p1) {
    p1[2] = 0x1C;
  }

  VECTOR partVel2 = {randRange(-1,1),randRange(-1,1),0,0};
  vector_scale(partVel2, partVel2, MATH_DT * 3);
  p1 = (char*)laserbeamSpawnParticle(fireFrom, partVelEmpty, randRangeInt(5, 15), 0xff20, randRangeInt(2, 5) * -2, 0x30, 0x30, 3.0 * randRange(0.4, 0.6), 0.02, 0.0, 0);
  if (p1) {
    p1[2] = 0x1C;
  }

  // draw  
  int i;
  for (i = 0; i < 2; ++i) {
    u32 color = (i == 0) ? pvars->ColorBeam : pvars->ColorGlow;
    quad.Tex0 = gfxGetEffectTex((i == 0) ? pvars->BeamTexId : pvars->GlowTexId, 1);

    float step = 0.4;
    float fadeFrom = 0.7;
    float d = 0;
    float w = pvars->Width * ((i == 0) ? 0.5 : 1.0);
    while (d < distance) {

      float p = d / distance;
      float pt1 = d / distance;
      float pt2 = (d+step) / distance;
      if (pt2 > 1) pt2 = 1;

      // clamp step
      //if ((distance - d) < step)
      //  step = distance - d;

      // color
      quad.VertexColors[0] = quad.VertexColors[1] = color;
      quad.VertexColors[2] = quad.VertexColors[3] = color;

      float fadeStart = clamp((1 - pt1) * (distance / fadeFrom), 0, 1);
      if (fadeStart < 1) {
        quad.VertexColors[0] = quad.VertexColors[1] = colorLerp(color, color & 0xffffff, 1 - fadeStart);
      }

      float fadeEnd = clamp((1 - pt2) * (distance / fadeFrom), 0, 1);
      if (fadeEnd < 1) {
        quad.VertexColors[2] = quad.VertexColors[3] = colorLerp(color, color & 0xffffff, 1 - fadeEnd);
      }

      // offset uvs
      float rCos = cosf(rot);
      float rSin = sinf(rot);
      v0 = t + p;
      v1 = t + p + 1;
      quad.VertexUVs[0] = (struct UV){u0*rCos - v0*rSin,v0*rCos + u0*rSin};
      quad.VertexUVs[1] = (struct UV){u1*rCos - v0*rSin,v0*rCos + u1*rSin};
      quad.VertexUVs[2] = (struct UV){u0*rCos - v1*rSin,v1*rCos + u0*rSin};
      quad.VertexUVs[3] = (struct UV){u1*rCos - v1*rSin,v1*rCos + u1*rSin};

      // build positions
      VECTOR pOffset;

      vector_scale(pOffset, camRight, w * 0.5);
      vector_lerp(quad.VertexPositions[0], fireFrom, fireTo, pt1);
      vector_subtract(quad.VertexPositions[1], quad.VertexPositions[0], pOffset);
      vector_add(quad.VertexPositions[0], quad.VertexPositions[0], pOffset);

      vector_lerp(quad.VertexPositions[2], fireFrom, fireTo, pt2);
      vector_subtract(quad.VertexPositions[3], quad.VertexPositions[2], pOffset);
      vector_add(quad.VertexPositions[2], quad.VertexPositions[2], pOffset);

      gfxDrawQuad((void*)0x00222590, &quad, 0, 1);

      d += step;
    }
  }
  
  // damage
  if (hit && CollLine_Fix_GetHitMoby() && pvars->Damage) {
    Moby* hitMoby = CollLine_Fix_GetHitMoby();
    MobyColDamageIn in = {
      .DamageFlags = pvars->DamageFlags,
      .DamageHp = pvars->Damage,
      .Damager = moby->PParent,
      .DamageClass = 0,
      .DamageStrength = 1,
      .DamageIndex = moby->PParent ? moby->PParent->OClass : moby->OClass,
      .Flags = 1,
      .Momentum = 0
    };
    mobyCollDamageDirect(hitMoby, &in);
  }
}

//--------------------------------------------------------------------------
void laserbeamUpdate(Moby* moby)
{
  if (!moby || !moby->PVar)
    return;

  if (moby->State == LASERBEAM_STATE_ACTIVATED) {
    gfxRegisterDrawFunction((void**)0x0022251C, &laserbeamPostDraw, moby);
  }
}

//--------------------------------------------------------------------------
void laserbeamSet(Moby* moby, VECTOR position, VECTOR direction, float maxLength, float width, float damage, u32 damageFlags, u32 colorBeam, u32 colorGlow, int beamTexId, int glowTexId)
{
  if (!moby || !moby->PVar) return;

  struct LaserbeamPVar* pvars = (struct LaserbeamPVar*)moby->PVar;

  vector_copy(moby->Position, position);
  vector_copy(pvars->Direction, direction);
  
  pvars->MaxLength = maxLength;
  pvars->Width = width;
  pvars->Damage = damage;
  pvars->DamageFlags = damageFlags;
  pvars->ColorBeam = colorBeam;
  pvars->ColorGlow = colorGlow;
  pvars->BeamTexId = beamTexId;
  pvars->GlowTexId = glowTexId;
}

//--------------------------------------------------------------------------
void laserbeamDestroy(Moby* moby)
{
  if (!moby) return;
  
  mobyDestroy(moby);
}

//--------------------------------------------------------------------------
Moby* laserbeamCreate(Moby* parent)
{
  Moby* moby = mobySpawn(LASERBEAM_OCLASS, sizeof(struct LaserbeamPVar));
  if (moby) {
    moby->PUpdate = &laserbeamUpdate;
    moby->ModeBits &= ~(MOBY_MODE_BIT_NO_UPDATE);
    moby->UpdateDist = 128;
    moby->PParent = parent;
  }

  DPRINTF("laserbeam %08X\n", (u32)moby);
  return moby;
}
