#ifndef SURVIVAL_MYSTERY_BOX_H
#define SURVIVAL_MYSTERY_BOX_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define MYSTERY_BOX_OCLASS                      (0x2635)
#define MYSTERY_BOX_COST                        (10000)
#define MYSTERY_BOX_COST_PER_VOX                (5000)
#define MYSTERY_BOX_CYCLE_ITEMS_DURATION        (TIME_SECOND * 3)
#define MYSTERY_BOX_MAX_LOCATIONS               (32)
#define PLAYER_MYSTERY_BOX_COOLDOWN_TICKS       (30)

enum MysteryBoxEventType {
	MYSTERY_BOX_EVENT_SPAWN,
  MYSTERY_BOX_EVENT_ACTIVATE,
  MYSTERY_BOX_EVENT_GIVE_PLAYER,
};

enum MysteryBoxState {
	MYSTERY_BOX_STATE_IDLE,
	MYSTERY_BOX_STATE_OPENING,
	MYSTERY_BOX_STATE_CYCLING_ITEMS,
	MYSTERY_BOX_STATE_DISPLAYING_ITEM,
	MYSTERY_BOX_STATE_BEFORE_CLOSING,
	MYSTERY_BOX_STATE_CLOSING,
	MYSTERY_BOX_STATE_HIDDEN,
};

enum MysteryBoxItem {
	MYSTERY_BOX_ITEM_WEAPON_MOD,
	MYSTERY_BOX_ITEM_ACTIVATE_POWER,
	MYSTERY_BOX_ITEM_UPGRADE_WEAPON,
	MYSTERY_BOX_ITEM_DREAD_TOKEN,
	MYSTERY_BOX_ITEM_INVISIBILITY_CLOAK,
	MYSTERY_BOX_ITEM_REVIVE_TOTEM,
	MYSTERY_BOX_ITEM_INFINITE_AMMO,
	MYSTERY_BOX_ITEM_RESET_GATE,
	MYSTERY_BOX_ITEM_TEDDY_BEAR,
  MYSTERY_BOX_ITEM_QUAD,
  MYSTERY_BOX_ITEM_SHIELD,
  MYSTERY_BOX_ITEM_EMP_HEALTH_GUN,
  MYSTERY_BOX_ITEM_RANDOMIZE_WEAPON_PICKUPS,
  MYSTERY_BOX_ITEM_COUNT
};

struct MysteryBoxPVar
{
  VECTOR SpawnpointPosition;
  VECTOR SpawnpointRotation;
  enum MysteryBoxItem Item;
  enum MysteryBoxItem CycleItem;
  float BoltCostMultiplier;
  int Random;
  int ActivatedTime;
  int StateChangedAtTime;
  int TicksSinceLastStateChanged;
  int ActivatedByPlayerId;
  int RoundHidden;
  int ItemTexId;
  int NumVoxPerPlayer[GAME_MAX_PLAYERS];
};

struct MysteryBoxItemWeight
{
  enum MysteryBoxItem Item;
  float Probability;
};

void mboxSpawn(void);
void mboxInit(void);
struct GuberMoby* mboxGetGuber(Moby* moby);
int mboxHandleEvent(Moby* moby, GuberEvent* event);

#endif // SURVIVAL_MYSTERY_BOX_H
