#ifndef SURVIVAL_MAP_ITEMS_H
#define SURVIVAL_MAP_ITEMS_H

#include "game.h"
#include "mob.h"

void mapOnItemApply_PlayerSpeed(int defIdx, SurvivalItemDef_t *def, Player *player);
void mapOnItemApply_PlayerHealth(int defIdx, SurvivalItemDef_t *def, Player *player);
void mapOnItemApply_PlayerDamage(int defIdx, SurvivalItemDef_t *def, Player *player, Moby *sourceMoby, Moby *mobMoby, struct MobDamageEventArgs *args);
void mapOnItemApply_PlayerCrit(int defIdx, SurvivalItemDef_t *def, Player *player, Moby *sourceMoby, Moby *mobMoby, struct MobDamageEventArgs *args);

#endif // SURVIVAL_MAP_ITEMS_H
