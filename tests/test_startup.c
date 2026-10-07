#include "app.h"
#include <assert.h>
#include <stdio.h>
App app;
int main(void) {
    /* Redirect HKCU inside this test process: never touch the real Run key. */
    wchar_t path[128];swprintf(path,128,L"Software\\NetPulseStartupTest-%lu",GetCurrentProcessId());
    HKEY isolated;assert(RegCreateKeyExW(HKEY_CURRENT_USER,path,0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&isolated,NULL)==ERROR_SUCCESS);
    assert(RegOverridePredefKey(HKEY_CURRENT_USER,isolated)==ERROR_SUCCESS);
    wcscpy(app.directory,L"C:\\Test profile with spaces");
    assert(!startup_enabled());assert(startup_set(1));assert(startup_enabled());
    assert(startup_set(1));assert(startup_enabled());assert(startup_set(0));assert(!startup_enabled());assert(startup_set(0));
    assert(RegOverridePredefKey(HKEY_CURRENT_USER,NULL)==ERROR_SUCCESS);RegCloseKey(isolated);
    assert(RegDeleteTreeW(HKEY_CURRENT_USER,path)==ERROR_SUCCESS);
    puts("PASS: isolated startup enable/disable/idempotency with quoted executable and profile paths");return 0;
}


