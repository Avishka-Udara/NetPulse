#define COBJMACROS
#include <windows.h>
#include <uiautomation.h>
#include "taskbar.h"
#include "placement.h"
/* Accessibility calls stay off the sampling/UI thread. A stalled provider makes
   the snapshot expire, hiding the widget instead of guessing that space is free. */
static SRWLOCK lock=SRWLOCK_INIT;
static struct { HWND bar; RECT bounds; Occupied items[256]; int count,valid; ULONGLONG stamp; } snapshot;
static LONG layout_dirty=1;
static LONG started;
static LONG enabled=1;
static HANDLE wake_event;
void taskbar_enable(int enable){if(InterlockedExchange(&enabled,enable)!=enable){if(enable)InterlockedExchange(&layout_dirty,1);if(wake_event)SetEvent(wake_event);}}

static HWND event_bar;
static void CALLBACK layout_event(HWINEVENTHOOK hook,DWORD event,HWND window,LONG object,LONG child,DWORD thread,DWORD time) {
    (void)hook;(void)object;(void)child;(void)thread;(void)time;
    if(event!=EVENT_OBJECT_CREATE && event!=EVENT_OBJECT_DESTROY && event!=EVENT_OBJECT_SHOW && event!=EVENT_OBJECT_HIDE && event!=EVENT_OBJECT_LOCATIONCHANGE)return;
    HWND bar=event_bar;
    if(bar && window && (window==bar || GetAncestor(window,GA_ROOT)==bar))InterlockedExchange(&layout_dirty,1);
}
int taskbar_menu_active(HWND bar) {
    GUITHREADINFO info={0};info.cbSize=sizeof(info);
    DWORD barPid=0,foregroundPid=0;
    DWORD barThread=GetWindowThreadProcessId(bar,&barPid),foregroundThread=GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
    DWORD threads[]={barThread,GetCurrentThreadId(),foregroundPid==barPid?foregroundThread:barThread};
    for(int i=0;i<3;i++)if(GetGUIThreadInfo(threads[i],&info) &&
        (info.flags&(GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE)))return 1;
    return 0;
}
static DWORD WINAPI inspect_taskbar(void *unused) {
    (void)unused;SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if(FAILED(CoInitializeEx(NULL,COINIT_MULTITHREADED)))return 0;
    IUIAutomation *uia=NULL;
    if(FAILED(CoCreateInstance(&CLSID_CUIAutomation,NULL,CLSCTX_INPROC_SERVER,&IID_IUIAutomation,(void**)&uia))){CoUninitialize();return 0;}
    HWINEVENTHOOK hook=NULL;DWORD hook_pid=0;ULONGLONG last_scan=0;int previous_valid=0;
    for(;;){
        if(!InterlockedCompareExchange(&enabled,0,0)){
            if(hook){UnhookWinEvent(hook);hook=NULL;hook_pid=0;}
            AcquireSRWLockExclusive(&lock);snapshot.valid=0;ReleaseSRWLockExclusive(&lock);
            WaitForSingleObject(wake_event,INFINITE);last_scan=0;continue;
        }
        MSG message;while(PeekMessageW(&message,NULL,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        ULONGLONG observed=GetTickCount64();
        DWORD age=(DWORD)(observed-last_scan),interval=previous_valid?15000:3000;
        if(last_scan && age<interval && (!InterlockedCompareExchange(&layout_dirty,0,0) || age<1000)){
            MsgWaitForMultipleObjects(1,&wake_event,FALSE,age<1000?1000-age:interval-age,QS_ALLINPUT);continue;
        }
        InterlockedExchange(&layout_dirty,0);last_scan=observed;
        HWND bar=FindWindowW(L"Shell_TrayWnd",NULL);DWORD pid_now=0;if(bar)GetWindowThreadProcessId(bar,&pid_now);
        event_bar=bar;
        if(pid_now!=hook_pid){if(hook)UnhookWinEvent(hook);hook=NULL;hook_pid=pid_now;
            if(hook_pid)hook=SetWinEventHook(EVENT_OBJECT_CREATE,EVENT_OBJECT_LOCATIONCHANGE,NULL,layout_event,hook_pid,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);}
        RECT bounds={0};Occupied items[256];int count=0,valid=0,buttons=0;
        IUIAutomationElement *root=NULL;IUIAutomationCondition *condition=NULL;IUIAutomationElementArray *all=NULL;
        if(bar && GetWindowRect(bar,&bounds) && SUCCEEDED(IUIAutomation_ElementFromHandle(uia,(UIA_HWND)bar,&root)) &&
           SUCCEEDED(IUIAutomation_get_ControlViewCondition(uia,&condition)) && SUCCEEDED(IUIAutomationElement_FindAll(root,TreeScope_Descendants,condition,&all))){
            int length=0;valid=SUCCEEDED(IUIAutomationElementArray_get_Length(all,&length)) && length<512;
            for(int i=0;valid && i<length;i++){
                IUIAutomationElement *e=NULL;RECT r;int pid,type;BOOL off;
                if(FAILED(IUIAutomationElementArray_GetElement(all,i,&e))){valid=0;break;}
                if(FAILED(IUIAutomationElement_get_CurrentControlType(e,&type))){valid=0;IUIAutomationElement_Release(e);break;}
                if(type!=UIA_PaneControlTypeId && type!=UIA_GroupControlTypeId && type!=UIA_MenuControlTypeId && type!=UIA_MenuItemControlTypeId &&
                   type!=UIA_WindowControlTypeId && type!=UIA_ToolBarControlTypeId && type!=UIA_ListControlTypeId){
                    if(FAILED(IUIAutomationElement_get_CurrentIsOffscreen(e,&off)) || FAILED(IUIAutomationElement_get_CurrentProcessId(e,&pid))){valid=0;IUIAutomationElement_Release(e);break;}
                    if(off || pid==(int)GetCurrentProcessId()){IUIAutomationElement_Release(e);continue;}
                    if(FAILED(IUIAutomationElement_get_CurrentBoundingRectangle(e,&r)))valid=0;
                    else if(r.right>r.left && r.bottom>bounds.top && r.top<bounds.bottom && r.right>bounds.left && r.left<bounds.right){
                        if(count==256)valid=0;
                        else {items[count++]=(Occupied){r.left-bounds.left,r.right-bounds.left};if(type==UIA_ButtonControlTypeId)buttons++;}
                    }
                }
                IUIAutomationElement_Release(e);
            }
        }
        if(all)IUIAutomationElementArray_Release(all);if(condition)IUIAutomationCondition_Release(condition);if(root)IUIAutomationElement_Release(root);
        AcquireSRWLockExclusive(&lock);snapshot.bar=bar;snapshot.bounds=bounds;snapshot.count=count;snapshot.valid=valid && buttons>0;
        CopyMemory(snapshot.items,items,(size_t)count*sizeof(*items));snapshot.stamp=observed;previous_valid=snapshot.valid;ReleaseSRWLockExclusive(&lock);
    }
}
int taskbar_space(HWND bar,int left,int right,int width,int preferred,int margin,int *x) {
    if(InterlockedCompareExchange(&started,1,0)==0){wake_event=CreateEventW(NULL,FALSE,FALSE,NULL);if(!wake_event){InterlockedExchange(&started,0);return 0;}HANDLE thread=CreateThread(NULL,0,inspect_taskbar,NULL,0,NULL);if(thread)CloseHandle(thread);else {CloseHandle(wake_event);wake_event=NULL;InterlockedExchange(&started,0);}}
    RECT bounds;GetWindowRect(bar,&bounds);Occupied items[256];int count;
    AcquireSRWLockShared(&lock);
    if(!snapshot.valid || snapshot.bar!=bar || !EqualRect(&bounds,&snapshot.bounds) || GetTickCount64()-snapshot.stamp>16500){ReleaseSRWLockShared(&lock);return 0;}
    count=snapshot.count;CopyMemory(items,snapshot.items,(size_t)count*sizeof(*items));ReleaseSRWLockShared(&lock);
    for(int i=0;i<count;i++){items[i].left-=margin;items[i].right+=margin;}
    return placement_find(left,right,width,preferred,items,count,x);
}
