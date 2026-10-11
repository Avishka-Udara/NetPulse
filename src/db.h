#ifndef NETPULSE_DB_H
#define NETPULSE_DB_H
#include <windows.h>
#include "core.h"
#include "sqlite3.h"
#define PENDING_MAX 256
#define APP_PENDING_MAX 2048
#define APP_PATH 520
typedef struct { wchar_t path[APP_PATH]; time_t stamp; Usage usage; } AppPending;
typedef struct { wchar_t path[APP_PATH]; uint64_t down,up; } AppTotal;
typedef struct { time_t stamp; Usage usage; } Pending;
typedef struct { time_t start,end;int lost; } CaptureSpan;
typedef struct {
    sqlite3 *handle; Pending pending[PENDING_MAX]; unsigned count;
    AppPending *apps; unsigned app_count;
    CaptureSpan spans[64];unsigned span_count;
    char error[256]; time_t session; uint64_t uptime;
} Database;
int db_open(Database *db,const wchar_t *path);
int db_add(Database *db,time_t stamp,const Usage *usage);
int db_flush(Database *db,uint64_t uptime);
int db_totals(Database *db,time_t start,time_t end,Usage *usage);
int db_traffic(Database *db,time_t start,time_t end,uint64_t *down,uint64_t *up);
int db_today(Database *db,time_t start,time_t end,uint64_t *down,uint64_t *up);
int db_reset_today(Database *db,time_t start,time_t end);
int db_correct(Database *db,time_t stamp,time_t start,time_t end,double peak,double offpeak);
int db_export(Database *db,const wchar_t *path);
int db_backup(Database *db,const wchar_t *path);
int db_app_add(Database *d,time_t stamp,const wchar_t *path,const Usage *u);
int db_apps(Database *d,time_t start,time_t end,int period,AppTotal *out,unsigned capacity,unsigned *count);
int db_app_series(Database *d,const time_t *edges,unsigned bins,int period,const wchar_t *path,uint64_t *down,uint64_t *up);
int db_period_usage(Database *d,time_t start,time_t end,Usage *u);
int db_capture_add(Database *d,time_t start,time_t end,int lost);
int db_capture_coverage(Database *d,time_t start,time_t end,uint64_t *observed,uint64_t *impaired);
int db_app_maintain(Database *d,const Config *c,time_t now);
int db_alert_claim(Database *d,time_t day,int band,int level,time_t now,int cooldown);
void db_close(Database *db);
#endif
