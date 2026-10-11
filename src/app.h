#ifndef NETPULSE_APP_H
#define NETPULSE_APP_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "core.h"
#include "network.h"
#include "db.h"
#include "collector.h"
typedef struct {
    HINSTANCE instance; HWND controller,widget,settings,taskbar;
    Config config; Network network; Database db; Usage usage;
    wchar_t directory[MAX_PATH],ini[MAX_PATH],status[256];
    time_t last_time,cycle,next_cycle,day_start,day_end; ULONGLONG last_tick,last_flush;
    uint64_t today_down,today_up;
    time_t tracking_since,tracking_heartbeat;uint64_t tracking_lost;wchar_t tracking_status[160];
    uint64_t budget_used[2];double recent_rate[2],observed[2];Forecast forecast[2];
    time_t forecast_end,recent_cut;int active_band,insights_ok;ULONGLONG insight_tick;
    double down_rate,up_rate; int dark,quitting,storage_ok,network_ok,attached;
    UINT taskbar_created; HFONT font,small_font; HBRUSH background;
    COLORREF bg,fg,muted,accent; HICON icon;
} App;
extern App app;
int startup_enabled(void);
int startup_set(int enable);
int settings_export(const wchar_t *path);
int settings_import(const wchar_t *path,Config *out);
int settings_load(void);
int settings_save(const Config *c);
void app_sample(void);
void app_stop_tracking(void);
int app_flush(void);
void app_refresh_usage(void);
void app_insights(int force);
void app_error(HWND owner,const wchar_t *message);
void ui_theme(void);
void ui_attach(void);
void ui_settings(void);
void ui_tray(int add);
void ui_menu(HWND owner);
void ui_destroy(void);
LRESULT CALLBACK widget_proc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK settings_proc(HWND,UINT,WPARAM,LPARAM);
#define WM_TRAY (WM_APP+1)
#define CMD_SETTINGS 4001
#define CMD_EXIT 4002
#define CMD_REATTACH 4003
#endif
