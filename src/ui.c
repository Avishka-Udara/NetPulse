#define _CRT_SECURE_NO_WARNINGS
#include "app.h"
#include "taskbar.h"
#include <windowsx.h>
#include <shellapi.h>
#include <commdlg.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <wchar.h>
enum { ID_START=101,ID_END,ID_PEAK,ID_OFF,ID_DAY,ID_TIME,ID_POSITION,ID_OFFSET,ID_ADAPTER,ID_SAVE,ID_EXPORT,ID_CORRECT_PEAK,ID_CORRECT_OFF,ID_CORRECT,ID_RESET,ID_CLOSE,ID_RESET_TODAY,ID_WIDTH,ID_FONT,ID_THEME,ID_BACKGROUND,ID_TOTALS,ID_STARTUP,ID_ABOUT,ID_MODE,ID_BACKUP_SETTINGS,ID_RESTORE_SETTINGS,ID_BACKUP_DATA,ID_DATA_FOLDER,ID_RETRY,ID_TRACKING };
static AdapterChoice choices[MAX_ADAPTERS];static unsigned choice_count;
static int scale=96,dragging,drag_origin,drag_offset;
static int px(int n){return MulDiv(n,scale,96);}
static int page=0,creating_page=0,settings_dirty,loading_fields;
static HWND page_controls[96];static int control_pages[96],control_count;
static HFONT heading_font,value_font;
static COLORREF surface,border;
static HBRUSH surface_brush;
static const COLORREF transparent_key=RGB(1,2,3);
static COLORREF widget_text;
static int widget_hidden;
static HFONT widget_font;
static HDC widget_dc;
static HBITMAP widget_bitmap;
static HGDIOBJ widget_original;
static int bitmap_width,bitmap_height;
static void release_widget_buffer(void) {
    if(widget_dc){SelectObject(widget_dc,widget_original);DeleteObject(widget_bitmap);DeleteDC(widget_dc);}
    widget_dc=NULL;widget_bitmap=NULL;bitmap_width=bitmap_height=0;
}
static int widget_font_dpi,widget_font_size;
static COLORREF widget_bg;
static int menu_was_open;
static ULONGLONG menu_closed;
static void show_page(HWND w,int selected) {
    page=selected;
    for(int i=0;i<control_count;i++)ShowWindow(page_controls[i],control_pages[i]==page || control_pages[i]==-1 || (control_pages[i]==-2 && (page==1 || page==2 || page==4))?SW_SHOW:SW_HIDE);
    for(int i=200;i<=205;i++)InvalidateRect(GetDlgItem(w,i),NULL,TRUE);
    InvalidateRect(w,NULL,TRUE);
}
static void amount(wchar_t *b,size_t n,uint64_t bytes) {
    double v=(double)bytes;const wchar_t *unit=L"B";
    if(v>=1e12){v/=1e12;unit=L"TB";}else if(v>=1e9){v/=1e9;unit=L"GB";}else if(v>=1e6){v/=1e6;unit=L"MB";}else if(v>=1e3){v/=1e3;unit=L"KB";}
    swprintf(b,n,L"%.2f %ls",v,unit);
}
static void rate(wchar_t *b,size_t n,double value) {
    const wchar_t *unit=L"B/s";
    if(value>=1e9){value/=1e9;unit=L"GB/s";}else if(value>=1e6){value/=1e6;unit=L"MB/s";}else if(value>=1e3){value/=1e3;unit=L"KB/s";}
    swprintf(b,n,L"%.1f %ls",value,unit);
}
/* Three significant digits; suffixes always describe decimal bytes. */
static void compact(wchar_t *b,size_t n,double value,int speed) {
    const wchar_t *units[]={L"B",L"K",L"M",L"G",L"T",L"P",L"E"};int u=0;
    while(value>=999.5 && u<6){value/=1000;u++;}
    swprintf(b,n,L"%.*f%ls%ls",value<99.95 && u?1:0,value,units[u],speed?L"/s":L"");
}
void ui_theme(void) {
    DWORD light=1,size=sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"SystemUsesLightTheme",RRF_RT_REG_DWORD,NULL,&light,&size);
    if(app.config.theme)light=app.config.theme==1;
    widget_bg=light?RGB(247,248,250):RGB(17,20,25);
    HIGHCONTRASTW hc={0};hc.cbSize=sizeof(hc);
    SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0);
    widget_text=(hc.dwFlags&HCF_HIGHCONTRASTON)?GetSysColor(COLOR_WINDOWTEXT):(light?RGB(20,20,20):RGB(245,245,245));
    size=sizeof(light);RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,NULL,&light,&size);
    if(app.config.theme)light=app.config.theme==1;
    app.dark=!light;app.bg=light?RGB(250,250,250):RGB(24,24,27);app.fg=light?RGB(24,24,27):RGB(244,244,245);
    app.muted=light?RGB(82,82,91):RGB(161,161,170);app.accent=light?RGB(37,99,235):RGB(96,165,250);
    surface=light?RGB(255,255,255):RGB(32,32,36);border=light?RGB(228,228,231):RGB(55,55,61);
    if(surface_brush)DeleteObject(surface_brush);surface_brush=CreateSolidBrush(surface);
    if(app.background)DeleteObject(app.background);app.background=CreateSolidBrush(app.bg);
    if(!app.font){app.font=CreateFontW(-MulDiv(14,(int)GetDpiForSystem(),96),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        app.small_font=CreateFontW(-12,0,0,0,FW_MEDIUM,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");}
    if(app.settings){BOOL dark=app.dark;DwmSetWindowAttribute(app.settings,20,&dark,sizeof(dark));RedrawWindow(app.settings,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);}
    if(app.widget)InvalidateRect(app.widget,NULL,TRUE);
}
void ui_tray(int add) {
    NOTIFYICONDATAW n={0};n.cbSize=sizeof(n);n.hWnd=app.controller;n.uID=1;n.uFlags=add?(NIF_ICON|NIF_MESSAGE|NIF_TIP):NIF_TIP;
    n.hIcon=app.icon;n.uCallbackMessage=WM_TRAY;
    wchar_t down[40],up[40];rate(down,40,app.down_rate);rate(up,40,app.up_rate);
    swprintf(n.szTip,128,L"NetPulse | Down %ls | Up %ls\n%ls",down,up,!app.storage_ok?L"Storage error - open settings":!app.attached?L"Taskbar attachment unavailable - open settings":widget_hidden?L"Taskbar full - tracking continues":L"Click for usage and settings");
    static wchar_t previous_tip[128];
    if(!add && wcscmp(previous_tip,n.szTip)==0)return;
    if(Shell_NotifyIconW(add?NIM_ADD:NIM_MODIFY,&n))wcscpy(previous_tip,n.szTip);
}
void ui_attach(void) {
    taskbar_enable(!app.config.tray_only);
    if(app.config.tray_only){if(IsWindow(app.widget))DestroyWindow(app.widget);widget_hidden=0;app.attached=1;return;}
    HWND bar=FindWindowW(L"Shell_TrayWnd",NULL);
    if(!bar){app.attached=0;return;}
    if(!IsWindow(app.widget) || app.taskbar!=bar) {
        if(IsWindow(app.widget))DestroyWindow(app.widget);app.widget=NULL;app.taskbar=bar;
        DPI_AWARENESS_CONTEXT old=SetThreadDpiAwarenessContext(GetWindowDpiAwarenessContext(bar));
        app.widget=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_LAYERED,L"NetPulse.Widget",L"NetPulse bandwidth",WS_CHILD|WS_CLIPSIBLINGS,
            0,0,160,40,bar,NULL,app.instance,NULL);
        SetThreadDpiAwarenessContext(old);
        app.attached=app.widget!=NULL && GetParent(app.widget)==bar;
        if(!app.attached)return;
        /* Explorer's composited taskbar otherwise treats the GDI surface as transparent. */
        SetLayeredWindowAttributes(app.widget,transparent_key,255,LWA_ALPHA|(app.config.transparent?LWA_COLORKEY:0));
    }
    if(taskbar_menu_active(bar)){menu_was_open=1;return;}
    if(menu_was_open){menu_was_open=0;menu_closed=GetTickCount64();}
    /* Give Explorer a bounded recovery interval after its modal menu closes. */
    if(menu_closed && GetTickCount64()-menu_closed<1600)return;
    SetLayeredWindowAttributes(app.widget,transparent_key,255,LWA_ALPHA|(app.config.transparent?LWA_COLORKEY:0));
    RECT r;GetClientRect(bar,&r);int dpi=(int)GetDpiForWindow(bar);if(!dpi)dpi=96;
    int effective=app.config.widget_width;
    int minimum=MulDiv(136,app.config.font_size,11);if(effective<minimum)effective=minimum;
    if(!app.config.show_totals)effective=MulDiv(effective,3,5);
    int width=MulDiv(effective,dpi,96),height=MulDiv(app.config.font_size*2+10,dpi,96);
    int x=0,y=(r.bottom-height)/2;
    int left=MulDiv(6,dpi,96),right=r.right-MulDiv(6,dpi,96);
    HWND tray=FindWindowExW(bar,NULL,L"TrayNotifyWnd",NULL);RECT tr;
    if(tray && GetWindowRect(tray,&tr)){MapWindowPoints(NULL,bar,(POINT*)&tr,2);right=tr.left-MulDiv(6,dpi,96);}
    int preferred=app.config.position==0?left:app.config.position==1?(r.right-width)/2:right-width;
    preferred+=MulDiv(app.config.offset,dpi,96);
    /* Unknown layouts and vertical taskbars fall back to the notification icon. */
    widget_hidden=r.right<r.bottom || height>r.bottom || !taskbar_space(bar,left,right,width,preferred,MulDiv(6,dpi,96),&x);
    if(widget_hidden)ShowWindow(app.widget,SW_HIDE);
    else {
        RECT current;GetWindowRect(app.widget,&current);MapWindowPoints(NULL,bar,(POINT*)&current,2);
        if(!IsWindowVisible(app.widget) || current.left!=x || current.top!=y || current.right-current.left!=width || current.bottom-current.top!=height)
            SetWindowPos(app.widget,HWND_TOP,x,y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    }
    app.attached=GetParent(app.widget)==bar;
}
void ui_menu(HWND owner) {
    HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,CMD_SETTINGS,L"Usage && settings...");
    AppendMenuW(menu,MF_STRING,CMD_REATTACH,L"Reattach to taskbar");AppendMenuW(menu,MF_SEPARATOR,0,NULL);
    AppendMenuW(menu,MF_STRING,CMD_EXIT,L"Exit NetPulse");POINT p;GetCursorPos(&p);SetForegroundWindow(app.controller);
    int cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,owner,NULL);DestroyMenu(menu);
    if(cmd)PostMessageW(app.controller,WM_COMMAND,(WPARAM)cmd,0);PostMessageW(app.controller,WM_NULL,0,0);
}
LRESULT CALLBACK widget_proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    switch(m) {
    case WM_ERASEBKGND:return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);
        if(!widget_dc || bitmap_width!=r.right || bitmap_height!=r.bottom){
            release_widget_buffer();widget_dc=CreateCompatibleDC(dc);widget_bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);
            if(!widget_dc || !widget_bitmap){if(widget_dc)DeleteDC(widget_dc);if(widget_bitmap)DeleteObject(widget_bitmap);widget_dc=NULL;widget_bitmap=NULL;EndPaint(w,&ps);return 0;}
            widget_original=SelectObject(widget_dc,widget_bitmap);bitmap_width=r.right;bitmap_height=r.bottom;
        }
        HDC mem=widget_dc;
        HBRUSH clear=CreateSolidBrush(app.config.transparent?transparent_key:widget_bg);FillRect(mem,&r,clear);DeleteObject(clear);SetBkMode(mem,TRANSPARENT);SetTextColor(mem,widget_text);
        int dpi=(int)GetDpiForWindow(w);
        if(!widget_font || widget_font_dpi!=dpi || widget_font_size!=app.config.font_size){
            if(widget_font)DeleteObject(widget_font);widget_font_dpi=dpi;widget_font_size=app.config.font_size;
            widget_font=CreateFontW(-MulDiv(app.config.font_size,dpi,96),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,NONANTIALIASED_QUALITY,0,L"Segoe UI");
        }
        HGDIOBJ of=SelectObject(mem,widget_font);
        wchar_t live[40],total[40],text[120];
        for(int row=0;row<2;row++) {
            compact(live,40,row?app.up_rate:app.down_rate,1);compact(total,40,(double)(row?app.today_up:app.today_down),0);
            RECT line={MulDiv(2,dpi,96),row*r.bottom/2,app.config.show_totals?r.right*61/100:r.right,(row+1)*r.bottom/2};
            swprintf(text,120,L"%ls %ls",row?L"\x2191":L"\x2193",live);SetTextColor(mem,widget_text);DrawTextW(mem,text,-1,&line,DT_SINGLELINE|DT_VCENTER);
            if(!app.config.show_totals)continue;
            line.left=r.right*63/100;line.right=r.right-MulDiv(2,dpi,96);
            SetTextColor(mem,widget_text);DrawTextW(mem,total,-1,&line,DT_SINGLELINE|DT_VCENTER|DT_RIGHT|DT_END_ELLIPSIS);
        }
        BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,of);EndPaint(w,&ps);return 0;
    }
    case WM_LBUTTONDOWN:
        if(GetKeyState(VK_CONTROL)<0){POINT p;GetCursorPos(&p);dragging=1;drag_origin=p.x;drag_offset=app.config.offset;SetCapture(w);}return 0;
    case WM_MOUSEMOVE:
        if(dragging){POINT p;GetCursorPos(&p);app.config.offset=drag_offset+MulDiv(p.x-drag_origin,96,(int)GetDpiForWindow(w));ui_attach();}return 0;
    case WM_LBUTTONUP:
        if(dragging){dragging=0;ReleaseCapture();if(!settings_save(&app.config))app_error(NULL,L"Could not save widget position.");}else ui_settings();return 0;
    case WM_CAPTURECHANGED:dragging=0;return 0;
    case WM_RBUTTONUP:ui_menu(app.controller);return 0;
    case WM_NCDESTROY:release_widget_buffer();if(app.widget==w)app.widget=NULL;break;
    }
    return DefWindowProcW(w,m,wp,lp);
}
/* Keep native combo keyboard/accessibility behavior; paint its frame and arrow
   to match our palette instead of leaving the default bright chrome in dark mode. */
