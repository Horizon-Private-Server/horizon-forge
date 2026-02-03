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

enum DropType
{
	DROP_NUKE,
	DROP_AMMO,
	DROP_DOUBLE_POINTS,
	DROP_DOUBLE_XP,
	DROP_FREEZE,
	DROP_HEALTH,
	DROP_COUNT
};

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
	enum DropType Type;
	int DestroyAtTime;
	int Team;
	char HitGround;
	char Owner;
	char Destroyed;
	int TexId;
	struct PartInstance *Particles[4];
};

struct DropSpawnEventArgs
{
	enum DropType Type;
	int DestroyAtTime;
	int Team;
	char Owner;
};

struct DropDestroyedEventArgs
{
};

struct DropPickupEventArgs
{
	int PickedUpByPlayerId;
};

void dropTick(void);
void dropInit(void);
int dropCreate(VECTOR position, enum DropType dropType, int destroyAtTime, int team);
struct GuberMoby *dropGetGuber(Moby *moby);
int dropHandleEvent(Moby *moby, GuberEvent *event);

#endif // SURVIVAL_DROP_H
