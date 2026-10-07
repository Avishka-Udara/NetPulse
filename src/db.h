#ifndef NETPULSE_DB_H
#define NETPULSE_DB_H
#include <windows.h>
#include "core.h"
#include "sqlite3.h"
#define PENDING_MAX 256
typedef struct { time_t stamp; Usage usage; } Pending;
typedef struct {
    sqlite3 *handle; Pending pending[PENDING_MAX]; unsigned count;
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
void db_close(Database *db);
#endif
