#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include "network.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
    Network n={0};uint64_t d,u;
    AdapterSample rows[]={{1,100000,200000},{2,500000,900000}};
    network_accumulate(&n,rows,2,&d,&u);assert(d==0 && u==0);
    rows[0].down+=4096;rows[0].up+=512;rows[1].down+=1024;rows[1].up+=256;
    network_accumulate(&n,rows,2,&d,&u);assert(d==5120 && u==768);
    network_accumulate(&n,rows,2,&d,&u);assert(d==0 && u==0);
    AdapterSample reversed[]={rows[1],rows[0]};reversed[1].up+=100;
    network_accumulate(&n,reversed,2,&d,&u);assert(d==0 && u==100);
    reversed[1].down=10;reversed[1].up+=50;
    network_accumulate(&n,reversed,2,&d,&u);assert(d==0 && u==50);
    reversed[1].down+=30;reversed[1].up=5;
    network_accumulate(&n,reversed,2,&d,&u);assert(d==30 && u==0);
    network_accumulate(&n,reversed,1,&d,&u);assert(d==0 && u==0);
    reversed[1].down+=100000;
    network_accumulate(&n,reversed,2,&d,&u);assert(d==0 && u==0);
    puts("PASS: adapter identity, no duplicate samples, independent resets, added/removed interfaces");
    if(argc<2 || strcmp(argv[1],"--live")!=0)return 0;
    memset(&n,0,sizeof(n));assert(network_poll(&n,0,&d,&u));
    assert(n.count>0);uint64_t selected=n.previous[0].luid;
    MIB_IF_ROW2 before={0},after={0};before.InterfaceLuid.Value=selected;after.InterfaceLuid.Value=selected;
    assert(GetIfEntry2(&before)==NO_ERROR);
    memset(&n,0,sizeof(n));assert(network_poll(&n,selected,&d,&u));
    MIB_IF_ROW2 inner_start={0},inner_end={0};inner_start.InterfaceLuid.Value=selected;inner_end.InterfaceLuid.Value=selected;
    assert(GetIfEntry2(&inner_start)==NO_ERROR);uint64_t sumd=0,sumu=0;
    for(int i=0;i<5;i++){Sleep(1000);assert(network_poll(&n,selected,&d,&u));sumd+=d;sumu+=u;}
    assert(GetIfEntry2(&inner_end)==NO_ERROR);
    assert(network_poll(&n,selected,&d,&u));sumd+=d;sumu+=u;
    assert(GetIfEntry2(&after)==NO_ERROR);
    assert(sumd>=inner_end.InOctets-inner_start.InOctets && sumd<=after.InOctets-before.InOctets);
    assert(sumu>=inner_end.OutOctets-inner_start.OutOctets && sumu<=after.OutOctets-before.OutOctets);
    printf("PASS: live adapter %llu: download=%llu upload=%llu bytes, within independent GetIfEntry2 bounds\n",(unsigned long long)selected,(unsigned long long)sumd,(unsigned long long)sumu);
    return 0;
}
