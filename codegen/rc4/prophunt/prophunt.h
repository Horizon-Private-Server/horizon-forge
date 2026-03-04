
#define TEAM_HIDER                (TEAM_BLUE)
#define TEAM_SEEKER               (TEAM_RED)
#define MAX_PROP_CLASSES          (128)

// which group id to use when looking for spawnable props on startup
#ifndef PROP_MOBY_GROUP_ID
#define PROP_MOBY_GROUP_ID        (123)
#endif

// max dist a prop can be to be spawned
#ifndef PROP_MAX_SPAWN_DIST
#define PROP_MAX_SPAWN_DIST       (0x40)
#endif

// prop moby draw distance
#ifndef PROP_MOBY_DRAW_DIST
#define PROP_MOBY_DRAW_DIST       (0x30)
#endif

// prop min scale
#ifndef PROP_SCALE_RAND_MIN
#define PROP_SCALE_RAND_MIN       (2.0/3.0)
#endif

// prop max scale
#ifndef PROP_SCALE_RAND_MAX
#define PROP_SCALE_RAND_MAX       (3.0/2.0)
#endif

// how long after a player idles before playing a sound
#ifndef PROP_SOUND_PERIOD_SEC
#define PROP_SOUND_PERIOD_SEC     (15)
#endif

// each time a sound is played lower period by this many seconds
#ifndef PROP_SOUND_PERIOD_DEC
#define PROP_SOUND_PERIOD_DEC     (3)
#endif

// max props that can be drawn at once
#ifndef MAX_DRAW_PROPS
#define MAX_DRAW_PROPS            (80)
#endif

// max number of props to spawn in the map
#ifndef MAX_SPAWN_PROPS
#define MAX_SPAWN_PROPS           (900)
#endif

// when spawning, the largest possible cluster size
#ifndef PROP_SPAWN_CLUSTER_MIN
#define PROP_SPAWN_CLUSTER_MIN    (1)
#endif

// when spawning, the largest possible cluster size
#ifndef PROP_SPAWN_CLUSTER_MAX
#define PROP_SPAWN_CLUSTER_MAX    (5)
#endif

// how spread out each spawn cluster can be
#ifndef PROP_SPAWN_CLUSTER_SPREAD
#define PROP_SPAWN_CLUSTER_SPREAD (5.0)
#endif

// number of bits to allocate per octant axis
#ifndef TREE_BITS_PER_AXIS
#define TREE_BITS_PER_AXIS        (5)
#endif

// number of bits to use for octant size
// ie if 3 then each octant is 8x8x8 units
#ifndef TREE_OCTANT_SIZE_BITS
#define TREE_OCTANT_SIZE_BITS     (3)
#endif

// propGetOctant() must always return a value from 0 to this number - 1
#define TREE_MAX_OCTANTS          (1 << (TREE_BITS_PER_AXIS * 2))
#define TREE_OCTANT_SIZE          (1 << TREE_OCTANT_SIZE_BITS)
#define TREE_AXIS_BITMASK         ((1 << TREE_BITS_PER_AXIS) - 1)

void propCleanup(void);
void propInit(void);
void propTick(void);
