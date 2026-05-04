#ifndef SURVIVAL_POOL_H
#define SURVIVAL_POOL_H

typedef struct PoolGroup
{
  int MobyOClass;
  int PoolSize;
  int PoolIndex;
  int PoolStart;
} PoolGroup_t;

void poolTick(void);
void poolInit(void);

#endif // SURVIVAL_POOL_H
