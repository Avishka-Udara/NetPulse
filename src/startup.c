#define _CRT_SECURE_NO_WARNINGS
#include "app.h"
#include <wchar.h>
static const wchar_t *run_key=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static int startup_command(wchar_t *command,size_t count) {
    wchar_t exe[MAX_PATH];DWORD n=GetModuleFileNameW(NULL,exe,MAX_PATH);
    if(!n || n>=MAX_PATH)return 0;
    int length=swprintf(command,count,L"\"%ls\" --data-dir \"%ls\" --background",exe,app.directory);
    return length>0 && (size_t)length<count;
}
int startup_enabled(void) {
    wchar_t saved[1024],expected[1024];DWORD size=sizeof(saved);
    if(!startup_command(expected,1024))return 0;
    if(RegGetValueW(HKEY_CURRENT_USER,run_key,L"NetPulse",RRF_RT_REG_SZ,NULL,saved,&size)!=ERROR_SUCCESS)return 0;
    return wcscmp(saved,expected)==0;
}
int startup_set(int enable) {
    HKEY key;wchar_t command[1024];
    if(!startup_command(command,1024))return 0;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,run_key,0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL)!=ERROR_SUCCESS)return 0;
    LSTATUS status=enable?RegSetValueExW(key,L"NetPulse",0,REG_SZ,(const BYTE*)command,(DWORD)((wcslen(command)+1)*sizeof(wchar_t))):RegDeleteValueW(key,L"NetPulse");
    RegCloseKey(key);return status==ERROR_SUCCESS || (!enable && status==ERROR_FILE_NOT_FOUND);
}
