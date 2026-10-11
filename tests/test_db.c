#define _CRT_SECURE_NO_WARNINGS
#include "db.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
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
    assert(db_flush(&d,1050));
    wchar_t backup[MAX_PATH];assert(GetTempFileNameW(L"build",L"npb",0,backup));
    assert(db_backup(&d,backup));Database copy;assert(db_open(&copy,backup));
    assert(db_traffic(&copy,120,240,&down,&up));assert(down==450 && up==105);
    sqlite3_stmt *check=NULL;assert(sqlite3_prepare_v2(copy.handle,"PRAGMA integrity_check",-1,&check,NULL)==SQLITE_OK);assert(sqlite3_step(check)==SQLITE_ROW);assert(strcmp((const char*)sqlite3_column_text(check,0),"ok")==0);sqlite3_finalize(check);
    db_close(&copy);
    HANDLE locked=CreateFileW(backup,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(locked!=INVALID_HANDLE_VALUE);
    assert(!db_backup(&d,backup));CloseHandle(locked);assert(db_open(&copy,backup));assert(db_traffic(&copy,120,240,&down,&up));assert(down==450 && up==105);db_close(&copy);
    assert(DeleteFileW(backup));
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
    assert(db_open(&d,L":memory:"));Usage au={0};au.down[0]=100;au.up[0]=25;
    assert(db_app_add(&d,3600,L"browser.exe",&au));assert(db_app_add(&d,3660,L"browser.exe",&au));
    au.down[0]=0;au.up[0]=0;au.down[1]=50;assert(db_app_add(&d,7200,L"sync.exe",&au));
    AppTotal rows[8];unsigned count;assert(db_apps(&d,3600,7200,-1,rows,8,&count));assert(count==1 && rows[0].down==200 && rows[0].up==50);
    time_t edges[]={3600,3660,7200,7260};uint64_t ds[3],us[3];assert(db_app_series(&d,edges,3,-1,L"",ds,us));assert(ds[0]==100 && ds[1]==100 && ds[2]==50 && us[1]==25);
    assert(sqlite3_exec(d.handle,"PRAGMA query_only=ON",0,0,0)==SQLITE_OK);assert(!db_flush(&d,1300));assert(d.app_count==3);
    assert(sqlite3_exec(d.handle,"PRAGMA query_only=OFF",0,0,0)==SQLITE_OK);assert(db_flush(&d,1400));assert(!d.app_count);
    assert(db_apps(&d,0,8000,1,rows,8,&count));uint64_t sum=0;for(unsigned i=0;i<count;i++)sum+=rows[i].down+rows[i].up;assert(sum==50);
    assert(db_app_series(&d,edges,3,-1,L"browser.exe",ds,us));assert(ds[0]==100 && ds[1]==100 && ds[2]==0);
    assert(db_traffic(&d,0,8000,&down,&up));assert(down==0 && up==0);db_close(&d);
    assert(db_open(&d,L":memory:"));
    uint64_t observed,impaired;
    assert(db_capture_add(&d,100,160,0));assert(db_capture_add(&d,160,200,0));assert(d.span_count==1);
    assert(db_capture_add(&d,180,240,1));assert(db_capture_add(&d,300,360,0));
    assert(db_capture_coverage(&d,120,330,&observed,&impaired));assert(observed==150 && impaired==60);
    assert(db_flush(&d,1500));assert(db_capture_add(&d,310,370,0));
    assert(db_capture_coverage(&d,120,330,&observed,&impaired));assert(observed==150 && impaired==60);
    assert(db_capture_coverage(&d,240,300,&observed,&impaired));assert(!observed && !impaired);
    assert(db_alert_claim(&d,86400,0,1,90000,900)==1);assert(db_alert_claim(&d,86400,0,1,92000,900)==0);
    assert(db_alert_claim(&d,86400,0,2,90899,900)==0);assert(db_alert_claim(&d,86400,0,2,90900,900)==1);
    assert(db_alert_claim(&d,86400,0,4,90901,900)==0);assert(db_alert_claim(&d,86400,0,8,91800,900)==1);
    assert(db_alert_claim(&d,86400,0,4,93000,900)==0);assert(db_alert_claim(&d,172800,0,1,180000,900)==1);
    Config cfg={0};time_t now=time(NULL),day_start,day_end;tracking_day(&cfg,now,&day_start,&day_end);
    au=(Usage){0};au.down[0]=100;au.up[1]=20;
    assert(db_app_add(&d,day_start-3600,L"one.exe",&au));assert(db_app_add(&d,day_start-7200,L"two.exe",&au));
    assert(db_flush(&d,1600));assert(db_app_maintain(&d,&cfg,now));
    assert(db_apps(&d,day_start-86400,day_end,-1,rows,8,&count));assert(count==2 && rows[0].down==100 && rows[0].up==20);
    time_t daily[]={day_start-86400,day_start};assert(db_app_series(&d,daily,1,-1,L"",ds,us));assert(ds[0]==200 && us[0]==40);
    time_t hour[]={day_start-3600,day_start};assert(db_app_series(&d,hour,1,-1,L"",ds,us));assert(ds[0]==100 && us[0]==20);
    /* Late arrivals invalidate a cached day without double-counting its baseline. */
    assert(db_app_add(&d,day_start-3600,L"one.exe",&au));assert(db_app_series(&d,daily,1,-1,L"",ds,us));assert(ds[0]==300);
    assert(db_flush(&d,1700));assert(db_app_maintain(&d,&cfg,now));assert(db_app_series(&d,daily,1,-1,L"",ds,us));assert(ds[0]==300 && us[0]==60);
    /* A backward clock jump must not summarize an unfinished future day. */
    assert(db_app_add(&d,day_start-36*3600,L"future.exe",&au));assert(db_flush(&d,1750));
    assert(db_app_maintain(&d,&cfg,day_start-2*86400+3600));
    assert(sqlite3_prepare_v2(d.handle,"SELECT count(*) FROM app_summary_days",-1,&check,NULL)==SQLITE_OK);assert(sqlite3_step(check)==SQLITE_ROW);assert(sqlite3_column_int(check,0)==1);sqlite3_finalize(check);
    cfg.day_mode=2;cfg.day_minute=330;assert(db_app_maintain(&d,&cfg,now));assert(db_app_series(&d,daily,1,-1,L"",ds,us));assert(ds[0]==300 && us[0]==60);
    assert(db_app_add(&d,day_start-40*86400,L"old.exe",&au));assert(db_add(&d,day_start-40*86400,&au));assert(db_flush(&d,1800));
    assert(db_capture_add(&d,day_start-40*86400,now,0));assert(db_flush(&d,1900));
    cfg.retention_days=30;assert(db_app_maintain(&d,&cfg,now));assert(db_apps(&d,0,day_start-31*86400,-1,rows,8,&count));assert(!count);
    assert(db_capture_coverage(&d,0,day_start-31*86400,&observed,&impaired));assert(!observed);
    assert(db_traffic(&d,0,day_start-31*86400,&down,&up));assert(down==100 && up==20);
    db_close(&d);
    puts("PASS: persistence, recovery, exact 128 MiB down / 16 MiB up across restart, midnight and corrections");return 0;
}
