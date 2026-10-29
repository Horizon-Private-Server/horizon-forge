#include <float.h>
#include <libdl/game.h>
#include <libdl/gamesettings.h>
#include <libdl/string.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/time.h>
#include <libdl/graphics.h>
#include <libdl/camera.h>
#include <libdl/stdio.h>

#include "skillfield.h"
#include "skills.h"
#include "game.h"
#include "utils.h"
#include "ammodrop.h"
#include "maputils.h"
#include "common.h"

#define SKILLFIELD_HILL_OPACITY_MULTIPLIER (0.5)
#define SKILLFIELD_HILL_HEIGHT (1.5)
#define SKILLFIELD_HILL_COLOR_HEALING (0x0000E080)
#define SKILLFIELD_HILL_COLOR_AMMO (0x0000FFFF)
#define SKILLFIELD_HILL_COLOR_ALL (0x00E0E080)

//--------------------------------------------------------------------------
int skillfieldArgsAreValid(struct SkillFieldSpawnArgs* args)
{
  int i;
  for (i = 0; i < 3; ++i) {
    if (!(args->Position[i] >= -FLT_MAX && args->Position[i] <= FLT_MAX)) return 0;
  }

  if (args->Type == SKILLFIELD_TYPE_NONE || (args->Type & ~SKILLFIELD_TYPE_ALL)) return 0;
  if ((args->Type & SKILLFIELD_TYPE_HEALING) && !(args->HealingAmount > 0 && args->HealingAmount <= FLT_MAX))
    return 0;
  if ((args->Type & SKILLFIELD_TYPE_AMMO) && !(args->AmmoAmount > 0 && args->AmmoAmount <= FLT_MAX))
    return 0;

  return args->CasterPlayerId < GAME_MAX_PLAYERS
    && args->CasterClassType < BANK_MAX_CLASSES && args->Radius > 0 && args->Radius <= FLT_MAX
    && args->DurationMs > 0 && args->PulseIntervalMs > 0
    && args->PulseIntervalMs <= args->DurationMs;
}

//--------------------------------------------------------------------------
void skillfieldPulse(Moby* moby)
{
  struct SkillFieldPVar* pvars = (struct SkillFieldPVar*)moby->PVar;
  struct SkillFieldSpawnArgs* config = &pvars->Config;
  int i;
  Player** players = playerGetAll();
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    Player* player = players[i];
    if (!playerIsValid(player) || !player->IsLocal || playerIsDead(player) || player->Health <= 0
        || !(vector_sqrdistance(player->PlayerPosition, moby->Position) <= config->Radius * config->Radius))
      continue;

    if (config->Type & SKILLFIELD_TYPE_HEALING)
      playerSetHealth(player, minf(player->MaxHealth, player->Health + config->HealingAmount));

    if ((config->Type & SKILLFIELD_TYPE_AMMO) && player->GadgetBox) {
      int weaponId = player->WeaponHeldId;
      skillsGiveAmmo(player, weaponId, ammodropGetPickupAmount(player, weaponId) * config->AmmoAmount);
    }
  }
}

//--------------------------------------------------------------------------
u32 skillfieldGetHillColor(Moby* moby, int age)
{
  struct SkillFieldPVar* pvars = (struct SkillFieldPVar*)moby->PVar;
  struct SkillFieldSpawnArgs* config = &pvars->Config;
  if (pvars->Expired || age < 0 || age >= config->DurationMs) return 0;

  u32 color = config->Type == SKILLFIELD_TYPE_HEALING ? SKILLFIELD_HILL_COLOR_HEALING : SKILLFIELD_HILL_COLOR_AMMO;
  if (config->Type == SKILLFIELD_TYPE_ALL)
    color = SKILLFIELD_HILL_COLOR_ALL;

  // Restart at maximum opacity on each scheduled pulse. Shorten the last fade
  // if expiration falls before the next pulse so it still reaches zero.
  int pulseAge = age % config->PulseIntervalMs;
  int pulseDuration = config->DurationMs - (age - pulseAge);
  if (pulseDuration > config->PulseIntervalMs)
    pulseDuration = config->PulseIntervalMs;
  float pulseOpacity = 1 - pulseAge / (float)pulseDuration;
  float radius = config->Radius;
  GameCamera* camera = cameraGetGameCamera(0);
  if (!camera) return 0;

  // Match the hill's distance fade: fully visible within 52 units of the
  // bounding sphere, then fade out over the next 12 units.
  float bound = sqrtf(radius * radius + SKILLFIELD_HILL_HEIGHT * SKILLFIELD_HILL_HEIGHT);
  float distance = vector_distance(camera->pos, moby->Position) - bound;
  float opacity = 1 - clamp((distance - 52) / 12, 0, 1);
  int alpha = (int)(128 * clamp(SKILLFIELD_HILL_OPACITY_MULTIPLIER, 0, 1) * opacity * pulseOpacity);
  return (color & 0xFFFFFF) | (alpha << 24);
}

