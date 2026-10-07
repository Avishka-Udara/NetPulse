#ifndef NETPULSE_TASKBAR_H
#define NETPULSE_TASKBAR_H
#include <windows.h>
void taskbar_enable(int enable);
int taskbar_menu_active(HWND bar);
int taskbar_space(HWND bar,int left,int right,int width,int preferred,int margin,int *x);
#endif
