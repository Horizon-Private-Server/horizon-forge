#ifndef RAIDS_VENDOR_H
#define RAIDS_VENDOR_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define VENDOR_OCLASS                           (0x263A)
#define VENDOR_INTERACT_RADIUS                  (4)

void vendorStart(void);
void vendorInit(void);

#endif // RAIDS_VENDOR_H
