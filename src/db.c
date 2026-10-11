#define _CRT_SECURE_NO_WARNINGS
#include "db.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
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
        "CREATE TABLE IF NOT EXISTS sessions(start INTEGER PRIMARY KEY,last_seen INTEGER NOT NULL,system_uptime_ms INTEGER NOT NULL); CREATE TABLE IF NOT EXISTS app_usage(t INTEGER NOT NULL,app TEXT NOT NULL,pd INTEGER NOT NULL,pu INTEGER NOT NULL,od INTEGER NOT NULL,ou INTEGER NOT NULL,PRIMARY KEY(t,app)); CREATE TABLE IF NOT EXISTS capture(start INTEGER PRIMARY KEY,end INTEGER NOT NULL,lost INTEGER NOT NULL); CREATE INDEX IF NOT EXISTS capture_end ON capture(end); CREATE TABLE IF NOT EXISTS alerts(day INTEGER NOT NULL,band INTEGER NOT NULL,mask INTEGER NOT NULL,last INTEGER NOT NULL,PRIMARY KEY(day,band)); CREATE TABLE IF NOT EXISTS app_summary_days(start INTEGER PRIMARY KEY,end INTEGER NOT NULL); CREATE TABLE IF NOT EXISTS app_daily(t INTEGER NOT NULL,app TEXT NOT NULL,pd INTEGER NOT NULL,pu INTEGER NOT NULL,od INTEGER NOT NULL,ou INTEGER NOT NULL,PRIMARY KEY(t,app)); PRAGMA user_version=3;");
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
    if(sqlite3_prepare_v2(d->handle,"DELETE FROM app_daily WHERE t IN (SELECT start FROM app_summary_days WHERE start<=? AND end>?)",-1,&s,NULL)!=SQLITE_OK)goto error;
    for(unsigned i=0;i<d->app_count;i++){sqlite3_bind_int64(s,1,d->apps[i].stamp);sqlite3_bind_int64(s,2,d->apps[i].stamp);if(sqlite3_step(s)!=SQLITE_DONE)goto error;sqlite3_reset(s);}sqlite3_finalize(s);s=NULL;
    if(sqlite3_prepare_v2(d->handle,"DELETE FROM app_summary_days WHERE start<=? AND end>?",-1,&s,NULL)!=SQLITE_OK)goto error;
    for(unsigned i=0;i<d->app_count;i++){sqlite3_bind_int64(s,1,d->apps[i].stamp);sqlite3_bind_int64(s,2,d->apps[i].stamp);if(sqlite3_step(s)!=SQLITE_DONE)goto error;sqlite3_reset(s);}sqlite3_finalize(s);s=NULL;
    if(sqlite3_prepare_v2(d->handle,"INSERT INTO capture VALUES(coalesce((SELECT start FROM capture WHERE end=?1 AND lost=?3 ORDER BY start DESC LIMIT 1),?1),?2,?3) ON CONFLICT(start) DO UPDATE SET end=max(end,excluded.end),lost=max(lost,excluded.lost)",-1,&s,NULL)!=SQLITE_OK)goto error;
    for(unsigned i=0;i<d->span_count;i++){sqlite3_bind_int64(s,1,d->spans[i].start);sqlite3_bind_int64(s,2,d->spans[i].end);sqlite3_bind_int(s,3,d->spans[i].lost);if(sqlite3_step(s)!=SQLITE_DONE)goto error;sqlite3_reset(s);}sqlite3_finalize(s);s=NULL;
    if(sqlite3_prepare_v2(d->handle,"INSERT INTO app_usage VALUES(?,?,?,?,?,?) ON CONFLICT(t,app) DO UPDATE SET pd=pd+excluded.pd,pu=pu+excluded.pu,od=od+excluded.od,ou=ou+excluded.ou",-1,&s,NULL)!=SQLITE_OK)goto error;
    for(unsigned i=0;i<d->app_count;i++){
        AppPending *p=&d->apps[i];sqlite3_bind_int64(s,1,p->stamp);sqlite3_bind_text16(s,2,p->path,-1,SQLITE_STATIC);
        for(int band=0;band<2;band++){sqlite3_bind_int64(s,3+band*2,(sqlite3_int64)p->usage.down[band]);sqlite3_bind_int64(s,4+band*2,(sqlite3_int64)p->usage.up[band]);}
        if(sqlite3_step(s)!=SQLITE_DONE)goto error;sqlite3_reset(s);
    }
    sqlite3_finalize(s);s=NULL;
    if(sqlite3_prepare_v2(d->handle,"INSERT INTO sessions VALUES(?,?,?) ON CONFLICT(start) DO UPDATE SET last_seen=excluded.last_seen,system_uptime_ms=excluded.system_uptime_ms",-1,&s,NULL)!=SQLITE_OK)goto error;
    sqlite3_bind_int64(s,1,d->session);sqlite3_bind_int64(s,2,time(NULL));sqlite3_bind_int64(s,3,(sqlite3_int64)uptime);
    if(sqlite3_step(s)!=SQLITE_DONE)goto error;
    sqlite3_finalize(s);s=NULL;if(!sql(d,"COMMIT"))goto rollback;
    d->count=0;d->app_count=0;d->span_count=0;d->uptime=uptime;d->error[0]=0;return 1;
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
void db_close(Database *d){if(d->handle)sqlite3_close(d->handle);d->handle=NULL;free(d->apps);d->apps=NULL;}

