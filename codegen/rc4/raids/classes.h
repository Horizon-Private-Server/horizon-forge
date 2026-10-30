#ifndef RAIDS_CLASSES_H
#define RAIDS_CLASSES_H

#include <tamtypes.h>
#include "skills.h"

#define RAIDS_MAX_CLASSES             (16)
#define RAIDS_MAX_CLASS_UNLOCK_RULES  (8)
#define RAIDS_MAX_CLASS_SKILLS        (16)
#define RAIDS_CLASS_MAX_LEVEL        (10)
#define RAIDS_CLASS_GRID_SIZE         (7)
#define RAIDS_CLASS_GRID_CENTER       (3)

enum RaidsClassType
{
  RAIDS_CLASS_GUNNER = 0,
  RAIDS_CLASS_MELEE = 1,
  RAIDS_CLASS_GRENADIER = 2,
  RAIDS_CLASS_SUPPORT = 3,
  RAIDS_CLASS_SCOUT = 4,
  RAIDS_CLASS_MARKSMAN = 5,
  RAIDS_CLASS_BOMBER = 6,
  RAIDS_CLASS_JUGGERNAUT = 7,
  RAIDS_CLASS_DEMOLITIONIST = 8,
  RAIDS_CLASS_FIELD_MEDIC = 9,
  RAIDS_CLASS_INFILTRATOR = 10,
  RAIDS_CLASS_SHARPSHOOTER = 11,
  RAIDS_CLASS_BRAWLER = 12,
};

typedef struct RaidsClassUnlockRule
{
  u8 ClassType;
  u8 RequiredLevel;
} RaidsClassUnlockRule_t;

typedef struct RaidsClassSkillDef
{
  u8 SkillId;
  u8 MaxRank;
  u8 X;
  u8 Y;
  u8 MinClassLevel; // Zero means no level requirement. Grid neighbors provide access.
  float Multiplier; // Effect strength only; never changes ranks, costs or grid access.
} RaidsClassSkillDef_t;

typedef struct RaidsClassDef
{
  int ClassType;
  int Tier; // Progression tier; individual skill nodes define their effect strength.
  char* Name;
  float ExperienceCurveMultiplier;
  // Vipers, Magma, Arbiter, Fusion, Mines, B6, Flail, Holoshield. Negative disables a weapon.
  s8 WeaponMaxRarities[8];
  int UnlockRulesCount;
  RaidsClassUnlockRule_t UnlockRules[RAIDS_MAX_CLASS_UNLOCK_RULES];
  RaidsClassSkillDef_t Skills[RAIDS_MAX_CLASS_SKILLS];
} RaidsClassDef_t;

const RaidsClassDef_t* classesGetDefinition(int classType);
const RaidsClassDef_t* classesGetDefinitionByIdx(int idx);
int classesGetSkillMaxRank(const RaidsClassDef_t* def, int skillIdx);
int classesGridIsAdjacent(int x, int y, int otherX, int otherY);
int classesValidateDefinitions(void);
void classesInit(void);

#endif // RAIDS_CLASSES_H
