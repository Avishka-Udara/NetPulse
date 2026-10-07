#ifndef NETPULSE_NETWORK_H
#define NETPULSE_NETWORK_H
#include <windows.h>
#include "core.h"
#define MAX_ADAPTERS 128
typedef struct { uint64_t luid,down,up; } AdapterSample;
typedef struct { AdapterSample previous[MAX_ADAPTERS]; unsigned count,active_count; int valid; } Network;
void network_accumulate(Network *n,const AdapterSample *current,unsigned count,uint64_t *down,uint64_t *up);
typedef struct { uint64_t luid; wchar_t name[257]; } AdapterChoice;
int network_poll(Network *n,uint64_t selected,uint64_t *down,uint64_t *up);
unsigned network_choices(AdapterChoice *out,unsigned capacity);
#endif
