#ifndef RAIDS_WALLWALK_H
#define RAIDS_WALLWALK_H

#include <libdl/player.h>

int wallwalkIsActive(Player* player);
float wallwalkGetCooldown(Player* player);
void wallwalkResetPlayer(Player* player);
void wallwalkUpdate(Player* player, int classType, float durationSeconds, int canControl);
void wallwalkInit(void);
void wallwalkCleanup(void);

#endif // RAIDS_WALLWALK_H
