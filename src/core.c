#define _CRT_SECURE_NO_WARNINGS
#include "core.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
/* Exact floor(bytes * part / total), without floating-point rounding or overflow. */
uint64_t proportional_bytes(uint64_t bytes,uint64_t part,uint64_t total) {
    if(!total || !part)return 0;if(part>=total)return bytes;
    uint64_t q=bytes/total,r=bytes%total;
    if(r<=UINT64_MAX/part)return q*part+r*part/total;
    q=0;r=0;
    for(int bit=63;bit>=0;bit--){
        q*=2;
        if(r>=total-r){r-=total-r;q++;}else r+=r;
        if((bytes>>bit)&1){if(r>=total-part){r-=total-part;q++;}else r+=part;}
    }
    return q;
}
void config_defaults(Config *c) {
    memset(c,0,sizeof(*c)); c->peak_start=480; c->peak_end=0;
    c->reset_day=1; c->peak_gb=100; c->offpeak_gb=100; c->position=2;
    c->widget_width=136;c->font_size=11;c->transparent=1;c->show_totals=1;
}
int parse_clock(const char *s,int *minute) {
    if(strlen(s)!=5 || !isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1]) ||
       s[2]!=':' || !isdigit((unsigned char)s[3]) || !isdigit((unsigned char)s[4])) return 0;
    int h=(s[0]-'0')*10+s[1]-'0',m=(s[3]-'0')*10+s[4]-'0';
    if(h>23 || m>59)return 0; *minute=h*60+m; return 1;
}
int is_peak(const Config *c,int m) {
    if(c->peak_start==c->peak_end) return 1;
    return c->peak_start<c->peak_end ? m>=c->peak_start && m<c->peak_end : m>=c->peak_start || m<c->peak_end;
}
static time_t boundary(const Config *c,int year,int month) {
    struct tm t={0};
    t.tm_year=year; t.tm_mon=month+1; t.tm_mday=0; t.tm_hour=12; t.tm_isdst=-1;
    mktime(&t); int last=t.tm_mday;
    t.tm_mday=c->reset_day<last ? c->reset_day:last;
    t.tm_hour=c->reset_minute/60; t.tm_min=c->reset_minute%60; t.tm_sec=0; t.tm_isdst=-1;
    return mktime(&t);
}
time_t cycle_start(const Config *c,time_t now) {
    struct tm t=*localtime(&now); time_t b=boundary(c,t.tm_year,t.tm_mon);
    return now<b ? boundary(c,t.tm_year,t.tm_mon-1):b;
}
time_t cycle_next(const Config *c,time_t now) {
    struct tm t=*localtime(&now); time_t b=boundary(c,t.tm_year,t.tm_mon);
    return now<b ? b:boundary(c,t.tm_year,t.tm_mon+1);
}
void usage_split(const Config *c,time_t start,time_t end,uint64_t down,uint64_t up,Usage *out) {
    memset(out,0,sizeof(*out));
    if(end<=start) { struct tm t=*localtime(&end); int p=is_peak(c,t.tm_hour*60+t.tm_min)?0:1; out->down[p]=down;out->up[p]=up;return; }
    uint64_t seconds[2]={0,0}; time_t cursor=start;
    while(cursor<end) {
        struct tm t=*localtime(&cursor); int p=is_peak(c,t.tm_hour*60+t.tm_min)?0:1;
        time_t next=cursor+60-t.tm_sec; if(next>end)next=end;
        seconds[p]+=(uint64_t)(next-cursor); cursor=next;
    }
    out->down[0]=proportional_bytes(down,(uint64_t)seconds[0],(uint64_t)(end-start));
    out->up[0]=proportional_bytes(up,(uint64_t)seconds[0],(uint64_t)(end-start));
    out->down[1]=down-out->down[0];out->up[1]=up-out->up[0];
}
int counter_delta(uint64_t od,uint64_t ou,uint64_t d,uint64_t u,uint64_t *dd,uint64_t *du) {
    /* Drivers can reset one counter independently. Keep the unaffected direction. */
    *dd=d>=od?d-od:0;*du=u>=ou?u-ou:0;return d>=od && u>=ou;
}
