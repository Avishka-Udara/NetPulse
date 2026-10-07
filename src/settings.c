#define _CRT_SECURE_NO_WARNINGS
#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <wchar.h>
static void read_text(const wchar_t *section,const wchar_t *key,const wchar_t *def,wchar_t *out,DWORD size) {
    GetPrivateProfileStringW(section,key,def,out,size,app.ini);
}
static int read_clock(const wchar_t *key,int fallback) {
    wchar_t b[32];char a[32];read_text(L"plan",key,L"",b,32);
    WideCharToMultiByte(CP_UTF8,0,b,-1,a,32,NULL,NULL);int n;return parse_clock(a,&n)?n:fallback;
}
static double read_cap(const wchar_t *key,double fallback) {
    wchar_t b[64],*end;read_text(L"plan",key,L"",b,64);double v=wcstod(b,&end);
    return *b && !*end && isfinite(v) && v>=0 && v<=1000000?v:fallback;
}
int settings_load(void) {
    Config *c=&app.config;config_defaults(c);
    c->peak_start=read_clock(L"peak_start",c->peak_start);c->peak_end=read_clock(L"peak_end",c->peak_end);
    c->reset_minute=read_clock(L"reset_time",0);c->reset_day=GetPrivateProfileIntW(L"plan",L"reset_day",1,app.ini);
    if(c->reset_day<1 || c->reset_day>31)c->reset_day=1;
    c->peak_gb=read_cap(L"peak_gb",100);c->offpeak_gb=read_cap(L"offpeak_gb",100);
    c->position=GetPrivateProfileIntW(L"widget",L"position",2,app.ini);if(c->position<0 || c->position>2)c->position=2;
    c->offset=(int)GetPrivateProfileIntW(L"widget",L"offset",0,app.ini);if(c->offset < -5000 || c->offset>5000)c->offset=0;
    c->widget_width=GetPrivateProfileIntW(L"widget",L"width",136,app.ini);if(c->widget_width<136 || c->widget_width>320)c->widget_width=136;
    c->font_size=GetPrivateProfileIntW(L"widget",L"font_size",11,app.ini);if(c->font_size<10 || c->font_size>16)c->font_size=11;
    c->theme=GetPrivateProfileIntW(L"widget",L"theme",0,app.ini);if(c->theme<0 || c->theme>2)c->theme=0;
    c->transparent=GetPrivateProfileIntW(L"widget",L"transparent",1,app.ini)!=0;
    c->show_totals=GetPrivateProfileIntW(L"widget",L"show_totals",1,app.ini)!=0;
    wchar_t b[64],*end;read_text(L"network",L"adapter",L"0",b,64);c->adapter=_wcstoui64(b,&end,10);if(*end)c->adapter=0;
    return 1;
}
int settings_save(const Config *c) {
    wchar_t temp[MAX_PATH];swprintf(temp,MAX_PATH,L"%ls.tmp",app.ini);
    FILE *f=_wfopen(temp,L"wb");if(!f)return 0;
    int n=fprintf(f,"[plan]\r\npeak_start=%02d:%02d\r\npeak_end=%02d:%02d\r\npeak_gb=%.9g\r\noffpeak_gb=%.9g\r\nreset_day=%d\r\nreset_time=%02d:%02d\r\n[widget]\r\nposition=%d\r\noffset=%d\r\nwidth=%d\r\nfont_size=%d\r\ntheme=%d\r\ntransparent=%d\r\nshow_totals=%d\r\n[network]\r\nadapter=%llu\r\n",
        c->peak_start/60,c->peak_start%60,c->peak_end/60,c->peak_end%60,c->peak_gb,c->offpeak_gb,c->reset_day,c->reset_minute/60,c->reset_minute%60,c->position,c->offset,c->widget_width,c->font_size,c->theme,c->transparent,c->show_totals,(unsigned long long)c->adapter);
    int ok=n>0;if(fclose(f)!=0)ok=0;
    if(ok)ok=MoveFileExW(temp,app.ini,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(temp);return ok;
}
