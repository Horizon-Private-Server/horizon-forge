#include <libdl/camera.h>
#include <libdl/game.h>
#include <libdl/math3d.h>
#include <libdl/moby.h>
#include <libdl/string.h>
#include <libdl/utils.h>
#include <libdl/weapon.h>

#include "visibomb.h"
#include "skills.h"
#include "bank.h"
#include "game.h"
#include "utils.h"

#define VISIBOMB_STATIONARY_SPEED_SQ (0.0001f)
#define VISIBOMB_CAMERA_DISTANCE (1.5f)
#define VISIBOMB_CAMERA_HEIGHT (0.5f)
#define VISIBOMB_LIFETIME_TICKS (5 * TPS)

// Ghidra: Arbiter constructor 0x003F28B8 and flight update 0x003F3798.
// Keep native speed, collision, damage and explosion handling.
struct VisibombRocketVars
{
  u8 Native00[0x50];
  Moby* Target;             // 0x50: native homing target
  u8 Native54[0x10];
  short LifetimeTicks;      // 0x64: native flight countdown
  u8 Native66[0x0A];
  VECTOR Direction;         // 0x70: unit flight direction (speed is separate)
  u8 Native80[0x28];
  Player* Player;           // 0xA8: shooter, including remote players
};

struct VisibombState
{
  Player* Player;
  Moby* Rocket;
  struct VisibombRocketVars* Vars;
  int RocketUID;
  int ClassType;
  int LastSpawnTime;
  int Active;
} visibombStates[GAME_MAX_PLAYERS];

//--------------------------------------------------------------------------
int visibombCanControl(Player* player)
{
  if (!skillsPlayerIsReady(player) || playerIsDead(player) || player->Health <= 0
      || player->InVehicle || player->timers.noInput || player->timers.postTeleportTimer
      || player->WeaponHeldId != WEAPON_ID_ARBITER
      || skillsGetPlayerBonus(player, RAIDS_CLASS_SKILL_VISIBOMB) <= 0)
    return 0;

  if (player->IsLocal && (MapConfig.State->MenuOpen != RAIDS_CUSTOM_MENU_NONE || shouldCloseMenu()))
    return 0;

  // Ground contact rejects jumping/falling, including a jump's apex. Standing
  // still retains gravity velocity (-0.015 Z on ordinary floors), so measure
  // motion perpendicular to gravity. This also supports wall-walking gravity.
  if (!player->Ground.onGood) return 0;
  VECTOR movement;
  vector_projectonplane(movement, player->Velocity, player->Ground.gravity);
  return vector_sqrmag(movement) < VISIBOMB_STATIONARY_SPEED_SQ;
}

//--------------------------------------------------------------------------
int visibombIsActive(struct VisibombState* state, int playerId)
{
  if (!state->Active) return 0;

  Player* player = playerGetFromIndex(playerId);
  Moby* rocket = state->Rocket;
  if (player != state->Player || !visibombCanControl(player)
      || bankGetPlayerEquippedClass(playerId)->ClassType != state->ClassType
      || !rocket || mobyIsDestroyed(rocket) || rocket->UID != state->RocketUID
      || rocket->OClass != MOBY_ID_ARBITER_ROCKET0 || rocket->PVar != state->Vars
      || state->Vars->Player != player || (rocket->State != 0 && rocket->State != 1)) {
    // Cancellation is latched. Stopping or landing cannot reclaim this rocket.
    state->Active = 0;
  }
  return state->Active;
}

//--------------------------------------------------------------------------
void visibombResetPlayer(Player* player)
{
  if (!player || player->PlayerId < 0 || player->PlayerId >= GAME_MAX_PLAYERS) return;
  memset(&visibombStates[player->PlayerId], 0, sizeof(visibombStates[0]));
}

//--------------------------------------------------------------------------
void visibombOnRocketSpawn(Moby* rocket)
{
  // This constructor call updates the native transform; it is not a spawn replacement.
  ((void (*)(Moby*))0x004FE720)(rocket);
  if (!rocket || !rocket->PVar) return;

  struct VisibombRocketVars* vars = (struct VisibombRocketVars*)rocket->PVar;
  Player* player = vars->Player;
  if (!skillsPlayerIsReady(player)) return;

  struct VisibombState* state = &visibombStates[player->PlayerId];
  int now = gameGetTime();
  // Multi-rocket shots create the primary first, then their side rockets in the
  // same gadget update. Only the primary gets guidance and the follow camera.
  if (state->Player == player && state->Rocket && state->LastSpawnTime == now) return;

  state->Player = player;
  state->Rocket = rocket;
  state->Vars = vars;
  state->RocketUID = rocket->UID;
  state->ClassType = bankGetPlayerEquippedClass(player->PlayerId)->ClassType;
  state->LastSpawnTime = now;
  state->Active = visibombCanControl(player);
  if (state->Active) {
    vars->Target = NULL; // Only this rocket relinquishes native target homing.
    if (vars->LifetimeTicks < VISIBOMB_LIFETIME_TICKS)
      vars->LifetimeTicks = VISIBOMB_LIFETIME_TICKS;
  }
}