//--------------------------------------------------------------------------
void skillfieldDrawHillRing(Moby* moby)
{
  // Hill (0x2604) post-draw at 0x00444F20 uses effect texture 0x20 and
  // two vertical triangle strips, fading out above and below the boundary.
  // Supply our own vertices to the engine's DrawStrip overload rather than
  // calling hill post-draw, which indexes the level's active hill cuboid.
  struct SkillFieldPVar* pvars = (struct SkillFieldPVar*)moby->PVar;
  u32 color = pvars->HillColor;
  if (!(color >> 24)) return;

  enum { SEGMENTS = 32, VERTEX_COUNT = (SEGMENTS + 1) * 2 };
  VECTOR positions[VERTEX_COUNT];
  // The packet writer copies colors in 16-byte blocks.
  u32 colors[(VERTEX_COUNT + 3) & ~3] __attribute__((aligned(16))) = {0};
  struct UV uvs[VERTEX_COUNT] __attribute__((aligned(16)));
  float radius = pvars->Config.Radius;
  float boundaryHeight = SKILLFIELD_HILL_HEIGHT / 3;
  float scroll = ((u32)gameGetTime() % (2 * TIME_SECOND)) / (float)(2 * TIME_SECOND);
  u32 rgb = color & 0xFFFFFF;
  int i, pass;
  for (i = 0; i <= SEGMENTS; ++i) {
    int vertex = i * 2;
    float angle = (i % SEGMENTS) * (2 * MATH_PI / SEGMENTS);
    positions[vertex][0] = moby->Position[0] + cosf(angle) * radius;
    positions[vertex][1] = moby->Position[1] + sinf(angle) * radius;
    positions[vertex][2] = moby->Position[2] + boundaryHeight;
    positions[vertex][3] = 1;
    colors[vertex] = color;
    colors[vertex + 1] = rgb;
    uvs[vertex] = (struct UV){16.0f * i / SEGMENTS - 8, scroll};
    uvs[vertex + 1].U = uvs[vertex].U;
  }

  // The native hill's SetTexture and SetBlendMode calls (wrapping UVs).
  ((void (*)(void*, int))0x004C9A58)((void*)0x00222590, 0x20);
  ((void (*)(void*, int, int, int))0x004C97D8)((void*)0x00222590, 0, 1, 1);
  for (pass = 0; pass < 2; ++pass) {
    for (i = 0; i <= SEGMENTS; ++i) {
      int vertex = i * 2;
      vector_copy(positions[vertex + 1], positions[vertex]);
      positions[vertex + 1][2] += pass == 0 ? SKILLFIELD_HILL_HEIGHT - boundaryHeight : -boundaryHeight;
      uvs[vertex + 1].V = scroll + (pass == 0 ? -0.4f : 0.4f);
    }

    // This overload copies the arrays into the draw packet before returning,
    // so the stack buffers can be reused for the second strip/next field.
    ((void (*)(void*, int, VECTOR*, u32*, struct UV*, int, int))0x004C9F08)
      ((void*)0x00222590, VERTEX_COUNT, positions, colors, uvs, 1, 1);
  }
}

//--------------------------------------------------------------------------
void skillfieldPostDraw(Moby* moby)
{
  struct SkillFieldPVar* pvars = (struct SkillFieldPVar*)moby->PVar;
  if (!pvars || !pvars->Initialized || pvars->Expired || mobyIsDestroyed(moby)) return;

  struct SkillFieldSpawnArgs* config = &pvars->Config;
  int i;
  u32 color;
  skillfieldDrawHillRing(moby);

  VECTOR position;
  vector_copy(position, moby->Position);
  position[2] += 1.5f;

  for (i = SKILLFIELD_TYPE_HEALING; i <= SKILLFIELD_TYPE_ALL; i <<= 1) {
    if (!(config->Type & i)) continue;

    enum TextAlign alignment = TEXT_ALIGN_MIDDLECENTER;
    if (config->Type == SKILLFIELD_TYPE_ALL)
      alignment = i == SKILLFIELD_TYPE_HEALING ? TEXT_ALIGN_MIDDLERIGHT : TEXT_ALIGN_MIDDLELEFT;
    color = i == SKILLFIELD_TYPE_HEALING ? 0x8000E080 : (0x80000000 | SKILLFIELD_HILL_COLOR_AMMO);

    gfxHelperDrawSprite_WS(position, 24, 24, 32, 32, i == SKILLFIELD_TYPE_HEALING ? 94 : 90,
      color, alignment, COMMON_DZO_DRAW_NORMAL);
  }
}

