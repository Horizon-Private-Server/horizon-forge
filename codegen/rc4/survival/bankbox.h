#ifndef SURVIVAL_BANK_BOX_H
#define SURVIVAL_BANK_BOX_H

#include <tamtypes.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/time.h>
#include <libdl/player.h>
#include <libdl/math3d.h>

#define BANK_BOX_OCLASS                         (0x01F7)
#define BANK_BOX_AMOUNT                         (10000)
#define BANK_BOX_MAX_DIST                       (5)
#define BANK_VEST_FACTOR                        (0.05)
#define BANK_MAX_BOLTS                          (99999999)
#define BANK_MAX_VEST                           (500000)
#define PLAYER_BANK_BOX_COOLDOWN_TICKS          (5)

struct BankBoxPVar
{
  int TotalBolts;
  int BoltsAtStartOfRound;
  int BoltsWithdrawnThisRound;
  int BoltsDepositThisRound;
};

void bboxSpawn(void);
void bboxInit(void);

#endif // SURVIVAL_BANK_BOX_H