//--------------------------------------------------------------------------
int visibombRocketOutOfRange(int nativeOutOfRange, Moby* rocket)
{
  if (!nativeOutOfRange) return 0;

  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    struct VisibombState* state = &visibombStates[i];
    if (state->Rocket == rocket && visibombIsActive(state, i)) return 0;
  }
  return nativeOutOfRange;
}

//--------------------------------------------------------------------------
void visibombOrientRocket(int externalId, VECTOR direction, VECTOR position, Player* player)
{
  // Native gravity/external orientation remains the default for every projectile.
  ((void (*)(int, VECTOR, VECTOR, Player*))0x0059F1E0)(externalId, direction, position, player);

  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
    struct VisibombState* state = &visibombStates[i];
    if (!state->Active || direction != state->Vars->Direction || !visibombIsActive(state, i)) continue;

    // The normal camera controls continue updating aim while the render camera
    // follows the rocket. Player sync already replicates this direction.
    VECTOR aim;
    vector_copy(aim, state->Player->CameraForward);
    float lengthSq = vector_sqrmag(aim);
    if (lengthSq > 0.99f && lengthSq < 1.01f)
      vector_normalize(direction, aim);
    return;
  }
}

//--------------------------------------------------------------------------
void visibombCopyCamera(int cameraIndex)
{
  Player* player = playerGetFromSlot(0);
  GameCamera* camera = cameraGetGameCamera(cameraIndex);
  struct VisibombState* state = NULL;
  if (player && player->IsLocal && player->PlayerId >= 0 && player->PlayerId < GAME_MAX_PLAYERS
      && cameraIndex == 0 && player->Camera == camera) {
    struct VisibombState* candidate = &visibombStates[player->PlayerId];
    if (visibombIsActive(candidate, player->PlayerId)) state = candidate;
  }

  VECTOR nativePosition;
  if (state) {
    VECTOR offset;
    vector_copy(nativePosition, camera->pos);
    vector_scale(offset, state->Vars->Direction, -VISIBOMB_CAMERA_DISTANCE);
    vector_add(camera->pos, state->Rocket->Position, offset);
    camera->pos[2] += VISIBOMB_CAMERA_HEIGHT;
  }

  // Ghidra: this builds the render camera and its view matrix from GameCamera.
  // Temporarily changing position preserves native aim, camera modes and blends.
  ((void (*)(int))0x004B2010)(cameraIndex);
  if (state) vector_copy(camera->pos, nativePosition);
}

//--------------------------------------------------------------------------
void visibombTick(void)
{
  int i;
  for (i = 0; i < GAME_MAX_PLAYERS; ++i)
    visibombIsActive(&visibombStates[i], i);
}

//--------------------------------------------------------------------------
void visibombInit(void)
{
  memset(visibombStates, 0, sizeof(visibombStates));

  // All sites are in code.0006 (0x003A1580..0x0062F028), reloaded per map.
  // Call-site wrappers retain native behavior unless this shooter is guiding.
  HOOK_JAL(0x003F316C, &visibombOnRocketSpawn);
  HOOK_JAL(0x003F3DFC, &visibombOrientRocket);
  HOOK_JAL(0x004B23D0, &visibombCopyCamera);
  HOOK_JAL(0x004A9770, &visibombCopyCamera); // Slot 0's render-time camera copy.

  // The native lifetime countdown still expires normally. Its separate range
  // check (90/143 units) must allow guided flight to use the longer countdown.
  // Pass the native range result and s3 (rocket) to the conditional wrapper.
  POKE_U32(0x003F5138, 0x0260282D); // move a1, s3 (original comparison latency slot)
  HOOK_JAL(0x003F5148, &visibombRocketOutOfRange);
  POKE_U32(0x003F514C, 0x0040202D); // move a0, v0 (call delay slot)
  POKE_U32(0x003F5150, 0x104000B7); // beq v0, zero, 0x003F5430 (restore s0 before return)
  POKE_U32(0x003F5154, 0x8E4300B4); // lw v1, 0xB4(s2) (branch delay slot)
}
