#define _CRT_SECURE_NO_WARNINGS
#include "db.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    Database d;Usage u={0},got;assert(db_open(&d,L":memory:"));
    u.down[0]=100;u.up[0]=25;u.down[1]=50;u.up[1]=10;
    assert(db_add(&d,120,&u));assert(db_totals(&d,120,180,&got));assert(got.down[0]+got.up[0]==125);
    assert(db_flush(&d,500));assert(d.count==0);assert(db_flush(&d,600));
    assert(db_totals(&d,120,180,&got));assert(got.down[0]+got.up[0]==125 && got.down[1]+got.up[1]==60);
    assert(db_add(&d,181,&u));assert(db_totals(&d,180,240,&got));assert(got.down[0]+got.up[0]==125);
    assert(db_flush(&d,700));assert(db_correct(&d,190,120,240,0.5,0.25));
    assert(db_totals(&d,120,240,&got));assert(got.down[0]==500000000 && got.down[1]==250000000);
    assert(db_correct(&d,191,120,240,0,0));assert(db_totals(&d,120,240,&got));assert(got.down[0]==0 && got.down[1]==0);
    assert(db_add(&d,200,&u));assert(db_flush(&d,800));assert(db_totals(&d,120,240,&got));assert(got.down[0]==125 && got.down[1]==60);
    assert(db_totals(&d,240,300,&got));assert(got.down[0]==0);
    /* A failed write keeps pending data, and retry commits it exactly once. */
    assert(db_add(&d,250,&u));assert(sqlite3_exec(d.handle,"PRAGMA query_only=ON",0,0,0)==SQLITE_OK);
    assert(!db_flush(&d,900));assert(d.count==1);assert(sqlite3_exec(d.handle,"PRAGMA query_only=OFF",0,0,0)==SQLITE_OK);
    assert(db_flush(&d,1000));assert(d.count==0);assert(db_totals(&d,240,300,&got));assert(got.down[0]==125);
    uint64_t down,up;assert(db_traffic(&d,120,240,&down,&up));assert(down==450 && up==105);
    assert(db_traffic(&d,240,300,&down,&up));assert(down==150 && up==35);
    assert(db_add(&d,300,&u));assert(db_traffic(&d,300,360,&down,&up));assert(down==150 && up==35);
    assert(db_traffic(&d,360,420,&down,&up));assert(down==0 && up==0);
    db_close(&d);
    /* Exact known transfers survive flush/restart, exclude yesterday, and are
       unaffected by provider quota corrections. No real user database is used. */
    wchar_t path[MAX_PATH];assert(GetTempFileNameW(L"build",L"npt",0,path));
    assert(db_open(&d,path));
    Usage transfer={0};transfer.down[0]=999999999;transfer.up[0]=888888888;
    assert(db_add(&d,86340,&transfer));
    transfer.down[0]=0;transfer.up[0]=0;transfer.down[1]=1048576;transfer.up[1]=131072;
    for(int i=0;i<128;i++)assert(db_add(&d,86400+i,&transfer));
    assert(db_traffic(&d,86400,172800,&down,&up));assert(down==134217728 && up==16777216);
    assert(db_flush(&d,1100));db_close(&d);assert(db_open(&d,path));
    assert(db_traffic(&d,86400,172800,&down,&up));assert(down==134217728 && up==16777216);
    assert(db_correct(&d,86550,86400,172800,100,200));
    assert(db_traffic(&d,86400,172800,&down,&up));assert(down==134217728 && up==16777216);
    /* Daily reset preserves raw traffic and plan adjustments across restart. */
    assert(db_reset_today(&d,86400,172800));assert(db_today(&d,86400,172800,&down,&up));assert(down==0 && up==0);
    db_close(&d);assert(db_open(&d,path));assert(db_today(&d,86400,172800,&down,&up));assert(down==0 && up==0);
    assert(db_add(&d,86600,&transfer));assert(db_today(&d,86400,172800,&down,&up));assert(down==1048576 && up==131072);
    assert(db_flush(&d,1150));assert(db_reset_today(&d,86400,172800));assert(db_today(&d,86400,172800,&down,&up));assert(down==0 && up==0);
    assert(db_traffic(&d,86400,172800,&down,&up));assert(down==135266304 && up==16908288);
    assert(db_totals(&d,86400,172800,&got));assert(got.down[0]==100000000000ULL && got.down[1]==200001179648ULL);
    assert(sqlite3_exec(d.handle,"PRAGMA query_only=ON",0,0,0)==SQLITE_OK);assert(!db_reset_today(&d,86400,172800));assert(sqlite3_exec(d.handle,"PRAGMA query_only=OFF",0,0,0)==SQLITE_OK);
    assert(db_add(&d,172800,&transfer));assert(db_traffic(&d,172800,259200,&down,&up));assert(down==1048576 && up==131072);
    assert(db_flush(&d,1200));db_close(&d);assert(DeleteFileW(path));
    puts("PASS: persistence, recovery, exact 128 MiB down / 16 MiB up across restart, midnight and corrections");return 0;
}
