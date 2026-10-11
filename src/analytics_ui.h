/* Included by ui.c after its shared drawing helpers. */
#define ANALYTICS_ROWS 2048
static AppTotal *app_rows,*recent_rows;
static unsigned app_row_count,recent_count,bins;
static uint64_t chart_down[32],chart_up[32],budget_used[2];
static time_t chart_edges[33];static wchar_t selected_app[APP_PATH];
static Forecast forecasts[2];static double observed_seconds[2],recent_rate[2];
static int active_band,analytics_ok;static ULONGLONG analytics_tick;
static const wchar_t *short_name(const wchar_t *path){const wchar_t *s=wcsrchr(path,L'\\');return s?s+1:path;}
static void analytics_free(void){free(app_rows);free(recent_rows);app_rows=recent_rows=NULL;selected_app[0]=0;app_row_count=recent_count=0;analytics_tick=0;}
static void analytics_refresh(HWND w,int force) {
    if(page!=6 && page!=7)return;ULONGLONG tick=GetTickCount64();if(!force && tick-analytics_tick<5000)return;analytics_tick=tick;
    if(!app_rows)app_rows=calloc(ANALYTICS_ROWS,sizeof(*app_rows));if(!recent_rows)recent_rows=calloc(ANALYTICS_ROWS,sizeof(*recent_rows));if(!app_rows || !recent_rows){analytics_ok=0;return;}
    time_t now=time(NULL);struct tm local=*localtime(&now);active_band=is_peak(&app.config,local.tm_hour*60+local.tm_min)?0:1;
    time_t cut=now-300;if(cut<app.tracking_since)cut=app.tracking_since;if(cut<app.day_start)cut=app.day_start;cut=(cut+59)/60*60;if(cut>now)cut=now;
    analytics_ok=db_apps(&app.db,cut,now+1,active_band,recent_rows,ANALYTICS_ROWS,&recent_count);
    for(int band=0;band<2;band++){
        uint64_t d[1],u[1];time_t edges[2]={app.day_start,app.day_end};
        if(!db_app_series(&app.db,edges,1,band,L"",d,u))analytics_ok=0;budget_used[band]=d[0]+u[0];
        edges[0]=cut;edges[1]=now+1;if(!db_app_series(&app.db,edges,1,band,L"",d,u))analytics_ok=0;
        observed_seconds[band]=(double)period_seconds(&app.config,cut,now,band);recent_rate[band]=observed_seconds[band]>0?(double)(d[0]+u[0])/observed_seconds[band]:0;
        time_t end=now;while(end<app.day_end){struct tm t=*localtime(&end);if((is_peak(&app.config,t.tm_hour*60+t.tm_min)?0:1)!=band)break;end+=60-t.tm_sec;}
        if(end>app.day_end)end=app.day_end;
        double observed=tracker_enabled() && app.tracking_since && now-app.tracking_heartbeat<=5 && !app.tracking_lost?observed_seconds[band]:0;
        forecasts[band]=budget_forecast((band?app.config.daily_offpeak_gb:app.config.daily_peak_gb)*1e9,budget_used[band],recent_rate[band],observed,(double)(end-now));
    }
    SYSTEMTIME chosen;memset(&chosen,0,sizeof(chosen));SendDlgItemMessageW(w,ID_APP_DATE,DTM_GETSYSTEMTIME,0,(LPARAM)&chosen);
    int monthly=SendDlgItemMessageW(w,ID_APP_RANGE,CB_GETCURSEL,0,0)==1;
    int minute=app.config.day_mode==1?app.config.peak_start:app.config.day_mode==2?app.config.day_minute:0;
    struct tm date={0};date.tm_year=chosen.wYear-1900;date.tm_mon=chosen.wMonth-1;date.tm_mday=monthly?1:chosen.wDay;date.tm_hour=minute/60;date.tm_min=minute%60;date.tm_isdst=-1;
    time_t start=mktime(&date);chart_edges[0]=start;
    if(monthly){int month=date.tm_mon;bins=0;do {date.tm_mday++;date.tm_isdst=-1;chart_edges[++bins]=mktime(&date);}while(date.tm_mon==month && bins<31);}
    else {time_t end;tracking_day(&app.config,start,&start,&end);bins=0;time_t t=start;while(t<end && bins<25){t+=3600;if(t>end)t=end;chart_edges[++bins]=t;}}
    int period=(int)SendDlgItemMessageW(w,ID_APP_PERIOD,CB_GETCURSEL,0,0)-1;
    if(!db_apps(&app.db,chart_edges[0],chart_edges[bins],period,app_rows,ANALYTICS_ROWS,&app_row_count))analytics_ok=0;
    if(!db_app_series(&app.db,chart_edges,bins,period,selected_app,chart_down,chart_up))analytics_ok=0;
    if(!analytics_ok){app_row_count=0;memset(chart_down,0,sizeof(chart_down));memset(chart_up,0,sizeof(chart_up));forecasts[0].state=forecasts[1].state=1;}
    HWND list=GetDlgItem(w,ID_APP_LIST);LRESULT top=SendMessageW(list,LB_GETTOPINDEX,0,0);SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_RESETCONTENT,0,0);
    for(unsigned i=0;i<app_row_count;i++){wchar_t row_name[640],d[48],u[48];amount(d,48,app_rows[i].down);amount(u,48,app_rows[i].up);swprintf(row_name,640,L"%ls - download %ls, upload %ls",short_name(app_rows[i].path),d,u);SendMessageW(list,LB_ADDSTRING,0,(LPARAM)row_name);if(!wcscmp(selected_app,app_rows[i].path))SendMessageW(list,LB_SETCURSEL,i,0);}
    SendMessageW(list,LB_SETTOPINDEX,top,0);SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,NULL,FALSE);
    SetDlgItemTextW(w,ID_APP_ENABLE,tracker_enabled()?L"Stop app tracking":L"Enable app tracking");
}
static void analytics_draw(HDC dc) {
    wchar_t text[256],value[48];COLORREF caution=app.dark?RGB(251,191,36):RGB(146,83,0);
    if(page==7){
        for(int band=0;band<2;band++){int x=band?360:32;amount(value,48,budget_used[band]);swprintf(text,256,L"Captured %ls: %ls",band?L"off-peak":L"peak",value);text_at(dc,text,x,574,290,app.muted);}return;
    }
    uint64_t total=0,max=0;for(unsigned i=0;i<bins;i++){uint64_t v=chart_down[i]+chart_up[i];total+=v;if(v>max)max=v;}
    amount(value,48,total);swprintf(text,256,L"%ls   /   %ls",*selected_app?short_name(selected_app):L"All applications",value);text_at(dc,text,24,255,632,app.fg);
    for(unsigned i=0;i<bins;i++){
        int x=200+(int)(i*632/bins),width=(int)(632/bins)-4;int h=max?(int)(76.0*(chart_down[i]+chart_up[i])/max):0;int dh=(chart_down[i]+chart_up[i])?(int)((double)h*chart_down[i]/(chart_down[i]+chart_up[i])):0;
        RECT r={px(x),px(354-h),px(x+width),px(354-h+dh)};HBRUSH b=CreateSolidBrush(app.accent);FillRect(dc,&r,b);DeleteObject(b);r.top=r.bottom;r.bottom=px(354);b=CreateSolidBrush(app.dark?RGB(94,234,212):RGB(13,148,136));FillRect(dc,&r,b);DeleteObject(b);
    }
    amount(value,48,max);text_at(dc,value,554,278,100,app.muted);
    HGDIOBJ tick_font=SelectObject(dc,app.small_font);
    for(unsigned i=0;i<bins;i++)if(i==0 || i% (bins>25?7:6)==0){struct tm t=*localtime(&chart_edges[i]);wchar_t label[16];wcsftime(label,16,bins>25?L"%d":L"%H:%M",&t);text_at(dc,label,24+(int)(i*632/bins),354,60,app.muted);}
    SelectObject(dc,tick_font);
    if(!max)text_at(dc,analytics_ok?L"No recorded app traffic in this range.":L"Could not load app history. Check storage.",24,306,620,app.muted);
    struct tm first=*localtime(&chart_edges[0]),last=*localtime(&chart_edges[bins]);wchar_t a[48],b[48];wcsftime(a,48,L"%d %b %H:%M",&first);wcsftime(b,48,L"%d %b %H:%M",&last);swprintf(text,256,L"%ls - %ls    Blue: download  /  Teal: upload",a,b);text_at(dc,text,24,378,632,app.muted);
    Forecast f=forecasts[active_band];const wchar_t *band=active_band?L"Off-peak":L"Peak";
    if(!tracker_enabled())wcscpy(text,L"Enable app tracking to collect usage and forecasts.");
    else if(app.tracking_lost)wcscpy(text,L"Capture incomplete. Forecasts are unavailable; restart tracking to retry.");
    else if(f.state==0)wcscpy(text,L"Set a daily budget to see usage cautions here.");
    else if(f.state==1)swprintf(text,256,L"Current %ls budget: collecting a full minute of recent activity...",band);
    else if(f.state==4)swprintf(text,256,L"Current %ls daily budget reached. High contributors are marked below.",band);
    else if(f.state==3)swprintf(text,256,L"Current %ls budget may run out in %.0f min at the recent average rate.",band,f.remaining_seconds/60);
    else swprintf(text,256,L"Current %ls budget: captured usage is on track at the recent rate.",band);
    text_at(dc,text,24,400,632,(f.state>=3 && !app.tracking_lost)?caution:app.muted);
    text_at(dc,app.tracking_status[0]?app.tracking_status:L"App history starts when you enable tracking. Administrator consent is optional.",24,622,632,app.muted);
}
static void analytics_row(DRAWITEMSTRUCT *item) {
    if(item->itemID>=app_row_count)return;AppTotal *a=&app_rows[item->itemID];HDC dc=item->hDC;RECT r=item->rcItem;
    HBRUSH back=CreateSolidBrush(item->itemState&ODS_SELECTED?surface:app.bg);FillRect(dc,&r,back);DeleteObject(back);SetBkMode(dc,TRANSPARENT);HGDIOBJ old=SelectObject(dc,app.font);
    double app_rate=0;for(unsigned i=0;i<recent_count;i++)if(!wcscmp(a->path,recent_rows[i].path) && observed_seconds[active_band]>0){app_rate=(double)(recent_rows[i].down+recent_rows[i].up)/observed_seconds[active_band];break;}
    int caution=tracker_enabled() && !app.tracking_lost && forecasts[active_band].state>=3 && app_rate>0 && app_rate>=recent_rate[active_band]*0.05;
    wchar_t text[256],d[48],u[48],total[48];amount(d,48,a->down);amount(u,48,a->up);amount(total,48,a->down+a->up);
    RECT line=r;line.left+=px(10);line.top+=px(5);line.right-=px(230);SetTextColor(dc,app.fg);DrawTextW(dc,short_name(a->path),-1,&line,DT_SINGLELINE|DT_END_ELLIPSIS);
    line=r;line.left=r.right-px(226);line.right-=px(10);line.top+=px(5);SetTextColor(dc,caution?(app.dark?RGB(251,191,36):RGB(146,83,0)):app.fg);swprintf(text,256,L"%ls%ls",caution?L"! High contributor  ":L"",total);DrawTextW(dc,text,-1,&line,DT_RIGHT|DT_SINGLELINE|DT_END_ELLIPSIS);
    line=r;line.left+=px(10);line.top+=px(26);line.right-=px(10);SetTextColor(dc,app.muted);swprintf(text,256,L"Download %ls    Upload %ls",d,u);DrawTextW(dc,text,-1,&line,DT_SINGLELINE);
    uint64_t largest=app_row_count?app_rows[0].down+app_rows[0].up:0;int width=largest?(int)((double)(r.right-r.left-px(20))*(a->down+a->up)/largest):0;RECT bar={r.left+px(10),r.bottom-px(5),r.left+px(10)+width,r.bottom-px(3)};HBRUSH fill=CreateSolidBrush(app.accent);FillRect(dc,&bar,fill);DeleteObject(fill);
    if((item->itemState&ODS_FOCUS)&&!(item->itemState&ODS_NOFOCUSRECT)){InflateRect(&r,-2,-2);DrawFocusRect(dc,&r);}SelectObject(dc,old);
}
