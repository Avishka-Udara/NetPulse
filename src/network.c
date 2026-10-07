#include <winsock2.h>
#include <ws2ipdef.h>
#include "network.h"
#include <iphlpapi.h>
#include <netioapi.h>
#include <string.h>
void network_accumulate(Network *n,const AdapterSample *current,unsigned count,uint64_t *down,uint64_t *up) {
    *down=*up=0;
    for(unsigned i=0;i<count;i++)if(n->valid)
        for(unsigned j=0;j<n->count;j++)if(n->previous[j].luid==current[i].luid) {
            uint64_t d,u;counter_delta(n->previous[j].down,n->previous[j].up,current[i].down,current[i].up,&d,&u);
            *down+=d;*up+=u;break;
        }
    memcpy(n->previous,current,count*sizeof(*current));n->count=count;n->valid=1;
}
int network_poll(Network *n,uint64_t selected,uint64_t *down,uint64_t *up) {
    PMIB_IF_TABLE2 table=NULL; *down=*up=0;
    /* A transient API error must not discard bytes since the last good sample. */
    if(GetIfTable2(&table)!=NO_ERROR)return 0;
    PMIB_IPFORWARD_TABLE2 routes=NULL;
    if(!selected && GetIpForwardTable2(AF_UNSPEC,&routes)!=NO_ERROR){FreeMibTable(table);return 0;}
    AdapterSample current[MAX_ADAPTERS]; unsigned count=0;n->active_count=0;
    for(ULONG i=0;i<table->NumEntries && count<MAX_ADAPTERS;i++) {
        MIB_IF_ROW2 *r=&table->Table[i];
        if(r->Type==IF_TYPE_SOFTWARE_LOOPBACK)continue;
        if(selected ? r->InterfaceLuid.Value!=selected : (!r->InterfaceAndOperStatusFlags.HardwareInterface || r->InterfaceAndOperStatusFlags.FilterInterface || r->InterfaceAndOperStatusFlags.EndPointInterface))continue;
        if(!selected) {
            int gateway=0;
            for(ULONG k=0;k<routes->NumEntries;k++) {
                MIB_IPFORWARD_ROW2 *route=&routes->Table[k];
                if(route->InterfaceLuid.Value==r->InterfaceLuid.Value && route->DestinationPrefix.PrefixLength==0 && !route->Loopback && route->ValidLifetime) {gateway=1;break;}
            }
            if(!gateway)continue;
        }
        if(r->OperStatus==IfOperStatusUp)n->active_count++;
        /* Retain disconnected rows: capture their final bytes and avoid losing the
           reconnect interval if their counters continue without a reset. */
        AdapterSample a={r->InterfaceLuid.Value,r->InOctets,r->OutOctets};
        current[count++]=a;
    }
    network_accumulate(n,current,count,down,up);if(routes)FreeMibTable(routes);FreeMibTable(table);return 1;
}
unsigned network_choices(AdapterChoice *out,unsigned capacity) {
    PMIB_IF_TABLE2 table=NULL;unsigned n=0;if(GetIfTable2(&table)!=NO_ERROR)return 0;
    for(ULONG i=0;i<table->NumEntries && n<capacity;i++) {
        MIB_IF_ROW2 *r=&table->Table[i];if(r->Type==IF_TYPE_SOFTWARE_LOOPBACK || r->InterfaceAndOperStatusFlags.FilterInterface || r->InterfaceAndOperStatusFlags.EndPointInterface)continue;
        out[n].luid=r->InterfaceLuid.Value;lstrcpynW(out[n].name,r->Alias,257);n++;
    }
    FreeMibTable(table);return n;
}
