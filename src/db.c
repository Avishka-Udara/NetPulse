#define _CRT_SECURE_NO_WARNINGS
#include "db.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
static int failed(Database *d) { snprintf(d->error,sizeof(d->error),"%s",sqlite3_errmsg(d->handle));return 0; }
static int sql(Database *d,const char *s) { return sqlite3_exec(d->handle,s,NULL,NULL,NULL)==SQLITE_OK ? 1:failed(d); }
int db_open(Database *d,const wchar_t *path) {
    memset(d,0,sizeof(*d));d->session=time(NULL);
    if(sqlite3_open16(path,&d->handle)!=SQLITE_OK)return failed(d);
    sqlite3_busy_timeout(d->handle,250);
    return sql(d,"PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA cache_size=-128; PRAGMA temp_store=MEMORY;"
        "CREATE TABLE IF NOT EXISTS usage(t INTEGER PRIMARY KEY,pd INTEGER NOT NULL,pu INTEGER NOT NULL,od INTEGER NOT NULL,ou INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS corrections(id INTEGER PRIMARY KEY,t INTEGER NOT NULL,p INTEGER NOT NULL,o INTEGER NOT NULL);"
        "CREATE INDEX IF NOT EXISTS corrections_time ON corrections(t);"
        "CREATE TABLE IF NOT EXISTS daily_resets(day INTEGER PRIMARY KEY,d INTEGER NOT NULL,u INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS sessions(start INTEGER PRIMARY KEY,last_seen INTEGER NOT NULL,system_uptime_ms INTEGER NOT NULL); PRAGMA user_version=1;");
}
int db_add(Database *d,time_t stamp,const Usage *u) {
    stamp-=stamp%60;
    unsigned i=0;for(;i<d->count;i++)if(d->pending[i].stamp==stamp)break;
    if(i==d->count){if(d->count==PENDING_MAX){snprintf(d->error,sizeof(d->error),"Storage backlog full; monitoring paused. Free disk space.");return 0;}
        memset(&d->pending[i],0,sizeof(Pending));d->pending[i].stamp=stamp;d->count++;}
    for(int p=0;p<2;p++){d->pending[i].usage.down[p]+=u->down[p];d->pending[i].usage.up[p]+=u->up[p];}return 1;
}
int db_flush(Database *d,uint64_t uptime) {
    sqlite3_stmt *s=NULL;if(!sql(d,"BEGIN IMMEDIATE"))return 0;
    const char *q="INSERT INTO usage VALUES(?,?,?,?,?) ON CONFLICT(t) DO UPDATE SET pd=pd+excluded.pd,pu=pu+excluded.pu,od=od+excluded.od,ou=ou+excluded.ou";
    if(sqlite3_prepare_v2(d->handle,q,-1,&s,NULL)!=SQLITE_OK)goto error;
    for(unsigned i=0;i<d->count;i++) {
        Pending *p=&d->pending[i];sqlite3_bind_int64(s,1,p->stamp);
        sqlite3_bind_int64(s,2,(sqlite3_int64)p->usage.down[0]);sqlite3_bind_int64(s,3,(sqlite3_int64)p->usage.up[0]);
        sqlite3_bind_int64(s,4,(sqlite3_int64)p->usage.down[1]);sqlite3_bind_int64(s,5,(sqlite3_int64)p->usage.up[1]);
        if(sqlite3_step(s)!=SQLITE_DONE)goto error;sqlite3_reset(s);
    }
    sqlite3_finalize(s);s=NULL;
    if(sqlite3_prepare_v2(d->handle,"INSERT INTO sessions VALUES(?,?,?) ON CONFLICT(start) DO UPDATE SET last_seen=excluded.last_seen,system_uptime_ms=excluded.system_uptime_ms",-1,&s,NULL)!=SQLITE_OK)goto error;
    sqlite3_bind_int64(s,1,d->session);sqlite3_bind_int64(s,2,time(NULL));sqlite3_bind_int64(s,3,(sqlite3_int64)uptime);
    if(sqlite3_step(s)!=SQLITE_DONE)goto error;
    sqlite3_finalize(s);s=NULL;if(!sql(d,"COMMIT"))goto rollback;
    d->count=0;d->uptime=uptime;d->error[0]=0;return 1;
error: failed(d);sqlite3_finalize(s);
rollback: sqlite3_exec(d->handle,"ROLLBACK",NULL,NULL,NULL);return 0;
}
int db_totals(Database *d,time_t start,time_t end,Usage *u) {
    sqlite3_stmt *s=NULL;memset(u,0,sizeof(*u));
    if(sqlite3_prepare_v2(d->handle,"SELECT coalesce(sum(pd),0),coalesce(sum(pu),0),coalesce(sum(od),0),coalesce(sum(ou),0) FROM usage WHERE t>=? AND t<?",-1,&s,NULL)!=SQLITE_OK)return failed(d);
    sqlite3_bind_int64(s,1,start);sqlite3_bind_int64(s,2,end);
    if(sqlite3_step(s)!=SQLITE_ROW){sqlite3_finalize(s);return failed(d);}
    for(int p=0;p<2;p++){u->down[p]=(uint64_t)sqlite3_column_int64(s,p*2);u->up[p]=(uint64_t)sqlite3_column_int64(s,p*2+1);}
    sqlite3_finalize(s);s=NULL;
    if(sqlite3_prepare_v2(d->handle,"SELECT coalesce(sum(p),0),coalesce(sum(o),0) FROM corrections WHERE t>=? AND t<?",-1,&s,NULL)!=SQLITE_OK)return failed(d);
    sqlite3_bind_int64(s,1,start);sqlite3_bind_int64(s,2,end);
    if(sqlite3_step(s)!=SQLITE_ROW){sqlite3_finalize(s);return failed(d);}
    /* Corrections apply to combined usage; preserve actual raw upload/download in the ledger. */
    for(int p=0;p<2;p++) {
        int64_t v=(int64_t)(u->down[p]+u->up[p])+sqlite3_column_int64(s,p);
        u->down[p]=v>0?(uint64_t)v:0;u->up[p]=0;
    }
    sqlite3_finalize(s);
    for(unsigned i=0;i<d->count;i++)if(d->pending[i].stamp>=start && d->pending[i].stamp<end)
        for(int p=0;p<2;p++){u->down[p]+=d->pending[i].usage.down[p];u->up[p]+=d->pending[i].usage.up[p];}
    return 1;
}
int db_traffic(Database *d,time_t start,time_t end,uint64_t *down,uint64_t *up) {
    sqlite3_stmt *s=NULL;*down=*up=0;
    if(sqlite3_prepare_v2(d->handle,"SELECT coalesce(sum(pd+od),0),coalesce(sum(pu+ou),0) FROM usage WHERE t>=? AND t<?",-1,&s,NULL)!=SQLITE_OK)return failed(d);
    sqlite3_bind_int64(s,1,start);sqlite3_bind_int64(s,2,end);
    if(sqlite3_step(s)!=SQLITE_ROW){sqlite3_finalize(s);return failed(d);}
    *down=(uint64_t)sqlite3_column_int64(s,0);*up=(uint64_t)sqlite3_column_int64(s,1);sqlite3_finalize(s);
    for(unsigned i=0;i<d->count;i++)if(d->pending[i].stamp>=start && d->pending[i].stamp<end)
        for(int p=0;p<2;p++){*down+=d->pending[i].usage.down[p];*up+=d->pending[i].usage.up[p];}
    return 1;
}
int db_today(Database *d,time_t start,time_t end,uint64_t *down,uint64_t *up) {
    if(!db_traffic(d,start,end,down,up))return 0;
    sqlite3_stmt *s=NULL;
    if(sqlite3_prepare_v2(d->handle,"SELECT d,u FROM daily_resets WHERE day=?",-1,&s,NULL)!=SQLITE_OK)return failed(d);
    sqlite3_bind_int64(s,1,start);int rc=sqlite3_step(s);
    if(rc==SQLITE_ROW){uint64_t bd=(uint64_t)sqlite3_column_int64(s,0),bu=(uint64_t)sqlite3_column_int64(s,1);*down=*down>bd?*down-bd:0;*up=*up>bu?*up-bu:0;}
    sqlite3_finalize(s);return rc==SQLITE_ROW || rc==SQLITE_DONE?1:failed(d);
}
int db_reset_today(Database *d,time_t start,time_t end) {
    uint64_t down,up;if(!db_traffic(d,start,end,&down,&up))return 0;
    sqlite3_stmt *s=NULL;
    if(sqlite3_prepare_v2(d->handle,"INSERT INTO daily_resets VALUES(?,?,?) ON CONFLICT(day) DO UPDATE SET d=excluded.d,u=excluded.u",-1,&s,NULL)!=SQLITE_OK)return failed(d);
    sqlite3_bind_int64(s,1,start);sqlite3_bind_int64(s,2,(sqlite3_int64)down);sqlite3_bind_int64(s,3,(sqlite3_int64)up);
    int ok=sqlite3_step(s)==SQLITE_DONE;sqlite3_finalize(s);return ok?1:failed(d);
}
int db_correct(Database *d,time_t stamp,time_t start,time_t end,double peak,double offpeak) {
    Usage u;sqlite3_stmt *s=NULL;if(!db_totals(d,start,end,&u))return 0;
    if(sqlite3_prepare_v2(d->handle,"INSERT INTO corrections(t,p,o) VALUES(?,?,?)",-1,&s,NULL)!=SQLITE_OK)return failed(d);
    sqlite3_bind_int64(s,1,stamp);
    sqlite3_bind_int64(s,2,(int64_t)(peak*1e9)-(int64_t)(u.down[0]+u.up[0]));
    sqlite3_bind_int64(s,3,(int64_t)(offpeak*1e9)-(int64_t)(u.down[1]+u.up[1]));
    int ok=sqlite3_step(s)==SQLITE_DONE;sqlite3_finalize(s);return ok?1:failed(d);
}
int db_export(Database *d,const wchar_t *path) {
    FILE *f=_wfopen(path,L"wb");if(!f){snprintf(d->error,sizeof(d->error),"Could not create the CSV file.");return 0;}
    sqlite3_stmt *s=NULL;int ok=0;
    if(sqlite3_prepare_v2(d->handle,"SELECT t,pd,pu,od,ou,0,0 FROM usage UNION ALL SELECT t,0,0,0,0,p,o FROM corrections ORDER BY 1",-1,&s,NULL)!=SQLITE_OK)goto done;
    fprintf(f,"unix_time,peak_download_bytes,peak_upload_bytes,offpeak_download_bytes,offpeak_upload_bytes,peak_adjustment_bytes,offpeak_adjustment_bytes\r\n");
    int rc;while((rc=sqlite3_step(s))==SQLITE_ROW) {
        for(int i=0;i<7;i++)fprintf(f,"%s%lld",i?",":"",(long long)sqlite3_column_int64(s,i));fprintf(f,"\r\n");
    }
    ok=rc==SQLITE_DONE && !ferror(f);
done: sqlite3_finalize(s);if(fclose(f)!=0)ok=0;if(!ok)snprintf(d->error,sizeof(d->error),"CSV export failed.");return ok;
}
int db_backup(Database *d,const wchar_t *path) {
    wchar_t directory[MAX_PATH],temporary[MAX_PATH];
    if(wcslen(path)>=MAX_PATH){snprintf(d->error,sizeof(d->error),"Backup path is too long.");return 0;}
    wcscpy(directory,path);wchar_t *slash=wcsrchr(directory,L'\\');wchar_t *forward=wcsrchr(directory,L'/');if(forward && (!slash || forward>slash))slash=forward;
    if(slash)slash[1]=0;else wcscpy(directory,L".");
    if(!GetTempFileNameW(directory,L"npb",0,temporary)){snprintf(d->error,sizeof(d->error),"Cannot create backup temporary file.");return 0;}
    sqlite3 *target=NULL;
    if(sqlite3_open16(temporary,&target)!=SQLITE_OK){if(target)sqlite3_close(target);DeleteFileW(temporary);snprintf(d->error,sizeof(d->error),"Cannot open backup destination.");return 0;}
    sqlite3_backup *backup=sqlite3_backup_init(target,"main",d->handle,"main");
    int rc=backup?sqlite3_backup_step(backup,-1):SQLITE_ERROR;
    int finish=backup?sqlite3_backup_finish(backup):SQLITE_ERROR;
    int close=sqlite3_close(target);
    if(rc!=SQLITE_DONE || finish!=SQLITE_OK || close!=SQLITE_OK || !MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary);snprintf(d->error,sizeof(d->error),"Database backup failed; choose another location.");return 0;}
    return 1;
}
void db_close(Database *d){if(d->handle)sqlite3_close(d->handle);d->handle=NULL;}
