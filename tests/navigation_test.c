#include "../navigation.h"
#include <assert.h>
#include <stdio.h>

static void test_open_grid(void) {
    NavGrid g; NavPath p;
    navClear(&g, 12, 12);
    assert(navFindPath(&g,(NavPoint){1,1},(NavPoint){9,9},&p));
    assert(p.count >= 2 && p.count <= 9);
    assert(p.points[0].x==1 && p.points[0].y==1);
    assert(p.points[p.count-1].x==9 && p.points[p.count-1].y==9);
}
static void test_wall_detour(void) {
    NavGrid g; NavPath p; int y;
    navClear(&g,10,10);
    for(y=0;y<9;y++) assert(navSetBlocked(&g,5,y,1));
    assert(navFindPath(&g,(NavPoint){2,2},(NavPoint){8,2},&p));
    assert(p.count>7);
    for(y=0;y<p.count;y++) assert(!(p.points[y].x==5 && p.points[y].y<9));
}
static void test_unreachable_and_invalid(void) {
    NavGrid g; NavPath p; int x;
    navClear(&g,5,5);
    for(x=0;x<5;x++) navSetBlocked(&g,x,2,1);
    assert(!navFindPath(&g,(NavPoint){1,1},(NavPoint){1,3},&p));
    assert(p.count==0);
    assert(!navFindPath(&g,(NavPoint){-1,0},(NavPoint){1,1},&p));
    navClear(&g,100,5);
    assert(g.width==0 && g.height==0);
}
static void test_no_corner_cut(void) {
    NavGrid g; NavPath p;
    navClear(&g,3,3);
    navSetBlocked(&g,1,0,1); navSetBlocked(&g,0,1,1);
    assert(!navFindPath(&g,(NavPoint){0,0},(NavPoint){1,1},&p));
}
int main(void) {
    test_open_grid(); test_wall_detour();
    test_unreachable_and_invalid(); test_no_corner_cut();
    puts("navigation tests: PASS");
    return 0;
}
