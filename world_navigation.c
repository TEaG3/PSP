#include "world_navigation.h"
#include <math.h>

#define NAV_AGENT_RADIUS 0.42f
#define NAV_GRID_MARGIN 0.72f

typedef struct { float x,z,hx,hz; } WorldBlock;
static int get_blocks(StageId stage,float limit,WorldBlock b[12]) {
    int n=0,i;
    if(stage==STAGE_VILLAGE) {
        b[n++]=(WorldBlock){-9.0f,7.0f,2.2f,1.75f};
        b[n++]=(WorldBlock){0.0f,7.0f,2.2f,1.75f};
        b[n++]=(WorldBlock){9.0f,7.0f,2.2f,1.75f};
        for(i=-2;i<=2;i++) b[n++]=(WorldBlock){(float)i*6.0f,-9.0f,0.8f,0.8f};
        b[n++]=(WorldBlock){-5.0f,0.0f,0.9f,0.9f};
        b[n++]=(WorldBlock){5.0f,-2.0f,0.5f,0.45f};
        b[n++]=(WorldBlock){6.1f,-2.0f,0.45f,0.45f};
    } else if(stage==STAGE_CASTLE) {
        b[n++]=(WorldBlock){-9.0f,0.0f,1.4f,1.4f};
        b[n++]=(WorldBlock){9.0f,0.0f,1.4f,1.4f};
        for(i=-2;i<=2;i++) b[n++]=(WorldBlock){(float)i*8.0f,-limit+1.0f,1.1f,1.1f};
        b[n++]=(WorldBlock){-13.0f,7.0f,0.9f,0.9f};
        b[n++]=(WorldBlock){13.0f,7.0f,0.9f,0.9f};
    } else {
        b[n++]=(WorldBlock){-12.0f,0.0f,1.75f,1.75f};
        b[n++]=(WorldBlock){12.0f,0.0f,1.75f,1.75f};
        for(i=-2;i<=2;i++) {
            b[n++]=(WorldBlock){(float)i*7.0f,-8.0f,1.4f,1.4f};
        }
        b[n++]=(WorldBlock){-6.0f,2.0f,2.1f,1.25f};
        b[n++]=(WorldBlock){6.0f,2.0f,2.1f,1.25f};
        b[n++]=(WorldBlock){-2.0f,-2.0f,0.9f,0.9f};
        b[n++]=(WorldBlock){2.0f,-2.0f,0.9f,0.9f};
    }
    return n;
}
static int circle_hits_block(float x,float z,float radius,const WorldBlock *b) {
    float nx=x<b->x-b->hx?b->x-b->hx:(x>b->x+b->hx?b->x+b->hx:x);
    float nz=z<b->z-b->hz?b->z-b->hz:(z>b->z+b->hz?b->z+b->hz:z);
    float dx=x-nx,dz=z-nz;
    return dx*dx+dz*dz<radius*radius;
}
int worldNavBlocked(StageId stage,float limit,float x,float z,float radius) {
    WorldBlock blocks[12]; int n=get_blocks(stage,limit,blocks),i;
    if(x < -limit+0.65f+radius || x > limit-0.65f-radius ||
       z < -limit+0.65f+radius || z > limit-0.65f-radius) return 1;
    for(i=0;i<n;i++) if(circle_hits_block(x,z,radius,&blocks[i])) return 1;
    return 0;
}
void worldNavBuild(StageId stage,float limit,NavGrid *grid) {
    int x,y; float cell,wx,wz;
    if(!grid) return;
    navClear(grid,NAV_MAX_WIDTH,NAV_MAX_HEIGHT);
    cell=(2.0f*limit)/(float)NAV_MAX_WIDTH;
    for(y=0;y<NAV_MAX_HEIGHT;y++) for(x=0;x<NAV_MAX_WIDTH;x++) {
        wx=-limit+((float)x+0.5f)*cell;
        wz=-limit+((float)y+0.5f)*cell;
        if(worldNavBlocked(stage,limit,wx,wz,NAV_AGENT_RADIUS+cell*NAV_GRID_MARGIN))
            (void)navSetBlocked(grid,x,y,1);
    }
}
static int world_to_cell(float limit,float x,float z,NavPoint *p) {
    float cell=(2.0f*limit)/(float)NAV_MAX_WIDTH;
    int cx=(int)floorf((x+limit)/cell),cy=(int)floorf((z+limit)/cell);
    if(!p || cx<0 || cy<0 || cx>=NAV_MAX_WIDTH || cy>=NAV_MAX_HEIGHT) return 0;
    p->x=cx;p->y=cy;return 1;
}
void worldNavCellCenter(float limit,int cell_x,int cell_y,float *x,float *z) {
    float cell=(2.0f*limit)/(float)NAV_MAX_WIDTH;
    if(x)*x=-limit+((float)cell_x+0.5f)*cell;
    if(z)*z=-limit+((float)cell_y+0.5f)*cell;
}
static int nearest_open(const NavGrid *g,NavPoint *p) {
    int radius,dx,dy;
    if(!g||!p)return 0;
    if(p->x>=0&&p->y>=0&&p->x<g->width&&p->y<g->height&&
       !g->blocked[p->y*g->width+p->x])return 1;
    for(radius=1;radius<6;radius++) for(dy=-radius;dy<=radius;dy++)
        for(dx=-radius;dx<=radius;dx++) {
            int x=p->x+dx,y=p->y+dy;
            if(x<0||y<0||x>=g->width||y>=g->height)continue;
            if(g->blocked[y*g->width+x])continue;
            p->x=x;p->y=y;return 1;
        }
    return 0;
}
int worldNavFindPath(const NavGrid *grid,float limit,float sx,float sz,
                     float gx,float gz,NavPath *path) {
    NavPoint start,goal;
    if(!path||!world_to_cell(limit,sx,sz,&start)||
       !world_to_cell(limit,gx,gz,&goal))return 0;
    if(!nearest_open(grid,&start)||!nearest_open(grid,&goal))return 0;
    return navFindPath(grid,start,goal,path);
}
