/* Included by ui.c after its shared drawing helpers. */
#define ANALYTICS_ROWS 2048
static AppTotal *app_rows,*recent_rows;
static unsigned app_row_count,recent_count,bins;
static uint64_t chart_down[32],chart_up[32],budget_used[2];
static time_t chart_edges[33];static wchar_t selected_app[APP_PATH];
static Forecast forecasts[2];static double observed_seconds[2],recent_rate[2];
static uint64_t coverage_seconds,impaired_seconds;static time_t coverage_duration;
static int active_band,analytics_ok;static ULONGLONG analytics_tick;
static const wchar_t *short_name(const wchar_t *path){const wchar_t *s=wcsrchr(path,L'\\');return s?s+1:path;}
typedef struct {wchar_t path[APP_PATH];HICON icon;} AppIcon;
static AppIcon app_icons[128];static unsigned icon_count;
static HICON analytics_icon(const wchar_t *path){
    for(unsigned i=0;i<icon_count;i++)if(!wcscmp(path,app_icons[i].path))return app_icons[i].icon;
    /* Never resolve network paths or shell extensions just to decorate a row. */
    if(icon_count==128 || wcslen(path)<3 || path[1]!=L':' || GetDriveTypeW((wchar_t[]){path[0],L':',L'\\',0})!=DRIVE_FIXED)return NULL;
    AppIcon *entry=&app_icons[icon_count++];wcscpy(entry->path,path);HICON small=NULL;
    ExtractIconExW(path,0,NULL,&small,1);entry->icon=small;return small;
}
static void analytics_free(void){for(unsigned i=0;i<icon_count;i++)if(app_icons[i].icon)DestroyIcon(app_icons[i].icon);icon_count=0;free(app_rows);free(recent_rows);app_rows=recent_rows=NULL;selected_app[0]=0;app_row_count=recent_count=0;analytics_tick=0;}
static void analytics_refresh(HWND w,int force) {
    if(page!=6 && page!=7)return;ULONGLONG tick=GetTickCount64();if(!force && tick-analytics_tick<5000)return;analytics_tick=tick;
    if(!app_rows)app_rows=calloc(ANALYTICS_ROWS,sizeof(*app_rows));if(!recent_rows)recent_rows=calloc(ANALYTICS_ROWS,sizeof(*recent_rows));if(!app_rows || !recent_rows){analytics_ok=0;return;}
    app_insights(force);active_band=app.active_band;
    memcpy(budget_used,app.budget_used,sizeof(budget_used));memcpy(forecasts,app.forecast,sizeof(forecasts));
    memcpy(observed_seconds,app.observed,sizeof(observed_seconds));memcpy(recent_rate,app.recent_rate,sizeof(recent_rate));
    time_t now=time(NULL);analytics_ok=app.insights_ok && db_apps(&app.db,app.recent_cut,now+1,active_band,recent_rows,ANALYTICS_ROWS,&recent_count);
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
    time_t observed_end=chart_edges[bins]<now?chart_edges[bins]:now;coverage_duration=observed_end>chart_edges[0]?observed_end-chart_edges[0]:0;
    if(!db_capture_coverage(&app.db,chart_edges[0],observed_end,&coverage_seconds,&impaired_seconds))analytics_ok=0;
    EnableWindow(GetDlgItem(w,ID_APP_DETAILS),selected_app[0]!=0);
    if(!analytics_ok){app_row_count=0;memset(chart_down,0,sizeof(chart_down));memset(chart_up,0,sizeof(chart_up));forecasts[0].state=forecasts[1].state=1;}
    HWND list=GetDlgItem(w,ID_APP_LIST);LRESULT top=SendMessageW(list,LB_GETTOPINDEX,0,0);SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_RESETCONTENT,0,0);
    for(unsigned i=0;i<app_row_count;i++){wchar_t row_name[640],d[48],u[48];amount(d,48,app_rows[i].down);amount(u,48,app_rows[i].up);swprintf(row_name,640,L"%ls - download %ls, upload %ls",short_name(app_rows[i].path),d,u);SendMessageW(list,LB_ADDSTRING,0,(LPARAM)row_name);if(!wcscmp(selected_app,app_rows[i].path))SendMessageW(list,LB_SETCURSEL,i,0);}
    SendMessageW(list,LB_SETTOPINDEX,top,0);SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,NULL,FALSE);
    SetDlgItemTextW(w,ID_APP_ENABLE,tracker_enabled()?L"Stop app tracking":L"Enable app tracking");
}
static void analytics_draw(HDC dc) {
    wchar_t text[256],value[48];COLORREF caution=app.dark?RGB(251,191,36):RGB(146,83,0);
    if(page==7){
        for(int band=0;band<2;band++){int x=band?360:32;amount(value,48,budget_used[band]);swprintf(text,256,L"Captured %ls: %ls",band?L"off-peak":L"peak",value);text_at(dc,text,x,584,290,app.muted);}return;
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
    if(!analytics_ok)wcscpy(text,L"History unavailable. Check storage and retry.");
    else if(!coverage_duration)wcscpy(text,L"Future period: no capture expected yet.");
    else swprintf(text,256,L"%ls  /  Coverage %.1f%% of elapsed range%ls. Gaps are unknown usage.",tracker_enabled()?L"Capturing":L"Paused",100.0*coverage_seconds/coverage_duration,impaired_seconds?L" (event loss)":L"");
    text_at(dc,text,24,622,632,impaired_seconds?caution:app.muted);
}
static void analytics_row(DRAWITEMSTRUCT *item) {
    if(item->itemID>=app_row_count)return;AppTotal *a=&app_rows[item->itemID];HDC dc=item->hDC;RECT r=item->rcItem;
    HBRUSH back=CreateSolidBrush(item->itemState&ODS_SELECTED?surface:app.bg);FillRect(dc,&r,back);DeleteObject(back);SetBkMode(dc,TRANSPARENT);HGDIOBJ old=SelectObject(dc,app.font);
    double app_rate=0;for(unsigned i=0;i<recent_count;i++)if(!wcscmp(a->path,recent_rows[i].path) && observed_seconds[active_band]>0){app_rate=(double)(recent_rows[i].down+recent_rows[i].up)/observed_seconds[active_band];break;}
    int caution=tracker_enabled() && !app.tracking_lost && forecasts[active_band].state>=3 && app_rate>0 && app_rate>=recent_rate[active_band]*0.05;
    wchar_t text[256],d[48],u[48],total[48];amount(d,48,a->down);amount(u,48,a->up);amount(total,48,a->down+a->up);
    HICON icon=analytics_icon(a->path);if(icon)DrawIconEx(dc,r.left+px(10),r.top+px(6),icon,px(18),px(18),0,NULL,DI_NORMAL);
    RECT line=r;line.left+=px(36);line.top+=px(5);line.right-=px(230);SetTextColor(dc,app.fg);DrawTextW(dc,short_name(a->path),-1,&line,DT_SINGLELINE|DT_END_ELLIPSIS);
    line=r;line.left=r.right-px(226);line.right-=px(10);line.top+=px(5);SetTextColor(dc,caution?(app.dark?RGB(251,191,36):RGB(146,83,0)):app.fg);swprintf(text,256,L"%ls%ls",caution?L"! High contributor  ":L"",total);DrawTextW(dc,text,-1,&line,DT_RIGHT|DT_SINGLELINE|DT_END_ELLIPSIS);
    line=r;line.left+=px(10);line.top+=px(26);line.right-=px(10);SetTextColor(dc,app.muted);swprintf(text,256,L"Download %ls    Upload %ls",d,u);DrawTextW(dc,text,-1,&line,DT_SINGLELINE);
    uint64_t largest=app_row_count?app_rows[0].down+app_rows[0].up:0;int width=largest?(int)((double)(r.right-r.left-px(20))*(a->down+a->up)/largest):0;RECT bar={r.left+px(10),r.bottom-px(5),r.left+px(10)+width,r.bottom-px(3)};HBRUSH fill=CreateSolidBrush(app.accent);FillRect(dc,&bar,fill);DeleteObject(fill);
    if((item->itemState&ODS_FOCUS)&&!(item->itemState&ODS_NOFOCUSRECT)){InflateRect(&r,-2,-2);DrawFocusRect(dc,&r);}SelectObject(dc,old);
}

static void analytics_dialog(HWND w,const wchar_t *title,const wchar_t *heading,const wchar_t *content,const wchar_t *detail){
    TASKDIALOGCONFIG c={0};c.cbSize=sizeof(c);c.hwndParent=w;c.hInstance=app.instance;c.dwFlags=TDF_SIZE_TO_CONTENT|TDF_ALLOW_DIALOG_CANCELLATION;
    c.dwCommonButtons=TDCBF_CLOSE_BUTTON;c.pszWindowTitle=title;c.pszMainInstruction=heading;c.pszContent=content;c.pszExpandedInformation=detail;c.pszExpandedControlText=L"Technical details";c.cxWidth=370;
    TaskDialogIndirect(&c,NULL,NULL,NULL);
}
static void analytics_details(HWND w){
    if(!selected_app[0])return;wchar_t content[1500],today[48],yesterday[48],range[48],change[80];
    time_t now=time(NULL),previous,previous_end;tracking_day(&app.config,app.day_start-1,&previous,&previous_end);
    time_t today_edges[]={app.day_start,now+1},old_edges[]={previous,previous_end};uint64_t d=0,u=0,td,yd;
    if(!db_app_series(&app.db,today_edges,1,-1,selected_app,&d,&u))return;td=d+u;
    if(!db_app_series(&app.db,old_edges,1,-1,selected_app,&d,&u))return;yd=d+u;
    amount(today,48,td);amount(yesterday,48,yd);uint64_t total=0;for(unsigned i=0;i<bins;i++)total+=chart_down[i]+chart_up[i];amount(range,48,total);
    if(yd)swprintf(change,80,L"%+.1f%% versus the previous full tracking day",100.0*((double)td/yd-1));else wcscpy(change,L"No recorded previous-day usage to compare");
    uint64_t co,ci,po,pi;if(!db_capture_coverage(&app.db,app.day_start,now,&co,&ci) || !db_capture_coverage(&app.db,previous,previous_end,&po,&pi))return;
    swprintf(content,1500,L"Selected chart range: %ls\n\nToday so far (all traffic): %ls\nPrevious full tracking day: %ls\n%ls\n\nCapture coverage: today %.1f%% / previous day %.1f%%%ls\n\nThe chart behind this window shows this application's trend. Missing capture time is unknown, not zero. Comparisons use captured traffic, not an ISP bill.",range,today,yesterday,change,now>app.day_start?100.0*co/(now-app.day_start):0,100.0*po/(previous_end-previous),ci||pi?L"; event loss recorded":L"");
    analytics_dialog(w,L"Application details",short_name(selected_app),content,selected_app);
}
static void analytics_forecast(HWND w){
    app_insights(1);int b=app.active_band;double cap=(b?app.config.daily_offpeak_gb:app.config.daily_peak_gb)*1e9;time_t now=time(NULL);double seconds=(double)(app.forecast_end-now);double remaining=cap-app.budget_used[b];if(remaining<0)remaining=0;
    wchar_t content[1600],left[48],speed[48],safe[48],deadline[48],exhaustion[64];amount(left,48,(uint64_t)remaining);rate(speed,48,app.recent_rate[b]);rate(safe,48,seconds>0?remaining/seconds:0);
    struct tm end=*localtime(&app.forecast_end);wcsftime(deadline,48,L"%d %b, %H:%M",&end);Forecast f=app.forecast[b];
    if(f.state==4)wcscpy(exhaustion,L"Budget already reached");else if(f.state==3){time_t when=now+(time_t)f.remaining_seconds;struct tm t=*localtime(&when);wcsftime(exhaustion,64,L"%d %b, %H:%M (estimate)",&t);}else if(f.state==2)wcscpy(exhaustion,L"Not projected before this period ends");else wcscpy(exhaustion,L"Waiting for a healthy minute of capture");
    if(!cap)wcscpy(content,L"Set a daily peak or off-peak budget in Daily budgets to see a forecast.");
    else swprintf(content,1600,L"Remaining from captured usage: %ls\nRecent average (up to 5 minutes): %ls\nSustainable average until period end: %ls\n\nPeriod ends: %ls (%.0f minutes left)\nProjected exhaustion: %ls\n\nThis is a pacing estimate, not a guarantee. Paused capture, other devices, VPNs and provider accounting can change the actual remaining allowance.",left,speed,safe,deadline,seconds>0?seconds/60:0,exhaustion);
    wchar_t technical[512],started[48]=L"Not started this session",heartbeat[48]=L"None";
    if(app.tracking_since){struct tm t=*localtime(&app.tracking_since);wcsftime(started,48,L"%d %b, %H:%M:%S",&t);}
    if(app.tracking_heartbeat){struct tm t=*localtime(&app.tracking_heartbeat);wcsftime(heartbeat,48,L"%d %b, %H:%M:%S",&t);}
    swprintf(technical,512,L"%ls\nContinuous capture began: %ls\nLast heartbeat: %ls\nReported lost events/buffers: %llu",app.tracking_status,started,heartbeat,(unsigned long long)app.tracking_lost);
    analytics_dialog(w,L"Budget forecast",b?L"Current off-peak budget":L"Current peak budget",content,technical);
}
