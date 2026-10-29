#ifndef RAIDS_SKILLFIELD_H
#define RAIDS_SKILLFIELD_H

#include <stddef.h>
#include <tamtypes.h>
#include <libdl/player.h>
#include <libdl/guber.h>

#define SKILLFIELD_OCLASS (0x4013)

enum SkillFieldType
{
  SKILLFIELD_TYPE_NONE = 0,
  SKILLFIELD_TYPE_HEALING = (1 << 0),
  SKILLFIELD_TYPE_AMMO = (1 << 1),
  SKILLFIELD_TYPE_ALL = SKILLFIELD_TYPE_HEALING | SKILLFIELD_TYPE_AMMO
};

enum SkillFieldEventType
{
  SKILLFIELD_EVENT_SPAWN
};

// Sent in the Guber spawn event. Keep room for the engine's spawn header.
struct SkillFieldSpawnArgs
{
  float Position[3];
  float Radius;
  float HealingAmount;
  float AmmoAmount;
  int StartTime;
  int DurationMs;
  int PulseIntervalMs;
  u8 CasterPlayerId;
  u8 CasterClassType;
  u8 Team;
  u8 Type;
};

typedef char SkillFieldSpawnArgsSizeCheck[sizeof(struct SkillFieldSpawnArgs) == 40 ? 1 : -1];

struct SkillFieldPVar
{
  struct SkillFieldSpawnArgs Config;
  int Initialized;
  int LastPulse;
  int CreatorClientId;
  int Expired;
  u32 HillColor; // +0x38: current 0xAABBGGRR for DZO; alpha 0x80 = full opacity.
};

typedef char SkillFieldHillColorOffsetCheck[offsetof(struct SkillFieldPVar, HillColor) == 0x38 ? 1 : -1];

int skillfieldCreate(struct SkillFieldSpawnArgs* args);
struct Guber* skillfieldGetGuber(Moby* moby);
int skillfieldHandleEvent(Moby* moby, GuberEvent* event);
void skillfieldInit(void);

#endif // RAIDS_SKILLFIELD_H