int db_app_add(Database *d,time_t stamp,const wchar_t *path,const Usage *u) {
    if(!d->apps){d->apps=calloc(APP_PENDING_MAX,sizeof(AppPending));if(!d->apps){snprintf(d->error,sizeof(d->error),"Not enough memory for app tracking.");return 0;}}
    stamp-=stamp%60;unsigned i;for(i=0;i<d->app_count;i++)if(d->apps[i].stamp==stamp && !wcscmp(d->apps[i].path,path))break;
    if(i==d->app_count){if(i==APP_PENDING_MAX){snprintf(d->error,sizeof(d->error),"App storage backlog full. Tracking paused.");return 0;}
        memset(&d->apps[i],0,sizeof(AppPending));wcsncpy(d->apps[i].path,path,APP_PATH-1);d->apps[i].stamp=stamp;d->app_count++;}
    for(int p=0;p<2;p++){d->apps[i].usage.down[p]+=u->down[p];d->apps[i].usage.up[p]+=u->up[p];}return 1;
}
int db_period_usage(Database *d,time_t start,time_t end,Usage *u) {
    memset(u,0,sizeof(*u));sqlite3_stmt *s=NULL;
    if(sqlite3_prepare_v2(d->handle,"SELECT coalesce(sum(pd),0),coalesce(sum(pu),0),coalesce(sum(od),0),coalesce(sum(ou),0) FROM usage WHERE t>=? AND t<?",-1,&s,NULL)!=SQLITE_OK)return failed(d);
    sqlite3_bind_int64(s,1,start);sqlite3_bind_int64(s,2,end);int rc=sqlite3_step(s);
    if(rc==SQLITE_ROW)for(int p=0;p<2;p++){u->down[p]=(uint64_t)sqlite3_column_int64(s,p*2);u->up[p]=(uint64_t)sqlite3_column_int64(s,p*2+1);}sqlite3_finalize(s);if(rc!=SQLITE_ROW)return failed(d);
    for(unsigned i=0;i<d->count;i++)if(d->pending[i].stamp>=start && d->pending[i].stamp<end)for(int p=0;p<2;p++){u->down[p]+=d->pending[i].usage.down[p];u->up[p]+=d->pending[i].usage.up[p];}return 1;
}
/* Only substitute whole cached days inside the requested range. Hourly and
   shifted tracking-day queries keep exact minute data; no proportional guesses. */
