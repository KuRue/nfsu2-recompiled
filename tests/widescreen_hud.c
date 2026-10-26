#include "../src/runtime/widescreen.c"
#include <assert.h>
uint32_t g_ebx,g_ecx,g_edx,g_esi,g_esp;
ptrdiff_t g_mem_base;
double g_st[8];
int main(void){
    void* mem=calloc(1,0x900000);assert(mem);g_mem_base=(ptrdiff_t)mem;
    nfs_widescreen_init("/nonexistent-nfsu2-check");assert(nfs_widescreen_hud_entries()==73);
    for(unsigned i=0;i<3;++i){
        unsigned w[]={1280,1920,2340},h[]={960,1080,1080};
        nfs_widescreen_set_resolution(w[i],h[i]);
        float x=-223,y=70;widescreen_hud(&x,&y);assert(x<-223&&y==70);
        float left=x;
        x=142;y=195;widescreen_hud(&x,&y);assert(x>142);
        assert(-left<240.f*w[i]/h[i]&&x<240.f*w[i]/h[i]);
    }
    nfs_widescreen_set_minimap_offset(-280);float x=-223,y=70;widescreen_hud(&x,&y);assert(y==-210);
    x=0;y=0;widescreen_hud(&x,&y);assert(x==0&&y==0);
    nfs_widescreen_init("/nonexistent-nfsu2-check");assert(nfs_widescreen_hud_entries()==73);
    free(mem);puts("HUD checks passed: built-in anchors, 4:3/16:9/phone aspect, minimap move, unrelated elements and reinit");
}
