#ifndef SURVIVAL_VENDOR_H
#define SURVIVAL_VENDOR_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define VENDOR_MOBY_OCLASS (0x263A)

enum VendorState
{
	VENDOR_STATE_ACTIVATED,
	VENDOR_STATE_DEACTIVATED
};

struct VendorPVar
{
	char DefaultState;
};

void vendorInit(void);

#endif // SURVIVAL_VENDOR_H