#define APP_RANGE "WITH chosen AS MATERIALIZED (SELECT start,end FROM app_summary_days WHERE start>=?1 AND end<=?2), gaps AS (SELECT ?1 AS start,coalesce((SELECT min(start) FROM chosen),?2) AS end UNION ALL SELECT end,coalesce((SELECT min(start) FROM chosen n WHERE n.start>=c.end),?2) FROM chosen c), all_app AS (SELECT u.* FROM gaps g JOIN app_usage u ON u.t>=g.start AND u.t<g.end UNION ALL SELECT a.* FROM app_daily a JOIN chosen s ON a.t=s.start) "
static int app_compare(const void *a,const void *b){const AppTotal *x=a,*y=b;uint64_t u=x->down+x->up,v=y->down+y->up;return u<v?1:u>v?-1:wcscmp(x->path,y->path);}
int db_apps(Database *d,time_t start,time_t end,int period,AppTotal *out,unsigned capacity,unsigned *count) {
    *count=0;sqlite3_stmt *s=NULL;const char *query=period==0?APP_RANGE "SELECT app,sum(pd),sum(pu) FROM all_app GROUP BY app":period==1?APP_RANGE "SELECT app,sum(od),sum(ou) FROM all_app GROUP BY app":APP_RANGE "SELECT app,sum(pd+od),sum(pu+ou) FROM all_app GROUP BY app";
    if(sqlite3_prepare_v2(d->handle,query,-1,&s,NULL)!=SQLITE_OK)return failed(d);sqlite3_bind_int64(s,1,start);sqlite3_bind_int64(s,2,end);
    int rc;while((rc=sqlite3_step(s))==SQLITE_ROW){if(*count==capacity){sqlite3_finalize(s);snprintf(d->error,sizeof(d->error),"Too many apps to display. Choose a shorter range.");return 0;}
        AppTotal *a=&out[(*count)++];memset(a,0,sizeof(*a));wcsncpy(a->path,sqlite3_column_text16(s,0),APP_PATH-1);a->down=(uint64_t)sqlite3_column_int64(s,1);a->up=(uint64_t)sqlite3_column_int64(s,2);}
    sqlite3_finalize(s);if(rc!=SQLITE_DONE)return failed(d);
    for(unsigned j=0;j<d->app_count;j++){AppPending *p=&d->apps[j];if(p->stamp<start || p->stamp>=end)continue;
        unsigned i;for(i=0;i<*count;i++)if(!wcscmp(out[i].path,p->path))break;
        if(i==*count){if(i==capacity)return 0;memset(&out[i],0,sizeof(out[i]));wcscpy(out[i].path,p->path);(*count)++;}
        for(int b=0;b<2;b++)if(period<0 || period==b){out[i].down+=p->usage.down[b];out[i].up+=p->usage.up[b];}}
    unsigned kept=0;for(unsigned i=0;i<*count;i++)if(out[i].down || out[i].up)out[kept++]=out[i];*count=kept;
    qsort(out,*count,sizeof(*out),app_compare);return 1;
}

int db_app_series(Database *d,const time_t *edges,unsigned bins,int period,const wchar_t *path,uint64_t *down,uint64_t *up) {
    if(!bins)return 1;
    if(bins>1){for(unsigned i=0;i<bins;i++)if(!db_app_series(d,edges+i,1,period,path,down+i,up+i))return 0;return 1;}
    memset(down,0,bins*sizeof(*down));memset(up,0,bins*sizeof(*up));sqlite3_stmt *s=NULL;
    if(sqlite3_prepare_v2(d->handle,APP_RANGE "SELECT coalesce(sum(pd),0),coalesce(sum(pu),0),coalesce(sum(od),0),coalesce(sum(ou),0) FROM all_app WHERE (?3='' OR app=?4)",-1,&s,NULL)!=SQLITE_OK)return failed(d);
    sqlite3_bind_int64(s,1,edges[0]);sqlite3_bind_int64(s,2,edges[bins]);sqlite3_bind_text16(s,3,path,-1,SQLITE_STATIC);sqlite3_bind_text16(s,4,path,-1,SQLITE_STATIC);
    int rc=sqlite3_step(s);if(rc==SQLITE_ROW)for(int p=0;p<2;p++)if(period<0 || p==period){down[0]+=(uint64_t)sqlite3_column_int64(s,p*2);up[0]+=(uint64_t)sqlite3_column_int64(s,1+p*2);}
    sqlite3_finalize(s);if(rc!=SQLITE_ROW)return failed(d);
    for(unsigned j=0;j<d->app_count;j++){AppPending *a=&d->apps[j];if(a->stamp<edges[0] || a->stamp>=edges[bins] || (*path && wcscmp(path,a->path)))continue;
        unsigned i=0;while(i<bins && a->stamp>=edges[i+1])i++;if(i==bins)continue;for(int p=0;p<2;p++)if(period<0 || p==period){down[i]+=a->usage.down[p];up[i]+=a->usage.up[p];}}
    return 1;
}

