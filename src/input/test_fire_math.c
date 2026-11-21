#include "fire_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static int close_float(float a,float b){return fabsf(a-b)<.0001f;}
int main(void){
 CvFrame f={.version=CV_FRAME_VERSION,.size=sizeof f,.sequence=100,.predicted_display_time_ns=1000000000};f.grip[1].valid=f.aim[1].valid=3;
 CvPose muzzle={.orientation={0,0,0,1},.position={12,3,4}};float origin[3]={0},out[12];
#define APPLY(entity,type,seq,time) cv_fire_apply(out,entity,type,&muzzle,seq,time,&f,origin)
 assert(APPLY(0x1b15f00,0,100,1000000000));
 assert(out[0]==1&&out[4]==-1&&out[8]==1&&out[9]==12&&out[10]==3&&out[11]==4);
 muzzle.orientation=(CvQuat){0,0,.70710678f,.70710678f};
 assert(APPLY(0x1b15f00,0,100,1000000000));assert(close_float(out[0],0)&&close_float(out[1],1)&&close_float(out[3],1)&&close_float(out[4],0)&&close_float(out[8],1));
 for(int i=0;i<12;i++)out[i]=77;
 assert(!APPLY(0x1b160fc,0,100,1000000000));for(int i=0;i<12;i++)assert(out[i]==77);
 assert(!APPLY(0x1b15f00,1,100,1000000000));
 assert(!APPLY(0x1b15f00,0,96,1000000000));assert(!APPLY(0x1b15f00,0,101,1000000000));
 assert(!APPLY(0x1b15f00,0,100,899999999));assert(!APPLY(0x1b15f00,0,100,1100000001));
 assert(APPLY(0x1b15f00,0,97,900000000));
 f.aim[1].valid=1;assert(!APPLY(0x1b15f00,0,100,1000000000));f.aim[1].valid=3;
 f.grip[1].valid=0;assert(!APPLY(0x1b15f00,0,100,1000000000));f.grip[1].valid=3;
 muzzle.orientation.w=NAN;assert(!APPLY(0x1b15f00,0,100,1000000000));muzzle.orientation=(CvQuat){0,0,0,1};
 muzzle.position.x=INFINITY;assert(!APPLY(0x1b15f00,0,100,1000000000));muzzle.position.x=257;assert(!APPLY(0x1b15f00,0,100,1000000000));
 muzzle.position.x=0;origin[0]=NAN;assert(!APPLY(0x1b15f00,0,100,1000000000));
 puts("PASS muzzle ray: player/type/freshness/tracking/finite/range guards and +X barrel basis");return 0;
}
