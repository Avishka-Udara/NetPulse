#define _CRT_SECURE_NO_WARNINGS
#include "app.h"
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
App app;
void app_error(HWND owner,const wchar_t *message){MessageBoxW(owner,message,L"NetPulse",MB_OK|MB_ICONWARNING);}
int app_flush(void) {
    app.storage_ok=db_flush(&app.db,GetTickCount64());app.last_flush=GetTickCount64();return app.storage_ok;
}
void app_refresh_usage(void) {
    time_t now=time(NULL);app.cycle=cycle_start(&app.config,now);app.next_cycle=cycle_next(&app.config,now);
    Usage u;if(db_totals(&app.db,app.cycle,app.next_cycle,&u))app.usage=u;else app.storage_ok=0;
    struct tm day=*localtime(&now);day.tm_hour=day.tm_min=day.tm_sec=0;day.tm_isdst=-1;
    app.day_start=mktime(&day);day.tm_mday++;day.tm_isdst=-1;app.day_end=mktime(&day);
    if(!db_today(&app.db,app.day_start,app.day_end,&app.today_down,&app.today_up))app.storage_ok=0;
}
void app_sample(void) {
    uint64_t down=0,up=0;ULONGLONG tick=GetTickCount64();time_t now=time(NULL);
    if(app.db.count>=PENDING_MAX-2 && !app_flush()) {
        app.network.valid=0;app.down_rate=app.up_rate=0;
        app.last_tick=tick;app.last_time=now;
        if(app.settings)PostMessageW(app.settings,WM_APP+10,0,0);
        if(app.widget && IsWindowVisible(app.widget))InvalidateRect(app.widget,NULL,FALSE);
        ui_tray(0);return;
    }
    time_t previous_cycle=app.cycle;
    app.network_ok=network_poll(&app.network,app.config.adapter,&down,&up);
    double elapsed=(double)(tick-app.last_tick)/1000.0;
    app.down_rate=elapsed>0?down/elapsed:0;app.up_rate=elapsed>0?up/elapsed:0;
    if(app.last_time && app.network_ok && (down || up)) {
        time_t start=app.last_time;
        /* Rebaseline after sleep or a clock jump rather than misattribute a long interval. */
        if(now<=start || now-start>10){start=now;app.down_rate=app.up_rate=0;}
        time_t cursor=start;uint64_t remaining_down=down,remaining_up=up;
        do {
            struct tm local=*localtime(&cursor);
            time_t end=cursor+60-local.tm_sec;if(end>now || start==now)end=now;
            uint64_t d=end==now?remaining_down:proportional_bytes(down,(uint64_t)(end-cursor),(uint64_t)(now-start));
            uint64_t u=end==now?remaining_up:proportional_bytes(up,(uint64_t)(end-cursor),(uint64_t)(now-start));
            Usage part;usage_split(&app.config,cursor,end,d,u,&part);
            if(!db_add(&app.db,cursor,&part)){app.storage_ok=0;break;}
            if(cursor>=app.cycle && cursor<app.next_cycle)
                for(int p=0;p<2;p++){app.usage.down[p]+=part.down[p];app.usage.up[p]+=part.up[p];}
            if(cursor>=app.day_start && cursor<app.day_end){app.today_down+=d;app.today_up+=u;}
            remaining_down-=d;remaining_up-=u;cursor=end;
        }while(cursor<now);
    }
    if(app.network_ok){app.last_tick=tick;app.last_time=now;}
    if(tick-app.last_flush>=60000)app_flush();
    if(now<previous_cycle || now>=app.next_cycle || now<app.day_start || now>=app.day_end)app_refresh_usage();
    ui_attach();
    if(app.widget && IsWindowVisible(app.widget))InvalidateRect(app.widget,NULL,FALSE);
    if(app.settings)PostMessageW(app.settings,WM_APP+10,0,0);
    ui_tray(0);
}
static LRESULT CALLBACK controller_proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    if(m==app.taskbar_created && app.taskbar_created){ui_tray(1);ui_attach();return 0;}
    switch(m) {
    case WM_TIMER:app_sample();return 0;
    case WM_SETTINGCHANGE:ui_theme();app_refresh_usage();return 0;
    case WM_TIMECHANGE:app_refresh_usage();return 0;
    case WM_THEMECHANGED:ui_theme();return 0;
    case WM_DISPLAYCHANGE:ui_attach();return 0;
    case WM_POWERBROADCAST:
        if(wp==PBT_APMSUSPEND){app_sample();app_flush();}
        if(wp==PBT_APMRESUMEAUTOMATIC || wp==PBT_APMRESUMESUSPEND){app.network.valid=0;app.last_time=time(NULL);app.last_tick=GetTickCount64();}return TRUE;
    case WM_TRAY:
        if(LOWORD(lp)==WM_LBUTTONUP || LOWORD(lp)==NIN_SELECT || LOWORD(lp)==NIN_KEYSELECT)ui_settings();
        if(LOWORD(lp)==WM_RBUTTONUP || LOWORD(lp)==WM_CONTEXTMENU)ui_menu(w);return 0;
    case WM_COMMAND:
        if(LOWORD(wp)==CMD_SETTINGS)ui_settings();
        if(LOWORD(wp)==CMD_REATTACH){if(app.widget)DestroyWindow(app.widget);app.widget=NULL;ui_attach();}
        if(LOWORD(wp)==CMD_EXIT)SendMessageW(w,WM_CLOSE,0,0);return 0;
    case WM_QUERYENDSESSION:app_sample();app_flush();return TRUE;
    case WM_ENDSESSION:if(wp){app_flush();app.quitting=1;DestroyWindow(w);}return 0;
    case WM_CLOSE:
        app_sample();if(!app_flush() && MessageBoxW(app.settings,L"Recent usage could not be saved. Exit and lose unsaved samples?",L"NetPulse",MB_YESNO|MB_ICONWARNING)!=IDYES)return 0;
        app.quitting=1;DestroyWindow(w);return 0;
    case WM_DESTROY:KillTimer(w,1);ui_destroy();PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(w,m,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command,int show) {
    (void)previous;(void)show;memset(&app,0,sizeof(app));app.instance=instance;app.storage_ok=1;
    HANDLE mutex=CreateMutexW(NULL,FALSE,L"Local\\NetPulse.Desktop.Singleton");
    if(!mutex)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS){HWND old=FindWindowW(L"NetPulse.Controller",L"NetPulse");if(old){if(wcscmp(command,L"--quit")==0)PostMessageW(old,WM_CLOSE,0,0);else PostMessageW(old,WM_COMMAND,CMD_SETTINGS,0);}CloseHandle(mutex);return 0;}
    if(wcscmp(command,L"--quit")==0){CloseHandle(mutex);return 0;}
    int argc=0,custom_directory=0;LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    for(int i=1;argv && i<argc;i++)if(wcscmp(argv[i],L"--data-dir")==0) {
        if(i+1>=argc){app_error(NULL,L"--data-dir requires a directory path.");LocalFree(argv);CloseHandle(mutex);return 1;}
        DWORD length=GetFullPathNameW(argv[++i],MAX_PATH,app.directory,NULL);
        if(!length || length>=MAX_PATH-40){app_error(NULL,L"The data directory path is too long or invalid.");LocalFree(argv);CloseHandle(mutex);return 1;}
        custom_directory=1;
    }
    if(argv)LocalFree(argv);
    if(!custom_directory && SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,SHGFP_TYPE_CURRENT,app.directory)!=S_OK){CloseHandle(mutex);return 1;}
    if(wcslen(app.directory)>MAX_PATH-40){CloseHandle(mutex);return 1;}
    if(!custom_directory)wcscat(app.directory,L"\\NetPulse");
    if(!CreateDirectoryW(app.directory,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS){app_error(NULL,L"Cannot create the local data folder.");CloseHandle(mutex);return 1;}
    swprintf(app.ini,MAX_PATH,L"%ls\\config.ini",app.directory);
    int first=GetFileAttributesW(app.ini)==INVALID_FILE_ATTRIBUTES;
    if(first){
        wchar_t source[MAX_PATH];DWORD length=GetModuleFileNameW(NULL,source,MAX_PATH);
        if(length>0 && length<MAX_PATH){wchar_t *last=wcsrchr(source,L'\\');
            if(last && (size_t)(last-source)+12<MAX_PATH){wcscpy(last+1,L"config.ini");CopyFileW(source,app.ini,TRUE);}}
    }
    settings_load();if(first && !settings_save(&app.config)){app_error(NULL,L"Cannot save settings.");CloseHandle(mutex);return 1;}
    wchar_t dbpath[MAX_PATH];swprintf(dbpath,MAX_PATH,L"%ls\\netpulse.db",app.directory);
    if(!db_open(&app.db,dbpath)){app_error(NULL,L"Cannot open the local usage database. Check disk space and folder permissions.");db_close(&app.db);CloseHandle(mutex);return 1;}
    app.icon=LoadIconW(instance,MAKEINTRESOURCEW(1));if(!app.icon)app.icon=LoadIconW(NULL,IDI_APPLICATION);
    WNDCLASSW cls={0};cls.hInstance=instance;cls.hCursor=LoadCursorW(NULL,IDC_ARROW);cls.hIcon=app.icon;
    cls.lpfnWndProc=controller_proc;cls.lpszClassName=L"NetPulse.Controller";RegisterClassW(&cls);
    cls.lpfnWndProc=widget_proc;cls.lpszClassName=L"NetPulse.Widget";cls.style=CS_DBLCLKS;RegisterClassW(&cls);
    cls.lpfnWndProc=settings_proc;cls.lpszClassName=L"NetPulse.Settings";cls.style=0;RegisterClassW(&cls);
    app.taskbar_created=RegisterWindowMessageW(L"TaskbarCreated");
    app.controller=CreateWindowExW(WS_EX_TOOLWINDOW,L"NetPulse.Controller",L"NetPulse",WS_POPUP,0,0,0,0,NULL,NULL,instance,NULL);
    if(!app.controller){db_close(&app.db);CloseHandle(mutex);return 1;}
    ui_theme();app_refresh_usage();ui_tray(1);ui_attach();
    app.last_tick=GetTickCount64();app.last_flush=app.last_tick;app.last_time=time(NULL);
    uint64_t d,u;network_poll(&app.network,app.config.adapter,&d,&u);SetTimer(app.controller,1,1000,NULL);
    if((first && !wcsstr(command,L"--background")) || wcsstr(command,L"--settings"))ui_settings();
    HKEY theme_key=NULL;HANDLE theme_event=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(!theme_event || RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",0,KEY_NOTIFY,&theme_key)!=ERROR_SUCCESS) {
        if(theme_event)CloseHandle(theme_event);theme_event=NULL;
    }
    if(theme_event && RegNotifyChangeKeyValue(theme_key,FALSE,REG_NOTIFY_CHANGE_LAST_SET,theme_event,TRUE)!=ERROR_SUCCESS){CloseHandle(theme_event);theme_event=NULL;}
    MSG msg;int running=1;
    while(running) {
        DWORD result=MsgWaitForMultipleObjects(theme_event?1:0,theme_event?&theme_event:NULL,FALSE,INFINITE,QS_ALLINPUT);
        if(theme_event && result==WAIT_OBJECT_0){ui_theme();RegNotifyChangeKeyValue(theme_key,FALSE,REG_NOTIFY_CHANGE_LAST_SET,theme_event,TRUE);}
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)) {
            if(msg.message==WM_QUIT){running=0;break;}
            if(!app.settings || !IsDialogMessageW(app.settings,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        }
    }
    if(theme_key)RegCloseKey(theme_key);if(theme_event)CloseHandle(theme_event);
    db_close(&app.db);CloseHandle(mutex);return 0;
}
