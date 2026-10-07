#include "app.h"
#include <assert.h>
#include <stdio.h>
App app;
int main(void) {
    assert(GetTempFileNameW(L"build",L"nps",0,app.ini));
    assert(settings_load());assert(app.config.widget_width==136 && app.config.font_size==11 && app.config.transparent==1);
    Config c=app.config;c.widget_width=220;c.font_size=15;c.theme=2;c.transparent=0;c.show_totals=0;c.offset=-250;c.position=0;c.peak_start=317;
    assert(settings_save(&c));assert(settings_load());
    assert(app.config.widget_width==220 && app.config.font_size==15 && app.config.theme==2 && app.config.transparent==0 && app.config.show_totals==0);
    assert(app.config.offset==-250 && app.config.position==0 && app.config.peak_start==317);
    assert(WritePrivateProfileStringW(L"widget",L"width",L"999",app.ini));
    assert(WritePrivateProfileStringW(L"widget",L"font_size",L"-1",app.ini));
    assert(settings_load());assert(app.config.widget_width==136 && app.config.font_size==11);
    assert(DeleteFileW(app.ini));puts("PASS: appearance settings round-trip, typed time, offset and invalid-value fallback");return 0;
}
