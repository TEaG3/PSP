#include "enemy_navigation.h"
#include "world_navigation.h"
#include <math.h>
#include <string.h>

#define ENEMY_REPATH_FRAMES 18
#define ENEMY_STOP_DISTANCE 1.35f
#define ENEMY_SEPARATION_RADIUS 1.35f
#define ENEMY_SEPARATION_WEIGHT 0.42f

static EnemyRoute routes[GAME_ENEMY_MAX];
static NavPath scratch_path;

static float clampf(float v,float lo,float hi) {
    return v<lo?lo:(v>hi?hi:v);
}
static float length2(float x,float z) { return x*x+z*z; }

void enemyNavReset(void) {
    memset(routes,0,sizeof(routes));
}
const EnemyRoute *enemyNavGetRoute(int index) {
    if(index<0||index>=GAME_ENEMY_MAX)return 0;
    return &routes[index];
}
static int build_route(EnemyRoute *r,const NavGrid *grid,float limit,
                       float sx,float sz,float gx,float gz) {
    int i;
    if(!worldNavFindPath(grid,limit,sx,sz,gx,gz,&scratch_path)) {
        r->count=0;r->cursor=0;return 0;
    }
    r->count=(unsigned short)scratch_path.count;
    r->cursor=0;
    for(i=0;i<scratch_path.count;i++) {
        r->x[i]=(unsigned char)scratch_path.points[i].x;
        r->y[i]=(unsigned char)scratch_path.points[i].y;
    }
    r->last_goal_x=gx;r->last_goal_z=gz;
    r->repath_frames=ENEMY_REPATH_FRAMES;
    return 1;
}
static void advance_cursor(EnemyRoute *r,float x,float z,float limit) {
    while(r->cursor<r->count) {
        float wx,wz,dx,dz;
        worldNavCellCenter(limit,r->x[r->cursor],r->y[r->cursor],&wx,&wz);
        dx=wx-x;dz=wz-z;
        if(length2(dx,dz)>0.22f)break;
        r->cursor++;
    }
}
void enemyNavUpdate(const PlayerState *target,EnemyStateData enemies[GAME_ENEMY_MAX],
                    const NavGrid *grid,StageId stage,float limit,float dt) {
    int i,j;
    if(!target||!enemies||!grid||grid->width<=0||grid->height<=0||
       !isfinite(dt)||dt<=0.0f)return;
    if(dt>0.08f)dt=0.08f;
    for(i=0;i<GAME_ENEMY_MAX;i++) {
        EnemyStateData *e=&enemies[i];
        EnemyRoute *r=&routes[i];
        float dx=target->x-e->x,dz=target->z-e->z;
        float distance=sqrtf(length2(dx,dz));
        float separation_x=0.0f,separation_z=0.0f;
        float dir_x,dir_z,dir_len,step,nx,nz,way_x,way_z;
        if(!e->active||e->state==ENEMY_DEAD) {
            r->count=0;r->cursor=0;continue;
        }
        if(distance>10.0f) {
            e->state=ENEMY_IDLE;r->count=0;r->cursor=0;continue;
        }
        e->state=ENEMY_CHASE;
        if(distance<=ENEMY_STOP_DISTANCE) {
            r->count=0;r->cursor=0;continue;
        }
        if(r->repath_frames>0)r->repath_frames--;
        if(r->count==0||r->repath_frames==0||
           length2(target->x-r->last_goal_x,target->z-r->last_goal_z)>2.25f) {
            if(!build_route(r,grid,limit,e->x,e->z,target->x,target->z))continue;
        }
        advance_cursor(r,e->x,e->z,limit);
        if(r->cursor>=r->count) {
            r->repath_frames=0;continue;
        }
        worldNavCellCenter(limit,r->x[r->cursor],r->y[r->cursor],&way_x,&way_z);
        dir_x=way_x-e->x;dir_z=way_z-e->z;
        dir_len=sqrtf(length2(dir_x,dir_z));
        if(dir_len<0.001f)continue;
        dir_x/=dir_len;dir_z/=dir_len;

        /* Separation steering prevents several agents occupying one route cell. */
        for(j=0;j<GAME_ENEMY_MAX;j++)if(j!=i&&enemies[j].active&&
            enemies[j].state!=ENEMY_DEAD) {
            float sx=e->x-enemies[j].x,sz=e->z-enemies[j].z;
            float d2=length2(sx,sz);
            if(d2>0.01f&&d2<ENEMY_SEPARATION_RADIUS*ENEMY_SEPARATION_RADIUS) {
                float inv=1.0f/d2;
                separation_x+=sx*inv;
                separation_z+=sz*inv;
            }
        }
        dir_x+=separation_x*ENEMY_SEPARATION_WEIGHT;
        dir_z+=separation_z*ENEMY_SEPARATION_WEIGHT;
        dir_len=sqrtf(length2(dir_x,dir_z));
        if(dir_len<0.001f)continue;
        dir_x/=dir_len;dir_z/=dir_len;
        e->angle=atan2f(dir_x,dir_z);
        step=e->speed*dt;
        if(step>dir_len)step=dir_len;
        nx=e->x+dir_x*step;nz=e->z+dir_z*step;
        if(worldNavBlocked(stage,limit,nx,nz,0.42f)) {
            /* Collision invalidates the cached segment; recalculate next tick. */
            r->repath_frames=0;
            continue;
        }
        e->x=nx;e->z=nz;
    }
}
