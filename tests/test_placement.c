#include <assert.h>
#include <stdio.h>
#include "placement.h"
int main(void) {
    int x=-1;Occupied a[]={{650,1200},{1300,1600},{0,60}};
    assert(placement_find(6,1600,170,1430,a,3,&x) && x==480);
    assert(placement_find(6,1600,170,6,a,3,&x) && x==60);
    Occupied full[]={{0,800},{700,1600}};
    assert(!placement_find(6,1600,170,6,full,2,&x));
    Occupied exact[]={{176,500}};
    assert(placement_find(6,500,170,330,exact,1,&x) && x==6);
    Occupied overflow[]={{-10,20},{10,70},{40,90},{300,900}};
    assert(placement_find(0,500,100,280,overflow,4,&x) && x==200);
    assert(placement_find(6,1600,170,715,NULL,0,&x) && x==715);
    assert(!placement_find(6,100,170,6,NULL,0,&x));
    puts("PASS: left/right/center preferences, crowded fallback, full taskbar, exact fit, merged obstacles");
    return 0;
}
