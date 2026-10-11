#define _CRT_SECURE_NO_WARNINGS
#include "db.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Synthetic, deterministic workload: no network access and no user profile. */
int main(void){
    _putenv_s("TZ","UTC");_tzset();
    Database d;Config c={0};wchar_t file[MAX_PATH];assert(GetTempFileNameW(L"build",L"nph",0,file));assert(db_open(&d,file));
    time_t today,end;tracking_day(&c,time(NULL),&today,&end);ULONGLONG begin=GetTickCount64();
    const unsigned apps=128,days=90;uint64_t expected=0;Usage u={0};u.down[0]=1048576;u.up[1]=4096;
    for(unsigned day=0;day<days;day++){
        for(unsigned hour=0;hour<24;hour++){
            for(unsigned a=0;a<apps;a++){wchar_t path[80];swprintf(path,80,L"C:\\synthetic\\app-%03u.exe",a);assert(db_app_add(&d,today-(days-day)*86400+hour*3600,path,&u));expected+=u.down[0]+u.up[1];}
            if((hour+1)%8==0)assert(db_flush(&d,GetTickCount64()));
        }
    }
    ULONGLONG inserted=GetTickCount64();for(unsigned i=0;i<(days+7)/8;i++)assert(db_app_maintain(&d,&c,time(NULL)));
    ULONGLONG summarized=GetTickCount64();AppTotal *rows=calloc(apps,sizeof(*rows));assert(rows);unsigned count;assert(db_apps(&d,today-days*86400,end,-1,rows,apps,&count));assert(count==apps);
    uint64_t total=0;for(unsigned i=0;i<count;i++)total+=rows[i].down+rows[i].up;assert(total==expected);
    time_t edges[31];for(int i=0;i<=30;i++)edges[i]=today-(30-i)*86400;uint64_t down[30],up[30];
    for(int repeat=0;repeat<10;repeat++){assert(db_app_series(&d,edges,30,-1,L"",down,up));for(int i=0;i<30;i++)assert(down[i]+up[i]==24*apps*(u.down[0]+u.up[1]));}
    ULONGLONG queried=GetTickCount64();c.retention_days=30;assert(db_app_maintain(&d,&c,time(NULL)));assert(db_apps(&d,0,end,-1,rows,apps,&count));
    total=0;for(unsigned i=0;i<count;i++)total+=rows[i].down+rows[i].up;assert(total==expected/3);
    sqlite3_stmt *check;assert(sqlite3_prepare_v2(d.handle,"PRAGMA integrity_check",-1,&check,0)==SQLITE_OK);assert(sqlite3_step(check)==SQLITE_ROW);assert(!strcmp((const char*)sqlite3_column_text(check,0),"ok"));sqlite3_finalize(check);
    printf("PASS: %u app-minute rows / %u apps / %u days; insert %llu ms, summarize %llu ms, 10 monthly charts + totals %llu ms; exact totals and retention\n",days*24*apps,apps,days,(unsigned long long)(inserted-begin),(unsigned long long)(summarized-inserted),(unsigned long long)(queried-summarized));
    free(rows);db_close(&d);assert(DeleteFileW(file));return 0;
}
