#ifndef SURVIVAL_UPGRADE_H
#define SURVIVAL_UPGRADE_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>

#define UPGRADE_MOBY_OCLASS (0x01F9)
#define UPGRADE_PICKUP_RADIUS (4)
#define UPGRADE_TOKEN_COST (1)
#define UPGRADE_MAX_USES (15)
#define PLAYER_UPGRADE_COOLDOWN_TICKS (15)

enum UpgradeEventType
{
	UPGRADE_EVENT_SPAWN,
	UPGRADE_EVENT_DESTROY,
	UPGRADE_EVENT_PICKUP
};

struct UpgradePVar
{
	int ItemIdx;
	int Uses;
	int TexId;
	u32 TexColor;
	int MaxUses;
	float Opacity;
	int BakedSpawnIdx;
	struct PartInstance *Particles[4];
};

struct UpgradeSpawnEventArgs
{
	int ItemIdx;
	int BakedSpawnIdx;
};

struct UpgradeDestroyedEventArgs
{
};

struct UpgradePickupEventArgs
{
	int PickedUpByPlayerId;
};

void upgradeSpawn(void);
void upgradeTick(void);
void upgradeInit(void);
struct GuberMoby *upgradeGetGuber(Moby *moby);
int upgradeHandleEvent(Moby *moby, GuberEvent *event);
int upgradeCreate(int bakedSpawnIdx, int itemIdx);
void upgradePickup(Moby *moby, int pickedUpByPlayerId);

#endif // SURVIVAL_UPGRADE_H