//--------------------------------------------------------------------------
void skillfieldUpdate(Moby* moby)
{
  struct SkillFieldPVar* pvars = (struct SkillFieldPVar*)moby->PVar;
  if (!pvars || !pvars->Initialized || !MapConfig.State) return;

  struct SkillFieldSpawnArgs* config = &pvars->Config;
  int age = (int)((u32)gameGetTime() - (u32)config->StartTime);
  // Publish even without a PS2 draw callback so DZO can read the current fade.
  pvars->HillColor = skillfieldGetHillColor(moby, age);
  if (age >= config->DurationMs)
    pvars->Expired = 1;

  if (pvars->Expired) {
    guberMobyDestroy(moby);
    return;
  }
  if (age < 0) return;

  gfxRegisterDrawFunction((void**)0x0022251C, (gfxDrawFuncDef*)&skillfieldPostDraw, moby);
  int pulse = age / config->PulseIntervalMs;
  if (pulse <= pvars->LastPulse) return;

  pvars->LastPulse = pulse; // Late updates skip elapsed pulses rather than applying a burst.
  skillfieldPulse(moby);
}

//--------------------------------------------------------------------------
int skillfieldHandleEvent_Spawn(Moby* moby, GuberEvent* event)
{
  struct SkillFieldPVar* pvars = (struct SkillFieldPVar*)moby->PVar;
  if (pvars->Initialized) return 0;

  struct SkillFieldSpawnArgs args;
  guberEventRead(event, &args, sizeof(args));
  GameSettings* gs = gameGetSettings();
  if (!skillfieldArgsAreValid(&args) || !gs
      || (s8)gs->PlayerClients[args.CasterPlayerId] != event->NetEvent.OriginClientIdx) {
    guberMobyDestroy(moby);
    return 0;
  }

  memset(pvars, 0, sizeof(*pvars));
  memcpy(&pvars->Config, &args, sizeof(args));
  pvars->Initialized = 1;
  pvars->LastPulse = -1;
  pvars->CreatorClientId = event->NetEvent.OriginClientIdx;
  memcpy(moby->Position, args.Position, sizeof(args.Position));
  moby->PUpdate = &skillfieldUpdate;
  moby->ModeBits = MOBY_MODE_BIT_HIDDEN | MOBY_MODE_BIT_NO_POST_UPDATE;
  moby->CollData = NULL;
  moby->CollActive = -1;
  moby->DrawDist = 0;
  moby->UpdateDist = -1;
  mobySetState(moby, 0, -1);
  return 0;
}

//--------------------------------------------------------------------------
struct Guber* skillfieldGetGuber(Moby* moby)
{
  if (moby && moby->OClass == SKILLFIELD_OCLASS && moby->PVar)
    return (Guber*)moby->GuberMoby;

  return NULL;
}

//--------------------------------------------------------------------------
int skillfieldHandleEvent(Moby* moby, GuberEvent* event)
{
  if (!moby || !event || !isInGame() || mobyIsDestroyed(moby)
      || moby->OClass != SKILLFIELD_OCLASS || !moby->PVar)
    return 0;

  switch (event->NetEvent.EventID)
  {
    case SKILLFIELD_EVENT_SPAWN: return skillfieldHandleEvent_Spawn(moby, event);
  }
  return 0;
}

//--------------------------------------------------------------------------
int skillfieldCreate(struct SkillFieldSpawnArgs* args)
{
  if (!MapConfig.State || !args || !skillfieldArgsAreValid(args)) return 0;

  GuberEvent* event = NULL;
  guberMobyCreateSpawned(SKILLFIELD_OCLASS, sizeof(struct SkillFieldPVar), &event, NULL);
  if (event)
    guberEventWrite(event, args, sizeof(*args));
  else
    DPRINTF("failed to create skill field spawn event\n");

  return event != NULL;
}

//--------------------------------------------------------------------------
void skillfieldInit(void)
{
  Moby* temp = mobySpawn(SKILLFIELD_OCLASS, 0);
  if (!temp) return;

  MobyFunctions* functions = mobyGetFunctions(temp);
  if (functions)
    mapInstallMobyFunctions(functions);

  mobyDestroy(temp);
}
