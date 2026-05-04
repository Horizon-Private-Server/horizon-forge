#ifndef SURVIVAL_DROP_H
#define SURVIVAL_DROP_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/math3d.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/sound.h>

#define DROP_MOBY_OCLASS (0x1F4)
#define DROP_PICKUP_RADIUS (3)

enum DropEventType
{
	DROP_EVENT_SPAWN,
	DROP_EVENT_DESTROY,
	DROP_EVENT_PICKUP
};

struct PartInstance
{
	char IClass;
	char Type;
	char Tex;
	char Alpha;
	u32 RGBA;
	char Rot;
	char DrawDist;
	short Timer;
	float Scale;
	VECTOR Position;
	int Update[8];
};

struct DropPVar
{
	int ItemIdx;
	int DestroyAtTime;
	int Team;
	char HitGround;
	char OwnerPlayerId;
	char Destroyed;
	int TexId;
	u32 TexColor;
	struct PartInstance *Particles[4];
};

struct DropSpawnEventArgs
{
	int ItemIdx;
	int DestroyAtTime;
	int Team;
	char OwnerPlayerId;
};

struct DropDestroyedEventArgs
{
};

struct DropPickupEventArgs
{
	int PickedUpByPlayerId;
};

int dropGetRandomItem(Moby *mobMoby, int forPlayerId, int gadgetId);
void dropTick(void);
void dropInit(void);
int dropCreate(VECTOR position, int itemIdx, int destroyAtTime, int team);
struct Guber *dropGetGuber(Moby *moby);
int dropHandleEvent(Moby *moby, GuberEvent *event);

#endif // SURVIVAL_DROP_H
