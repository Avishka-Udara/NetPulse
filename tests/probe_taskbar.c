#include <windows.h>
#include <stdio.h>
#include "taskbar.h"
int main(void) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HWND bar=FindWindowW(L"Shell_TrayWnd",NULL);RECT r;GetClientRect(bar,&r);
    int x=-1,dpi=GetDpiForWindow(bar),width=MulDiv(136,dpi,96),right=r.right-MulDiv(6,dpi,96);
    HWND tray=FindWindowExW(bar,NULL,L"TrayNotifyWnd",NULL);RECT tr;
    if(tray && GetWindowRect(tray,&tr)){MapWindowPoints(NULL,bar,(POINT*)&tr,2);right=tr.left-MulDiv(6,dpi,96);}
    for(int i=0;i<12;i++) {int ok=taskbar_space(bar,6,right,width,right-width,8,&x);printf("taskbar=%ldx%ld dpi=%d width=%d right=%d fit=%d x=%d\n",r.right,r.bottom,dpi,width,right,ok,x);Sleep(500);}
    return 0;
}
