#ifndef PSP_ARABIC_SHAPER_H
#define PSP_ARABIC_SHAPER_H

/* Fixed-buffer UTF-8 Arabic shaping and basic mixed-direction layout.
 * C99, no heap allocation, suitable for PSPSDK. Output is visual-order
 * Presentation Forms-B codepoints for a bitmap glyph renderer.
 */
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t base, isolated, final_form, initial_form, medial_form;
} PspArabicForm;

static const PspArabicForm psp_arabic_forms[] = {
    {0x0621,0xFE80,0,0,0},{0x0622,0xFE81,0xFE82,0,0},
    {0x0623,0xFE83,0xFE84,0,0},{0x0624,0xFE85,0xFE86,0,0},
    {0x0625,0xFE87,0xFE88,0,0},{0x0626,0xFE89,0xFE8A,0xFE8B,0xFE8C},
    {0x0627,0xFE8D,0xFE8E,0,0},{0x0628,0xFE8F,0xFE90,0xFE91,0xFE92},
    {0x0629,0xFE93,0xFE94,0,0},{0x062A,0xFE95,0xFE96,0xFE97,0xFE98},
    {0x062B,0xFE99,0xFE9A,0xFE9B,0xFE9C},{0x062C,0xFE9D,0xFE9E,0xFE9F,0xFEA0},
    {0x062D,0xFEA1,0xFEA2,0xFEA3,0xFEA4},{0x062E,0xFEA5,0xFEA6,0xFEA7,0xFEA8},
    {0x062F,0xFEA9,0xFEAA,0,0},{0x0630,0xFEAB,0xFEAC,0,0},
    {0x0631,0xFEAD,0xFEAE,0,0},{0x0632,0xFEAF,0xFEB0,0,0},
    {0x0633,0xFEB1,0xFEB2,0xFEB3,0xFEB4},{0x0634,0xFEB5,0xFEB6,0xFEB7,0xFEB8},
    {0x0635,0xFEB9,0xFEBA,0xFEBB,0xFEBC},{0x0636,0xFEBD,0xFEBE,0xFEBF,0xFEC0},
    {0x0637,0xFEC1,0xFEC2,0xFEC3,0xFEC4},{0x0638,0xFEC5,0xFEC6,0xFEC7,0xFEC8},
    {0x0639,0xFEC9,0xFECA,0xFECB,0xFECC},{0x063A,0xFECD,0xFECE,0xFECF,0xFED0},
    {0x0640,0x0640,0x0640,0x0640,0x0640},{0x0641,0xFED1,0xFED2,0xFED3,0xFED4},
    {0x0642,0xFED5,0xFED6,0xFED7,0xFED8},{0x0643,0xFED9,0xFEDA,0xFEDB,0xFEDC},
    {0x0644,0xFEDD,0xFEDE,0xFEDF,0xFEE0},{0x0645,0xFEE1,0xFEE2,0xFEE3,0xFEE4},
    {0x0646,0xFEE5,0xFEE6,0xFEE7,0xFEE8},{0x0647,0xFEE9,0xFEEA,0xFEEB,0xFEEC},
    {0x0648,0xFEED,0xFEEE,0,0},{0x0649,0xFEEF,0xFEF0,0,0},
    {0x064A,0xFEF1,0xFEF2,0xFEF3,0xFEF4}
};
#define PSP_ARABIC_FORM_COUNT ((int)(sizeof(psp_arabic_forms)/sizeof(psp_arabic_forms[0])))

