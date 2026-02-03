#ifndef SURVIVAL_BLESSINGS_H
#define SURVIVAL_BLESSINGS_H

#define PLAYER_MAX_BLESSINGS (4)
#define ITEM_BLESSING_HEALTH_REGEN_RATE_TPS (TPS * 0.2)
#define ITEM_BLESSING_AMMO_REGEN_RATE_TPS (TPS * 5)
#define ITEM_BLESSING_THORN_DAMAGE_FACTOR (0.2)
#define ITEM_BLESSING_MULTI_JUMP_COUNT (5)

enum BlessingItemId
{
	BLESSING_ITEM_NONE = 0,
	BLESSING_ITEM_MULTI_JUMP = 1,
	BLESSING_ITEM_LUCK = 2,
	BLESSING_ITEM_BULL = 3,
	BLESSING_ITEM_ELEM_IMMUNITY = 4,
	BLESSING_ITEM_HEALTH_REGEN = 5,
	BLESSING_ITEM_AMMO_REGEN = 6,
	BLESSING_ITEM_THORNS = 7,
	BLESSING_ITEM_COUNT
};

void blessingsSetPlayerSlots(int playerId, int slots);
int blessingsGetPlayerSlots(int playerId);
void blessingsSetPlayerBlessingAt(int playerId, int slot, int blessing);
int blessingsGetPlayerBlessingAt(int playerId, int slot);
int blessingsPlayerHasBlessing(int playerId, int blessing);
void blessingsMobReactToThorns(Moby *moby, float damage, int byPlayerId);

#endif // SURVIVAL_BLESSINGS_H
