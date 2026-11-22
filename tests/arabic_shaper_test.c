#include "../arabic_shaper.h"
#include <assert.h>
#include <stdio.h>

static void test_arabic_forms(void) {
    uint32_t out[64]; int n;
    n=arabic_process_line("باب",out,64,1);
    assert(n==3);
    assert(out[0]==0xFE90); /* final beh */
    assert(out[1]==0xFE8D); /* alef */
    assert(out[2]==0xFE91); /* initial beh */
}
static void test_lam_alef_variants(void) {
    uint32_t out[64]; int n;
    n=arabic_process_line("لا",out,64,1);
    assert(n==1 && out[0]==0xFEFB);
    n=arabic_process_line("لأ",out,64,1);
    assert(n==1 && out[0]==0xFEF7);
    n=arabic_process_line("لإ",out,64,1);
    assert(n==1 && out[0]==0xFEF9);
    n=arabic_process_line("لآ",out,64,1);
    assert(n==1 && out[0]==0xFEF5);
    n=arabic_process_line("بلا",out,64,1);
    assert(n==2 && out[0]==0xFEFC && out[1]==0xFE91);
}
static void test_mixed_text_and_marks(void) {
    uint32_t out[64]; int n;
    n=arabic_process_line("ABC 123",out,64,1);
    assert(n==7);
    assert(out[0]=='A' && out[1]=='B' && out[2]=='C');
    assert(out[4]=='1' && out[5]=='2' && out[6]=='3');
    n=arabic_process_line("بَاب",out,64,1);
    assert(n==3);
}
int main(void) {
    test_arabic_forms();
    test_lam_alef_variants();
    test_mixed_text_and_marks();
    puts("Arabic shaping tests: PASS");
    return 0;
}