static LRESULT CALLBACK combo_proc(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
    (void)data;
    LRESULT result=DefSubclassProc(w,m,wp,lp);
    if(m==WM_PAINT){
        COMBOBOXINFO info={0};info.cbSize=sizeof(info);
        if(GetComboBoxInfo(w,&info)){
            HDC dc=GetDC(w);RECT r;GetClientRect(w,&r);HBRUSH b=CreateSolidBrush(surface);
            int saved=SaveDC(dc);ExcludeClipRect(dc,info.rcItem.left,info.rcItem.top,info.rcItem.right,info.rcItem.bottom);FillRect(dc,&r,b);RestoreDC(dc,saved);
            FillRect(dc,&info.rcButton,b);DeleteObject(b);
            b=CreateSolidBrush(((GetFocus()==w || IsChild(w,GetFocus())) && !(SendMessageW(GetParent(w),WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS))?app.accent:surface);FrameRect(dc,&r,b);DeleteObject(b);
            int x=(info.rcButton.left+info.rcButton.right)/2,y=(info.rcButton.top+info.rcButton.bottom)/2;
            HPEN pen=CreatePen(PS_SOLID,px(1),app.fg);HGDIOBJ old=SelectObject(dc,pen);
            MoveToEx(dc,x-px(3),y-px(1),NULL);LineTo(dc,x,y+px(2));LineTo(dc,x+px(4),y-px(2));SelectObject(dc,old);DeleteObject(pen);ReleaseDC(w,dc);
        }
    }
    if(m==WM_NCDESTROY)RemoveWindowSubclass(w,combo_proc,id);
    return result;
}
static HWND control(HWND parent,const wchar_t *cls,const wchar_t *text,int id,int x,int y,int width,int height,DWORD style) {
    HWND w=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style|(wcscmp(cls,L"BUTTON")==0?BS_OWNERDRAW:0),px(x),px(y),px(width),px(height),parent,(HMENU)(INT_PTR)id,app.instance,NULL);
    if(wcscmp(cls,L"COMBOBOX")==0){SetWindowSubclass(w,combo_proc,1,0);SendMessageW(w,CB_SETMINVISIBLE,7,0);}
    if(control_count<96){page_controls[control_count]=w;control_pages[control_count++]=creating_page;}
    SendMessageW(w,WM_SETFONT,(WPARAM)app.font,TRUE);if(wcscmp(cls,L"EDIT")==0)SendMessageW(w,EM_SETLIMITTEXT,32,0);return w;
}
static void label(HWND w,const wchar_t *t,int x,int y,int width){control(w,L"STATIC",t,0,x,y,width,20,0);}
static void edit(HWND w,int id,const wchar_t *t,int x,int y,int width){control(w,L"EDIT",t,id,x,y,width,30,WS_TABSTOP|ES_AUTOHSCROLL);}
static void clock_input(HWND w,int id,int x,int y,int width) {
    HWND combo=control(w,L"COMBOBOX",L"",id,x,y,width,180,WS_TABSTOP|CBS_DROPDOWN|CBS_AUTOHSCROLL|WS_VSCROLL);
    for(int minute=0;minute<1440;minute+=30){wchar_t text[8];swprintf(text,8,L"%02d:%02d",minute/60,minute%60);SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)text);}
    SendMessageW(combo,CB_LIMITTEXT,5,0);
}
static void set_number(HWND w,int id,double n){wchar_t b[64];swprintf(b,64,L"%.6g",n);SetDlgItemTextW(w,id,b);}
static void set_clock(HWND w,int id,int n){wchar_t b[16];swprintf(b,16,L"%02d:%02d",n/60,n%60);SetDlgItemTextW(w,id,b);}
static int get_number(HWND w,int id,double min,double max,double *out) {
    wchar_t b[64],*end;GetDlgItemTextW(w,id,b,64);*out=wcstod(b,&end);return *b && !*end && isfinite(*out) && *out>=min && *out<=max;
}
static int get_clock(HWND w,int id,int *out) {
    wchar_t b[32];char a[64];GetDlgItemTextW(w,id,b,32);WideCharToMultiByte(CP_UTF8,0,b,-1,a,64,NULL,NULL);return parse_clock(a,out);
}
static void settings_fields(HWND w) {
    loading_fields=1;control_count=0;creating_page=-1;
    const wchar_t *tabs[]={L"Overview",L"Data plan",L"Taskbar",L"Adjust usage",L"Appearance",L"Manage"};
    for(int i=0;i<6;i++)control(w,L"BUTTON",tabs[i],200+i,24+i*106,82,100,36,WS_TABSTOP);
    control(w,L"BUTTON",L"Help and about",ID_ABOUT,620,24,36,32,WS_TABSTOP);
    control(w,L"BUTTON",L"Close",ID_CLOSE,544,550,112,36,WS_TABSTOP);
    creating_page=1;
    label(w,L"Peak starts",32,206,260);clock_input(w,ID_START,32,236,280);
    label(w,L"Peak ends",360,206,260);clock_input(w,ID_END,360,236,280);
    label(w,L"Peak allowance (GB)",32,290,290);edit(w,ID_PEAK,L"",40,320,264);
    label(w,L"Off-peak allowance (GB)",360,290,290);edit(w,ID_OFF,L"",368,320,264);
    label(w,L"Monthly reset day",32,374,260);edit(w,ID_DAY,L"",40,404,264);
    label(w,L"Reset time",360,374,260);clock_input(w,ID_TIME,360,404,280);
    label(w,L"24-hour local time. Zero allowance means unlimited.",32,464,590);
    label(w,L"Off-peak covers the remaining hours. Days 29-31 adjust to shorter months.",32,488,620);
    creating_page=2;
    label(w,L"Position",32,206,270);
    HWND combo=control(w,L"COMBOBOX",L"",ID_POSITION,32,236,280,160,WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL);
    SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Far left");SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Center (nearest free space)");SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Right (beside system tray)");
    label(w,L"Fine offset (pixels)",360,206,270);edit(w,ID_OFFSET,L"",368,236,264);
    label(w,L"Network adapter",32,310,400);
    combo=control(w,L"COMBOBOX",L"",ID_ADAPTER,32,340,608,200,WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL);
    SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Automatic - physical adapters with a default route");
    choice_count=network_choices(choices,MAX_ADAPTERS);int selected=0;
    for(unsigned i=0;i<choice_count;i++){SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)choices[i].name);if(choices[i].luid==app.config.adapter)selected=(int)i+1;}
    if(app.config.adapter && !selected && choice_count<MAX_ADAPTERS){choices[choice_count].luid=app.config.adapter;SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Saved adapter (currently unavailable)");selected=(int)++choice_count;}
    SendMessageW(combo,CB_SETCURSEL,(WPARAM)selected,0);
    label(w,L"Live speed (left), today's totals (right). Adapter traffic may exceed ISP usage.",32,406,608);
    label(w,L"Moves to free space when crowded; uses the tray icon if no space fits.",32,436,620);
    label(w,L"Start with Windows",32,470,270);
    combo=control(w,L"COMBOBOX",L"",ID_STARTUP,360,464,280,120,WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS);
    SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Off");SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"On - at sign-in");SendMessageW(combo,CB_SETCURSEL,startup_enabled(),0);
    creating_page=4;
    label(w,L"Theme",32,206,270);
    combo=control(w,L"COMBOBOX",L"",ID_THEME,32,236,280,160,WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS);
    const wchar_t *themes[]={L"Follow Windows",L"Light",L"Dark"};for(int i=0;i<3;i++)SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)themes[i]);
    SendMessageW(combo,CB_SETCURSEL,app.config.theme,0);
    label(w,L"Background",360,206,270);
    combo=control(w,L"COMBOBOX",L"",ID_BACKGROUND,360,236,280,160,WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS);
    SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Solid");SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Transparent");SendMessageW(combo,CB_SETCURSEL,app.config.transparent,0);
    label(w,L"Widget width (136-320 px)",32,290,280);edit(w,ID_WIDTH,L"",40,320,264);set_number(w,ID_WIDTH,app.config.widget_width);
    label(w,L"Text size (10-16 px)",360,290,280);edit(w,ID_FONT,L"",368,320,264);set_number(w,ID_FONT,app.config.font_size);
    label(w,L"Display",32,374,270);
    combo=control(w,L"COMBOBOX",L"",ID_TOTALS,32,404,608,160,WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS);
    SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Live speeds only");SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Live speeds and today's totals");SendMessageW(combo,CB_SETCURSEL,app.config.show_totals,0);
    label(w,L"Run mode",32,472,260);
    combo=control(w,L"COMBOBOX",L"",ID_MODE,360,464,280,130,WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS);
    SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Taskbar widget + tray");SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Tray only (lower overhead)");SendMessageW(combo,CB_SETCURSEL,app.config.tray_only,0);
    creating_page=5;
    label(w,L"Settings backup",32,218,600);
    control(w,L"BUTTON",L"Export settings...",ID_BACKUP_SETTINGS,32,252,190,36,WS_TABSTOP);
    control(w,L"BUTTON",L"Restore settings...",ID_RESTORE_SETTINGS,238,252,190,36,WS_TABSTOP);
    label(w,L"Includes plan and appearance. Windows startup remains unchanged.",32,306,620);
    label(w,L"Usage database",32,354,600);
    control(w,L"BUTTON",L"Back up history...",ID_BACKUP_DATA,32,388,190,36,WS_TABSTOP);
    control(w,L"BUTTON",L"Open data folder",ID_DATA_FOLDER,238,388,190,36,WS_TABSTOP);
    control(w,L"BUTTON",L"Retry saving",ID_RETRY,444,388,180,36,WS_TABSTOP);
    label(w,L"Backup uses SQLite's snapshot API, including committed WAL data.",32,442,620);
    label(w,L"For recovery, exit NetPulse before replacing the database from a backup.",32,470,620);
    creating_page=3;
    label(w,L"Peak used (GB)",32,238,280);edit(w,ID_CORRECT_PEAK,L"0",40,268,264);
    label(w,L"Off-peak used (GB)",360,238,280);edit(w,ID_CORRECT_OFF,L"0",368,268,264);
    control(w,L"BUTTON",L"Apply totals",ID_CORRECT,32,330,160,36,WS_TABSTOP);
    control(w,L"BUTTON",L"Reset cycle usage",ID_RESET,208,330,180,36,WS_TABSTOP);
    label(w,L"Adjustments affect your billing cycle only. Today's traffic stays unchanged.",32,406,620);
    control(w,L"BUTTON",L"Reset today only",ID_RESET_TODAY,32,448,180,36,WS_TABSTOP);
    label(w,L"Resets the daily display; preserves history and your billing cycle.",32,494,620);
    creating_page=0;
    control(w,L"BUTTON",L"Export history...",ID_EXPORT,24,550,160,36,WS_TABSTOP);
    control(w,L"BUTTON",L"Tracking details",ID_TRACKING,200,550,160,36,WS_TABSTOP);
    creating_page=-2;
    control(w,L"BUTTON",L"Save settings",ID_SAVE,380,550,148,36,WS_TABSTOP);
    set_clock(w,ID_START,app.config.peak_start);set_clock(w,ID_END,app.config.peak_end);set_clock(w,ID_TIME,app.config.reset_minute);
    set_number(w,ID_PEAK,app.config.peak_gb);set_number(w,ID_OFF,app.config.offpeak_gb);set_number(w,ID_DAY,app.config.reset_day);
    set_number(w,ID_OFFSET,app.config.offset);SendDlgItemMessageW(w,ID_POSITION,CB_SETCURSEL,(WPARAM)app.config.position,0);
    set_number(w,ID_CORRECT_PEAK,(app.usage.down[0]+app.usage.up[0])/1e9);set_number(w,ID_CORRECT_OFF,(app.usage.down[1]+app.usage.up[1])/1e9);
    loading_fields=0;settings_dirty=0;
}
static void panel(HDC dc,int x,int y,int width,int height) {
    HGDIOBJ oldBrush=SelectObject(dc,surface_brush);HPEN pen=CreatePen(PS_NULL,0,surface);HGDIOBJ oldPen=SelectObject(dc,pen);
    RoundRect(dc,px(x),px(y),px(x+width),px(y+height),px(12),px(12));SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(pen);
}
static void text_at(HDC dc,const wchar_t *text,int x,int y,int width,COLORREF color) {
    SetTextColor(dc,color);RECT r={px(x),px(y),px(x+width),px(y+38)};DrawTextW(dc,text,-1,&r,DT_SINGLELINE|DT_END_ELLIPSIS);
}
static void summary(HWND w,HDC dc) {
    (void)w;SetBkMode(dc,TRANSPARENT);HGDIOBJ old=SelectObject(dc,heading_font);
    text_at(dc,L"NetPulse",24,22,400,app.fg);SelectObject(dc,app.font);
    text_at(dc,L"Network activity, at a glance",24,51,440,app.muted);
    
    const wchar_t *titles[]={L"Today's activity",L"Your data plan",L"Taskbar",L"Usage adjustments",L"Appearance",L"Backup and recovery"};
    const wchar_t *descriptions[]={L"Recorded today, or since your last daily reset",L"Flexible allowances, on your schedule",L"Widget placement and the connection to monitor",L"Enter your provider's current-cycle usage",L"Customize the widget without changing your usage",L"Keep a portable copy of your settings and history"};
    SelectObject(dc,heading_font);text_at(dc,titles[page],24,142,630,app.fg);SelectObject(dc,app.font);text_at(dc,descriptions[page],24,174,630,app.muted);
    wchar_t b[256],value[48],live[48];
    if(page==0) {
        for(int i=0;i<2;i++) {
            int x=i?352:24;panel(dc,x,212,304,124);
            text_at(dc,i?L"UPLOAD":L"DOWNLOAD",x+20,230,264,app.muted);
            amount(value,48,i?app.today_up:app.today_down);SelectObject(dc,value_font);text_at(dc,value,x+20,262,264,app.fg);SelectObject(dc,app.font);
            rate(live,48,i?app.up_rate:app.down_rate);swprintf(b,256,L"%ls  right now",live);text_at(dc,b,x+20,306,264,app.accent);
        }
        text_at(dc,L"THIS BILLING CYCLE",24,360,600,app.muted);
        for(int i=0;i<2;i++) {
            int x=i?352:24;double used=(app.usage.down[i]+app.usage.up[i])/1e9,cap=i?app.config.offpeak_gb:app.config.peak_gb;
            swprintf(b,256,L"%ls   %.2f GB",i?L"Off-peak":L"Peak",used);text_at(dc,b,x,396,304,app.fg);
            RECT track={px(x),px(428),px(x+304),px(432)};HBRUSH brush=CreateSolidBrush(border);FillRect(dc,&track,brush);DeleteObject(brush);
            double fraction=cap>0?used/cap:0;if(fraction>1)fraction=1;track.right=track.left+(int)(px(304)*fraction);
            brush=CreateSolidBrush(cap>0 && used>cap?RGB(230,120,90):app.accent);FillRect(dc,&track,brush);DeleteObject(brush);
            if(cap>0)swprintf(b,256,L"%.2f GB %ls",fabs(cap-used),used>cap?L"over allowance":L"remaining");else wcscpy(b,L"Unlimited allowance");text_at(dc,b,x,446,304,app.muted);
        }
        struct tm t=*localtime(&app.next_cycle);wchar_t date[40];wcsftime(date,40,L"%d %b, %H:%M",&t);
        swprintf(b,256,L"Resets %ls  /  System uptime %lluh",date,(unsigned long long)(GetTickCount64()/3600000));text_at(dc,b,24,492,632,app.muted);
    } else {
        if(page==1)for(int row=0;row<3;row++)for(int col=0;col<2;col++)panel(dc,col?360:32,230+84*row,280,42);
        if(page==2)panel(dc,360,230,280,42);
        if(page==4){panel(dc,32,314,280,42);panel(dc,360,314,280,42);}
        if(page==3){panel(dc,32,262,280,42);panel(dc,360,262,280,42);}
    }
    const wchar_t *status=app.status[0]?app.status:settings_dirty?L"Unsaved changes - choose Save settings to apply them.":L"Both directions count. Daily totals exclude plan adjustments.";
    if(!app.storage_ok){MultiByteToWideChar(CP_UTF8,0,app.db.error,-1,b,256);status=b;}
    else if(!app.status[0] && !settings_dirty && !app.attached)status=L"Taskbar unavailable. Use the tray icon to open NetPulse.";
    else if(!app.status[0] && !settings_dirty && app.config.tray_only)status=L"Tray-only mode. Taskbar scanning is paused; traffic tracking continues.";
    else if(!app.status[0] && !settings_dirty && widget_hidden)status=L"Widget hidden: no verified free space. Tracking continues in the tray.";
    else if(!app.status[0] && !settings_dirty && !app.network_ok)status=L"Network counters unavailable. Retrying...";
    else if(!app.status[0] && !settings_dirty && !app.network.active_count)status=L"No active adapter. Check your connection or adapter selection.";
    text_at(dc,status,24,610,632,app.storage_ok?app.muted:RGB(230,120,90));SelectObject(dc,old);
}
static void draw_button(DRAWITEMSTRUCT *item) {
    FillRect(item->hDC,&item->rcItem,app.background);
    int id=(int)item->CtlID,nav=id>=200 && id<=205,active=nav && page==id-200;
    int primary=id==ID_SAVE;
    COLORREF fill=primary?app.accent:((active || !nav)?surface:app.bg);
    if(item->itemState&ODS_SELECTED)fill=surface;
    HBRUSH brush=CreateSolidBrush(fill);HPEN pen=CreatePen(PS_SOLID,1,fill);
    HGDIOBJ ob=SelectObject(item->hDC,brush),op=SelectObject(item->hDC,pen),of=SelectObject(item->hDC,app.font);
    RECT r=item->rcItem;RoundRect(item->hDC,r.left,r.top,r.right,r.bottom,px(10),px(10));
    wchar_t text[80];GetWindowTextW(item->hwndItem,text,80);if(id==ID_ABOUT)wcscpy(text,L"?");SetBkMode(item->hDC,TRANSPARENT);
    SetTextColor(item->hDC,primary?(app.dark?RGB(15,23,42):RGB(255,255,255)):(active?app.fg:(nav?app.muted:app.fg)));
    DrawTextW(item->hDC,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if(active){RECT underline={r.left+px(22),r.bottom-px(2),r.right-px(22),r.bottom};HBRUSH line=CreateSolidBrush(app.accent);FillRect(item->hDC,&underline,line);DeleteObject(line);}
    if((item->itemState&ODS_FOCUS) && !(item->itemState&ODS_NOFOCUSRECT) && !(SendMessageW(GetParent(item->hwndItem),WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){InflateRect(&r,-4,-4);DrawFocusRect(item->hDC,&r);}
    SelectObject(item->hDC,of);SelectObject(item->hDC,op);SelectObject(item->hDC,ob);DeleteObject(pen);DeleteObject(brush);
}
static void validation_error(HWND w,int control_id,const wchar_t *message) {
    wcsncpy(app.status,message,255);app.status[255]=0;
    HWND field=GetDlgItem(w,control_id);for(int i=0;i<control_count;i++)if(page_controls[i]==field && control_pages[i]>=0){show_page(w,control_pages[i]);break;}SetFocus(field);
    SendMessageW(w,WM_CHANGEUISTATE,MAKEWPARAM(UIS_CLEAR,UISF_HIDEFOCUS),0);
    RECT footer={0,px(604),px(680),px(650)};InvalidateRect(w,&footer,FALSE);
}
static void save_settings(HWND w) {
    Config c=app.config;double day,offset,width,font;
    if(!get_number(w,ID_WIDTH,136,320,&width)||floor(width)!=width){validation_error(w,ID_WIDTH,L"Width must be a whole number from 136 to 320.");return;}
    if(!get_number(w,ID_FONT,10,16,&font)||floor(font)!=font){validation_error(w,ID_FONT,L"Text size must be a whole number from 10 to 16.");return;}
    c.widget_width=(int)width;c.font_size=(int)font;
    c.theme=(int)SendDlgItemMessageW(w,ID_THEME,CB_GETCURSEL,0,0);
    c.transparent=(int)SendDlgItemMessageW(w,ID_BACKGROUND,CB_GETCURSEL,0,0);
    c.tray_only=(int)SendDlgItemMessageW(w,ID_MODE,CB_GETCURSEL,0,0);
    c.show_totals=(int)SendDlgItemMessageW(w,ID_TOTALS,CB_GETCURSEL,0,0);
    if(!get_clock(w,ID_START,&c.peak_start)){validation_error(w,ID_START,L"Peak start: use HH:MM between 00:00 and 23:59.");return;}
    if(!get_clock(w,ID_END,&c.peak_end)){validation_error(w,ID_END,L"Peak end: use HH:MM between 00:00 and 23:59.");return;}
    if(!get_clock(w,ID_TIME,&c.reset_minute)){validation_error(w,ID_TIME,L"Reset time: use HH:MM between 00:00 and 23:59.");return;}
    if(!get_number(w,ID_PEAK,0,1000000,&c.peak_gb)){validation_error(w,ID_PEAK,L"Peak allowance must be between 0 and 1,000,000 GB.");return;}
    if(!get_number(w,ID_OFF,0,1000000,&c.offpeak_gb)){validation_error(w,ID_OFF,L"Off-peak allowance must be between 0 and 1,000,000 GB.");return;}
    if(!get_number(w,ID_DAY,1,31,&day)||floor(day)!=day){validation_error(w,ID_DAY,L"Reset day must be a whole number from 1 to 31.");return;}
    if(!get_number(w,ID_OFFSET,-5000,5000,&offset)||floor(offset)!=offset){validation_error(w,ID_OFFSET,L"Offset must be a whole number from -5000 to 5000.");return;}
    c.reset_day=(int)day;c.offset=(int)offset;c.position=(int)SendDlgItemMessageW(w,ID_POSITION,CB_GETCURSEL,0,0);
    int a=(int)SendDlgItemMessageW(w,ID_ADAPTER,CB_GETCURSEL,0,0);c.adapter=a>0 && a<=(int)choice_count?choices[a-1].luid:0;
    app_sample();if(!app_flush()){app_error(w,L"Could not save pending usage. Settings have not been changed.");return;}
    if(!settings_save(&c)){app_error(w,L"Could not save settings. Check folder permissions and free disk space.");return;}
    int startup=(int)SendDlgItemMessageW(w,ID_STARTUP,CB_GETCURSEL,0,0);
    if(startup!=startup_enabled() && !startup_set(startup)){
        if(!settings_save(&app.config))app_error(w,L"Startup could not be updated, and restoring the previous settings failed. Reopen settings to review them.");
        else app_error(w,L"Windows startup could not be updated. Your previous settings have been restored.");return;
    }
    int changed=c.adapter!=app.config.adapter;app.config=c;if(changed)app.network.valid=0;
    app_refresh_usage();ui_theme();ui_attach();InvalidateRect(w,NULL,FALSE);
    SetWindowTextW(w,L"NetPulse - settings saved");wcscpy(app.status,L"Settings saved.");settings_dirty=0;
}
static void correct_usage(HWND w,int reset) {
    double peak=0,off=0;
    if(!reset && (!get_number(w,ID_CORRECT_PEAK,0,1000000,&peak)||!get_number(w,ID_CORRECT_OFF,0,1000000,&off))){app_error(w,L"Usage totals must be between 0 and 1,000,000 GB.");return;}
    if(MessageBoxW(w,reset?L"Set both current-cycle totals to zero? Historical traffic remains in the local database.":L"Replace current-cycle peak and off-peak totals with these values?",L"Adjust usage",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    app_sample();if(!app_flush() || !db_correct(&app.db,time(NULL),app.cycle,app.next_cycle,peak,off)){app_error(w,L"Could not save the usage adjustment.");return;}
    app_refresh_usage();set_number(w,ID_CORRECT_PEAK,peak);set_number(w,ID_CORRECT_OFF,off);InvalidateRect(w,NULL,FALSE);
}
static void export_usage(HWND w) {
    wchar_t path[MAX_PATH]=L"NetPulse-usage.csv";OPENFILENAMEW o={0};o.lStructSize=sizeof(o);o.hwndOwner=w;o.lpstrFilter=L"CSV files\0*.csv\0\0";
    o.lpstrFile=path;o.nMaxFile=MAX_PATH;o.lpstrDefExt=L"csv";o.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&o))return;
    app_sample();if(!app_flush() || !db_export(&app.db,path)){app_error(w,L"Could not export usage. Check disk space and destination permissions.");return;}
    SetWindowTextW(w,L"NetPulse - usage exported");
}
static void tracking_details(HWND w) {
    wchar_t text[1800],date[64];struct tm local=*localtime(&app.db.session);wcsftime(date,64,L"%d %b %Y, %H:%M",&local);
    swprintf(text,1800,L"Current session started: %ls\nSampling: every second\nMode: %ls\n\nMonitored interfaces:\n",date,app.config.tray_only?L"Tray only":L"Taskbar and tray");
    AdapterChoice available[MAX_ADAPTERS];unsigned count=network_choices(available,MAX_ADAPTERS);
    for(unsigned i=0;i<app.network.count;i++)for(unsigned j=0;j<count;j++)if(app.network.previous[i].luid==available[j].luid){
        size_t used=wcslen(text);if(used+ wcslen(available[j].name)+4<1400){wcscat(text,available[j].name);wcscat(text,L"\n");}break;
    }
    if(!app.network.count)wcscat(text,L"No matching interface currently available.\n");
    sqlite3_stmt *stmt=NULL;int reset=0;
    if(sqlite3_prepare_v2(app.db.handle,"SELECT 1 FROM daily_resets WHERE day=?",-1,&stmt,NULL)==SQLITE_OK){sqlite3_bind_int64(stmt,1,app.day_start);reset=sqlite3_step(stmt)==SQLITE_ROW;}sqlite3_finalize(stmt);
    wcscat(text,reset?L"\nToday's display was reset manually.":L"\nToday's display includes recorded traffic since midnight.");
    wcscat(text,L"\n\nAdapter traffic includes local transfers and protocol overhead. Your provider may count differently. Traffic while NetPulse is closed is not recorded.");
    MessageBoxW(w,text,L"Tracking details",MB_OK|MB_ICONINFORMATION);
}
static void manage_file(HWND w,int action) {
    if(settings_dirty){app_error(w,L"Save or discard your pending settings before backing up or restoring.");return;}
    int restoring=action==ID_RESTORE_SETTINGS,history=action==ID_BACKUP_DATA;
    wchar_t path[MAX_PATH];wcscpy(path,history?L"NetPulse-history.db":L"NetPulse-settings.ini");
    OPENFILENAMEW o={0};o.lStructSize=sizeof(o);o.hwndOwner=w;o.lpstrFile=path;o.nMaxFile=MAX_PATH;
    o.lpstrFilter=history?L"SQLite database\0*.db\0\0":L"NetPulse settings\0*.ini\0\0";o.lpstrDefExt=history?L"db":L"ini";
    o.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(restoring?OFN_FILEMUSTEXIST:OFN_OVERWRITEPROMPT);
    if(!(restoring?GetOpenFileNameW(&o):GetSaveFileNameW(&o)))return;
    wchar_t live[MAX_PATH];swprintf(live,MAX_PATH,L"%ls\\netpulse.db",app.directory);
    if(_wcsicmp(path,app.ini)==0 || _wcsicmp(path,live)==0){app_error(w,L"Choose a backup file outside the active settings/database files.");return;}
    if(restoring){
        Config imported;if(!settings_import(path,&imported)){app_error(w,L"This file is not a recognized NetPulse settings backup.");return;}
        if(MessageBoxW(w,L"Apply the plan and appearance from this backup? Missing or invalid values use safe defaults. Usage history and Windows startup are unchanged.",L"Restore settings",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
        app_sample();if(!app_flush() || !settings_save(&imported)){app_error(w,L"Restore could not be saved. Current settings remain active.");return;}
        app.config=imported;app.network.valid=0;app_refresh_usage();ui_theme();ui_attach();
        for(int i=0;i<control_count;i++)DestroyWindow(page_controls[i]);settings_fields(w);show_page(w,5);
    }else if(history){app_sample();if(!app_flush() || !db_backup(&app.db,path)){app_error(w,L"History backup failed. Check the destination and available space.");return;}}
    else if(!settings_export(path)){app_error(w,L"Could not export settings.");return;}
    wcscpy(app.status,restoring?L"Settings restored. Usage history is unchanged.":L"Backup saved successfully.");InvalidateRect(w,NULL,FALSE);
}
static HRESULT CALLBACK about_callback(HWND w,UINT notification,WPARAM wp,LPARAM lp,LONG_PTR data) {
    (void)wp;(void)data;
    if(notification==TDN_HYPERLINK_CLICKED && wcscmp((const wchar_t*)lp,L"https://github.com/avishka-Udara")==0)
        ShellExecuteW(w,L"open",(const wchar_t*)lp,NULL,NULL,SW_SHOWNORMAL);
    return S_OK;
}
LRESULT CALLBACK settings_proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    switch(m) {
    case WM_CREATE:settings_fields(w);show_page(w,0);SendMessageW(w,WM_CHANGEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);return 0;
    case WM_MEASUREITEM:{MEASUREITEMSTRUCT *i=(MEASUREITEMSTRUCT*)lp;if(i->CtlType==ODT_COMBOBOX){i->itemHeight=px(28);return TRUE;}break;}
    case WM_DRAWITEM:{DRAWITEMSTRUCT *i=(DRAWITEMSTRUCT*)lp;
        if(i->CtlType==ODT_COMBOBOX){
            int selected=(i->itemState&ODS_SELECTED)!=0;HBRUSH b=CreateSolidBrush(selected?(app.dark?RGB(45,45,51):RGB(239,244,255)):surface);FillRect(i->hDC,&i->rcItem,b);DeleteObject(b);
            SetBkMode(i->hDC,TRANSPARENT);SetTextColor(i->hDC,app.fg);
            HGDIOBJ old=SelectObject(i->hDC,app.font);wchar_t text[256]=L"";
            if(i->itemID!=(UINT)-1){LRESULT len=SendMessageW(i->hwndItem,CB_GETLBTEXTLEN,i->itemID,0);if(len>=0 && len<256)SendMessageW(i->hwndItem,CB_GETLBTEXT,i->itemID,(LPARAM)text);}
            RECT r=i->rcItem;r.left+=px(8);r.right-=px(4);DrawTextW(i->hDC,text,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
            if((i->itemState&ODS_FOCUS) && !(i->itemState&ODS_NOFOCUSRECT) && !(SendMessageW(w,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS))DrawFocusRect(i->hDC,&i->rcItem);SelectObject(i->hDC,old);
        }else draw_button(i);return TRUE;}

    case WM_ERASEBKGND:{RECT r;GetClientRect(w,&r);FillRect((HDC)wp,&r,app.background);return 1;}
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);FillRect(dc,&ps.rcPaint,app.background);summary(w,dc);EndPaint(w,&ps);return 0;}
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)wp,app.fg);SetBkColor((HDC)wp,m==WM_CTLCOLORSTATIC?app.bg:surface);return (LRESULT)(m==WM_CTLCOLORSTATIC?app.background:surface_brush);
    case WM_APP+10:{RECT r={0,px(page==0?204:604),px(680),px(page==0?536:650)};InvalidateRect(w,&r,FALSE);return 0;}
    case WM_COMMAND:
        if(!loading_fields && LOWORD(wp)!=ID_CORRECT_PEAK && LOWORD(wp)!=ID_CORRECT_OFF &&
           (HIWORD(wp)==EN_CHANGE || HIWORD(wp)==CBN_SELCHANGE || HIWORD(wp)==CBN_EDITCHANGE)){
            settings_dirty=1;app.status[0]=0;RECT footer={0,px(604),px(680),px(650)};InvalidateRect(w,&footer,FALSE);
        }
        if(LOWORD(wp)>=200 && LOWORD(wp)<=205){show_page(w,LOWORD(wp)-200);return 0;}
        switch(LOWORD(wp)){
        case ID_ABOUT:{
            TASKDIALOGCONFIG dialog={0};dialog.cbSize=sizeof(dialog);dialog.hwndParent=w;dialog.hInstance=app.instance;
            dialog.dwFlags=TDF_ENABLE_HYPERLINKS|TDF_ALLOW_DIALOG_CANCELLATION|TDF_SIZE_TO_CONTENT;
            dialog.dwCommonButtons=TDCBF_CLOSE_BUTTON;dialog.pszWindowTitle=L"About NetPulse";dialog.pszMainInstruction=L"NetPulse 1.2.0";
            dialog.pszContent=L"A lightweight, local network monitor.\n\nCreated by <a href=\"https://github.com/avishka-Udara\">Avishka Udara</a>\n\nCustomize your plan, appearance, and taskbar placement in Settings. Usage adjustments let you reset today's display or your billing cycle without deleting history.";
            dialog.pszFooter=L"GNU GPL version 3. Your usage data stays on this device.";dialog.pfCallback=about_callback;
            TaskDialogIndirect(&dialog,NULL,NULL,NULL);break;
        }
        case ID_TRACKING:tracking_details(w);break;
        case ID_BACKUP_SETTINGS:case ID_RESTORE_SETTINGS:case ID_BACKUP_DATA:manage_file(w,LOWORD(wp));break;
        case ID_DATA_FOLDER:ShellExecuteW(w,L"open",app.directory,NULL,NULL,SW_SHOWNORMAL);break;
        case ID_RETRY:app_sample();if(app_flush()){app_refresh_usage();wcscpy(app.status,L"Usage saved successfully.");}else app_error(w,L"Saving still failed. Check free space and folder permissions.");InvalidateRect(w,NULL,FALSE);break;
        case ID_SAVE:save_settings(w);break;case ID_EXPORT:export_usage(w);break;
        case ID_CORRECT:correct_usage(w,0);break;case ID_RESET:correct_usage(w,1);break;
        case ID_RESET_TODAY:
            if(MessageBoxW(w,L"Reset today's displayed download and upload to zero? Your recorded history and billing-cycle usage will be preserved.",L"Reset today's display",MB_YESNO|MB_ICONQUESTION)==IDYES){
                app_sample();if(!app_flush() || !db_reset_today(&app.db,app.day_start,app.day_end))app_error(w,L"Could not reset today's display.");
                else {app_refresh_usage();wcscpy(app.status,L"Today's display reset. New traffic continues counting.");InvalidateRect(w,NULL,FALSE);}
            }break;
        case ID_CLOSE:case IDCANCEL:SendMessageW(w,WM_CLOSE,0,0);break;
        case IDOK:save_settings(w);break;
        }return 0;
    case WM_CLOSE:if(settings_dirty && MessageBoxW(w,L"Discard unsaved settings changes?",L"Unsaved changes",MB_YESNO|MB_ICONQUESTION)!=IDYES)return 0;DestroyWindow(w);return 0;
    case WM_DESTROY:app.settings=NULL;return 0;
    }
    return DefWindowProcW(w,m,wp,lp);
}
void ui_settings(void) {
    if(IsWindow(app.settings)){ShowWindow(app.settings,SW_RESTORE);SetForegroundWindow(app.settings);return;}
    /* System-aware settings keep native controls consistent across Explorer's DPI modes. */
    DPI_AWARENESS_CONTEXT old=SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    HDC dc=GetDC(NULL);scale=GetDeviceCaps(dc,LOGPIXELSX);ReleaseDC(NULL,dc);
    if(!heading_font)heading_font=CreateFontW(-px(22),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    if(!value_font)value_font=CreateFontW(-px(30),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    RECT r={0,0,px(680),px(650)};AdjustWindowRectEx(&r,WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0);
    app.settings=CreateWindowExW(WS_EX_CONTROLPARENT,L"NetPulse.Settings",L"NetPulse - usage & settings",WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,NULL,NULL,app.instance,NULL);
    SetThreadDpiAwarenessContext(old);BOOL dark=app.dark;DwmSetWindowAttribute(app.settings,20,&dark,sizeof(dark));ShowWindow(app.settings,SW_SHOW);SetForegroundWindow(app.settings);
}
void ui_destroy(void) {
    NOTIFYICONDATAW n={0};n.cbSize=sizeof(n);n.hWnd=app.controller;n.uID=1;Shell_NotifyIconW(NIM_DELETE,&n);
    if(IsWindow(app.settings))DestroyWindow(app.settings);if(IsWindow(app.widget))DestroyWindow(app.widget);
    if(widget_font)DeleteObject(widget_font);if(heading_font)DeleteObject(heading_font);if(value_font)DeleteObject(value_font);if(surface_brush)DeleteObject(surface_brush);
    if(app.font)DeleteObject(app.font);if(app.small_font)DeleteObject(app.small_font);if(app.background)DeleteObject(app.background);
}
