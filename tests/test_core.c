#define _CRT_SECURE_NO_WARNINGS
#include "core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static time_t at(int y,int m,int d,int h,int min,int s){struct tm t={0};t.tm_year=y-1900;t.tm_mon=m-1;t.tm_mday=d;t.tm_hour=h;t.tm_min=min;t.tm_sec=s;t.tm_isdst=-1;return mktime(&t);}
int main(void) {
    assert(proportional_bytes(UINT64_MAX,1,2)==UINT64_MAX/2);
    assert(proportional_bytes(UINT64_MAX,UINT64_MAX-1,UINT64_MAX)==UINT64_MAX-1);
    assert(proportional_bytes(UINT64_MAX-1,UINT64_MAX-2,UINT64_MAX)==UINT64_MAX-3);
    assert(proportional_bytes(1001,1,3)==333);
    for(uint64_t total=1;total<101;total++)for(uint64_t part=0;part<=total;part++)
        assert(proportional_bytes(9999991,part,total)==9999991*part/total);

    #ifdef _WIN32
    _putenv("TZ=UTC0");_tzset();
    #else
    setenv("TZ","UTC0",1);tzset();
    #endif
    Config c;config_defaults(&c);int n;
    assert(parse_clock("00:00",&n)&&n==0);assert(parse_clock("23:59",&n)&&n==1439);
    assert(!parse_clock("24:00",&n));assert(!parse_clock("8:00",&n));assert(!parse_clock("12:60",&n));assert(!parse_clock("08:00x",&n));
    assert(is_peak(&c,480));assert(is_peak(&c,1439));assert(!is_peak(&c,0));assert(!is_peak(&c,479));
    c.peak_start=1320;c.peak_end=360;assert(is_peak(&c,0));assert(is_peak(&c,1320));assert(!is_peak(&c,360));assert(!is_peak(&c,1319));
    c.peak_end=c.peak_start;assert(is_peak(&c,600));config_defaults(&c);
    Usage u;usage_split(&c,at(2026,10,7,7,59,59),at(2026,10,7,8,0,1),101,51,&u);
    assert(u.down[0]==50 && u.down[1]==51 && u.up[0]==25 && u.up[1]==26);
    usage_split(&c,at(2026,10,7,23,59,59),at(2026,10,8,0,0,1),200,80,&u);assert(u.down[0]==100&&u.down[1]==100&&u.up[1]==40);
    usage_split(&c,at(2026,10,7,9,0,0),at(2026,10,7,9,0,0),7,3,&u);assert(u.down[0]==7 && u.up[0]==3);
    uint64_t d,up;assert(counter_delta(100,200,180,240,&d,&up)&&d==80&&up==40);
    assert(!counter_delta(100,200,90,240,&d,&up)&&d==0&&up==40);
    assert(!counter_delta(100,200,180,100,&d,&up)&&d==80&&up==0);
    assert(cycle_start(&c,at(2026,10,7,9,0,0))==at(2026,10,1,0,0,0));assert(cycle_next(&c,at(2026,12,31,23,0,0))==at(2027,1,1,0,0,0));
    c.reset_day=31;c.reset_minute=480;
    assert(cycle_start(&c,at(2026,2,28,7,59,59))==at(2026,1,31,8,0,0));
    assert(cycle_start(&c,at(2026,2,28,8,0,0))==at(2026,2,28,8,0,0));
    assert(cycle_next(&c,at(2026,2,28,8,0,0))==at(2026,3,31,8,0,0));
    assert(cycle_start(&c,at(2024,2,29,8,0,0))==at(2024,2,29,8,0,0));
    /* Windows CRT POSIX timezone format; reset in the spring DST gap normalizes forward. */
    #ifdef _WIN32
    _putenv("TZ=EST5EDT");_tzset();
    #else
    setenv("TZ","EST5EDT",1);tzset();
    #endif
    c.reset_day=8;c.reset_minute=150;
    time_t now=at(2026,3,8,12,0,0);assert(cycle_start(&c,now)<=now);assert(cycle_next(&c,now)>now);
    puts("PASS: clocks, overnight schedules, exact byte splits, counter resets, month ends, leap year, DST");return 0;
}
