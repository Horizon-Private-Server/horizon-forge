#ifndef OBSTACLE_HACKERORB_H
#define OBSTACLE_HACKERORB_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

enum HackerorbState {
	HACKERORB_STATE_CAPTURE_IN_PROGRESS = 1,
	HACKERORB_STATE_CAPTURED = 4,
	HACKERORB_STATE_UNCAPTURED = 5,
};

enum HackerorbEventType {
	HACKERORB_EVENT_SPAWN,
  HACKERORB_EVENT_SET_STATE,
};

struct Guber* hackerorbGetGuber(Moby* moby);
int hackerorbHandleEvent(Moby* moby, GuberEvent* event);
void hackerorbInit(void);

#endif // OBSTACLE_HACKERORB_H