static int psp_arabic_is_mark(uint32_t c) {
    return (c>=0x064B && c<=0x065F) || c==0x0670 ||
           (c>=0x06D6 && c<=0x06ED) || (c>=0xFE70 && c<=0xFE7F);
}
static const PspArabicForm *psp_arabic_find(uint32_t c) {
    int lo=0, hi=PSP_ARABIC_FORM_COUNT-1;
    while(lo<=hi) {
        int m=lo+(hi-lo)/2;
        if(psp_arabic_forms[m].base==c) return &psp_arabic_forms[m];
        if(psp_arabic_forms[m].base<c) lo=m+1; else hi=m-1;
    }
    return NULL;
}
static int psp_utf8_next(const unsigned char **p, uint32_t *out) {
    const unsigned char *s=*p; uint32_t c;
    if(!s || !*s) return 0;
    if(s[0]<0x80){c=s[0];*p=s+1;}
    else if(s[0]>=0xC2 && s[0]<=0xDF && (s[1]&0xC0)==0x80){
        c=((uint32_t)(s[0]&31)<<6)|(s[1]&63);*p=s+2;
    } else if(s[0]>=0xE0 && s[0]<=0xEF && s[1] && s[2] &&
              (s[1]&0xC0)==0x80 && (s[2]&0xC0)==0x80 &&
              !(s[0]==0xE0 && s[1]<0xA0) &&
              !(s[0]==0xED && s[1]>=0xA0)){
        c=((uint32_t)(s[0]&15)<<12)|((uint32_t)(s[1]&63)<<6)|(s[2]&63);*p=s+3;
    } else if(s[0]>=0xF0 && s[0]<=0xF4 && s[1] && s[2] && s[3] &&
              (s[1]&0xC0)==0x80 && (s[2]&0xC0)==0x80 && (s[3]&0xC0)==0x80 &&
              !(s[0]==0xF0 && s[1]<0x90) && !(s[0]==0xF4 && s[1]>=0x90)){
        c=((uint32_t)(s[0]&7)<<18)|((uint32_t)(s[1]&63)<<12)|
          ((uint32_t)(s[2]&63)<<6)|(s[3]&63);*p=s+4;
    } else { c=0xFFFD;*p=s+1; }
    *out=c; return 1;
}
static uint32_t psp_lam_alef(uint32_t alef, int joined) {
    switch(alef){
        case 0x0622:return joined?0xFEF6:0xFEF5;
        case 0x0623:return joined?0xFEF8:0xFEF7;
        case 0x0625:return joined?0xFEFA:0xFEF9;
        case 0x0627:return joined?0xFEFC:0xFEFB;
        default:return 0;
    }
}
static int psp_arabic_dir(uint32_t c) {
    if((c>='0'&&c<='9')||(c>=0x0660&&c<=0x0669)||
       (c>='A'&&c<='Z')||(c>='a'&&c<='z')) return 0;
    if(psp_arabic_find(c)||(c>=0x0600&&c<=0x06FF)||
       (c>=0xFE70&&c<=0xFEFF)) return 1;
    return -1;
}
static uint32_t psp_mirror(uint32_t c) {
    switch(c){case '(':return ')';case ')':return '(';
    case '[':return ']';case ']':return '[';
    case '{':return '}';case '}':return '{';default:return c;}
}

/* Returns number of visual-order codepoints, or 0 for invalid arguments.
 * max_out is the hard output capacity. Marks stay attached to their base.
 */
static int arabic_process_line(const char *utf8, uint32_t *out,
                               int max_out, int strip_marks) {
    uint32_t in[512], shaped[512], visual[512];
    int start[512], end[512], dir[512], n=0, i, runs=0, used=0;
    const unsigned char *p=(const unsigned char *)utf8;
    if(!utf8||!out||max_out<=0) return 0;
    while(*p && n<512){uint32_t c;if(!psp_utf8_next(&p,&c))break;
        if(c==0xFEFF)continue;
        if(strip_marks && psp_arabic_is_mark(c))continue;
        in[n++]=c;
    }
    if(!n)return 0;
    for(i=0;i<n;i++) shaped[i]=in[i];
    for(i=0;i<n;i++){
        const PspArabicForm *cur=psp_arabic_find(in[i]), *pr=NULL,*nx=NULL;
        int pi=i-1,ni=i+1,jp=0,jn=0;
        uint32_t lig;
        if(!cur)continue;
        while(pi>=0 && psp_arabic_is_mark(in[pi]))pi--;
        while(ni<n && psp_arabic_is_mark(in[ni]))ni++;
        if(pi>=0)pr=psp_arabic_find(in[pi]);
        if(ni<n)nx=psp_arabic_find(in[ni]);
        jp=pr && pr->initial_form && cur->final_form;
        lig=(in[i]==0x0644 && ni<n)?psp_lam_alef(in[ni],jp):0;
        if(lig){shaped[i]=lig;shaped[ni]=0;continue;}
        jn=nx && cur->initial_form && nx->final_form;
        if(jp&&jn&&cur->medial_form)shaped[i]=cur->medial_form;
        else if(jp&&cur->final_form)shaped[i]=cur->final_form;
        else if(jn&&cur->initial_form)shaped[i]=cur->initial_form;
        else shaped[i]=cur->isolated;
    }
    /* Build directional runs; neutrals inherit the preceding run (RTL at start). */
    for(i=0;i<n;i++){
        int d=psp_arabic_dir(in[i]);
        if(d<0)d=(i?dir[i-1]:1);
        dir[i]=d;
    }
    for(i=0;i<n;){
        int j=i+1;while(j<n&&dir[j]==dir[i])j++;
        start[runs]=i;end[runs]=j;runs++;i=j;
    }
    while(runs>0){
        int a=start[runs-1],b=end[runs-1],j;
        if(dir[a]==1){
            /* Reverse clusters, not individual combining marks. */
            int cs[512],ce[512],cn=0,k=a;
            while(k<b){int q=k+1;while(q<b&&psp_arabic_is_mark(in[q]))q++;
                cs[cn]=k;ce[cn]=q;cn++;k=q;}
            for(j=cn-1;j>=0;j--){int q;for(q=cs[j];q<ce[j];q++)
                if(shaped[q] && used<max_out)visual[used++]=shaped[q];}
        } else {
            for(j=a;j<b;j++)if(shaped[j]&&used<max_out)visual[used++]=psp_mirror(shaped[j]);
        }
        runs--;
    }
    if(used>max_out)used=max_out;
    for(i=0;i<used;i++)out[i]=visual[i];
    return used;
}
#ifdef __cplusplus
}
#endif
#endif