int db_capture_add(Database *d,time_t start,time_t end,int lost) {
    if(end<=start)return 1;
    if(d->span_count){CaptureSpan *p=&d->spans[d->span_count-1];if(p->end==start && p->lost==!!lost){p->end=end;return 1;}}
    if(d->span_count==64 && !db_flush(d,d->uptime))return 0;
    d->spans[d->span_count++]=(CaptureSpan){start,end,!!lost};return 1;
}
static int span_compare(const void *a,const void *b){const CaptureSpan *x=a,*y=b;return x->start<y->start?-1:x->start>y->start;}
int db_capture_coverage(Database *d,time_t start,time_t end,uint64_t *observed,uint64_t *impaired) {
    *observed=*impaired=0;if(end<=start)return 1;
    sqlite3_stmt *s=NULL;CaptureSpan *spans=NULL;size_t count=0,capacity=0;int rc;
    if(sqlite3_prepare_v2(d->handle,"SELECT start,end,lost FROM capture WHERE start<? AND end>?",-1,&s,NULL)!=SQLITE_OK)return failed(d);
    sqlite3_bind_int64(s,1,end);sqlite3_bind_int64(s,2,start);
    while((rc=sqlite3_step(s))==SQLITE_ROW){
        if(count==capacity){size_t next=capacity?capacity*2:128;CaptureSpan *p=realloc(spans,next*sizeof(*p));if(!p){sqlite3_finalize(s);free(spans);return 0;}spans=p;capacity=next;}
        spans[count++]=(CaptureSpan){(time_t)sqlite3_column_int64(s,0),(time_t)sqlite3_column_int64(s,1),sqlite3_column_int(s,2)};
    }
    sqlite3_finalize(s);if(rc!=SQLITE_DONE){free(spans);return failed(d);}
    CaptureSpan *p=realloc(spans,(count+d->span_count+1)*sizeof(*p));if(!p){free(spans);return 0;}spans=p;
    for(unsigned i=0;i<d->span_count;i++)spans[count++]=d->spans[i];
    qsort(spans,count,sizeof(*spans),span_compare);
    /* Union, rather than sum, also handles clock rollback and pending overlaps. */
    for(int loss=0;loss<2;loss++){time_t cursor=start;uint64_t total=0;for(size_t i=0;i<count;i++){
        if(loss && !spans[i].lost)continue;time_t a=spans[i].start>cursor?spans[i].start:cursor,b=spans[i].end<end?spans[i].end:end;
        if(b>a){total+=(uint64_t)(b-a);cursor=b;}
    }if(loss)*impaired=total;else *observed=total;}
    free(spans);return 1;
}
int db_app_maintain(Database *d,const Config *c,time_t now) {
    /* Work is bounded to eight completed days per invocation. */
    sqlite3_stmt *s=NULL;time_t today,end;tracking_day(c,now,&today,&end);
    if(!sql(d,"BEGIN IMMEDIATE"))return 0;
    if(c->retention_days>0){struct tm t=*localtime(&today);t.tm_mday-=c->retention_days;t.tm_isdst=-1;time_t cutoff=mktime(&t);
        const char *queries[]={"DELETE FROM app_usage WHERE t<?","DELETE FROM app_daily WHERE t IN (SELECT start FROM app_summary_days WHERE start<?)","DELETE FROM app_summary_days WHERE start<?","INSERT INTO capture SELECT ?1,max(end),max(lost) FROM capture WHERE start<?1 AND end>?1 HAVING count(*)>0 ON CONFLICT(start) DO UPDATE SET end=max(end,excluded.end),lost=max(lost,excluded.lost)","DELETE FROM capture WHERE start<?","DELETE FROM alerts WHERE day<?"};
        for(unsigned i=0;i<sizeof(queries)/sizeof(*queries);i++){if(sqlite3_prepare_v2(d->handle,queries[i],-1,&s,NULL)!=SQLITE_OK)goto error;sqlite3_bind_int64(s,1,cutoff);if(sqlite3_step(s)!=SQLITE_DONE)goto error;sqlite3_finalize(s);s=NULL;}
    }
    for(int batch=0;batch<8;batch++){
        if(sqlite3_prepare_v2(d->handle,"WITH gaps AS (SELECT 0 AS start,min(?1,coalesce((SELECT min(start) FROM app_summary_days),?1)) AS end UNION ALL SELECT end,min(?1,coalesce((SELECT min(start) FROM app_summary_days n WHERE n.start>=s.end),?1)) FROM app_summary_days s) SELECT min((SELECT min(t) FROM app_usage WHERE t>=g.start AND t<g.end)) FROM gaps g",-1,&s,NULL)!=SQLITE_OK)goto error;
        sqlite3_bind_int64(s,1,today);if(sqlite3_step(s)!=SQLITE_ROW)goto error;
        if(sqlite3_column_type(s,0)==SQLITE_NULL){sqlite3_finalize(s);s=NULL;break;}
        time_t stamp=(time_t)sqlite3_column_int64(s,0),start;sqlite3_finalize(s);s=NULL;tracking_day(c,stamp,&start,&end);
        const char *queries[]={"DELETE FROM app_daily WHERE t IN (SELECT start FROM app_summary_days WHERE start<?2 AND end>?1)","DELETE FROM app_summary_days WHERE start<?2 AND end>?1","INSERT INTO app_daily SELECT ?1,app,sum(pd),sum(pu),sum(od),sum(ou) FROM app_usage WHERE t>=?1 AND t<?2 GROUP BY app","INSERT INTO app_summary_days VALUES(?1,?2)"};
        for(unsigned i=0;i<sizeof(queries)/sizeof(*queries);i++){if(sqlite3_prepare_v2(d->handle,queries[i],-1,&s,NULL)!=SQLITE_OK)goto error;sqlite3_bind_int64(s,1,start);sqlite3_bind_int64(s,2,end);if(sqlite3_step(s)!=SQLITE_DONE)goto error;sqlite3_finalize(s);s=NULL;}
    }
    if(sql(d,"COMMIT"))return 1;goto rollback;
error:failed(d);sqlite3_finalize(s);
rollback:sqlite3_exec(d->handle,"ROLLBACK",NULL,NULL,NULL);return 0;
}
int db_alert_claim(Database *d,time_t day,int band,int level,time_t now,int cooldown) {
    if(!level)return 0;sqlite3_stmt *s=NULL;int mask=0;time_t last=0;
    if(!sql(d,"BEGIN IMMEDIATE"))return -1;
    if(sqlite3_prepare_v2(d->handle,"SELECT mask,last FROM alerts WHERE day=? AND band=?",-1,&s,NULL)!=SQLITE_OK)goto error;
    sqlite3_bind_int64(s,1,day);sqlite3_bind_int(s,2,band);int rc=sqlite3_step(s);
    if(rc==SQLITE_ROW){mask=sqlite3_column_int(s,0);last=(time_t)sqlite3_column_int64(s,1);}else if(rc!=SQLITE_DONE)goto error;
    sqlite3_finalize(s);s=NULL;
    if((mask&level) || (last && (now<last || now-last<cooldown))){sqlite3_exec(d->handle,"ROLLBACK",NULL,NULL,NULL);return 0;}
    if(sqlite3_prepare_v2(d->handle,"INSERT INTO alerts VALUES(?,?,?,?) ON CONFLICT(day,band) DO UPDATE SET mask=excluded.mask,last=excluded.last",-1,&s,NULL)!=SQLITE_OK)goto error;
    sqlite3_bind_int64(s,1,day);sqlite3_bind_int(s,2,band);sqlite3_bind_int(s,3,mask|(level*2-1));sqlite3_bind_int64(s,4,now);
    if(sqlite3_step(s)!=SQLITE_DONE)goto error;sqlite3_finalize(s);s=NULL;if(sql(d,"COMMIT"))return 1;goto rollback;
error:failed(d);sqlite3_finalize(s);
rollback:sqlite3_exec(d->handle,"ROLLBACK",NULL,NULL,NULL);return -1;
}
