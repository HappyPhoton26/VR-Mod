#include "pickup_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){
 CvPickupGripState s={0};float grip[2]={.8f,0};
 assert(cv_pickup_grips(&s,3,grip)==1);
 assert(cv_pickup_grips(&s,3,grip)==1); /* can reach into range while squeezing */
 s.consumed|=1;assert(!cv_pickup_grips(&s,3,grip));
 grip[0]=.5f;assert(!cv_pickup_grips(&s,3,grip));
 grip[0]=.2f;assert(!cv_pickup_grips(&s,3,grip));
 grip[0]=.8f;grip[1]=.9f;assert(cv_pickup_grips(&s,3,grip)==3);
 assert(!cv_pickup_grips(&s,0,grip));assert(!s.down&&!s.consumed);
 grip[0]=NAN;grip[1]=0;assert(!cv_pickup_grips(&s,3,grip));
 CvFrame f={.version=1,.size=sizeof f};CvPose anchor={.orientation={0,0,0,1},.position={1,2,3}};
 float ref[22]={0};ref[6]=100;ref[7]=200;ref[8]=60;ref[9]=1;ref[13]=1;ref[17]=1;
 f.grip[0]=(CvTrackedPose){.pose={.position={1.25f,1.5f,2}},.valid=3};CvVec3 world;
 assert(cv_pickup_hand_world(&f,&anchor,ref,0,&world));
 assert(fabsf(world.x-140)<1e-5&&fabsf(world.y-190)<1e-5&&fabsf(world.z-40)<1e-5);
 anchor.orientation=(CvQuat){0,.70710678f,0,.70710678f};
 f.grip[0].pose.position=(CvVec3){0,1.5f,2.75f};
 assert(cv_pickup_hand_world(&f,&anchor,ref,0,&world));
 assert(fabsf(world.x-140)<2e-5&&fabsf(world.y-190)<2e-5&&fabsf(world.z-40)<2e-5);
 f.grip[0].valid=0;assert(!cv_pickup_hand_world(&f,&anchor,ref,0,&world));
 assert(!cv_pickup_hand_world(&f,&anchor,ref,2,&world));
 CvVec3 lo={0,0,0},hi={4,4,4};
 assert(cv_pickup_distance((CvVec3){2,2,2},lo,hi)==0);
 assert(cv_pickup_distance((CvVec3){7,8,4},lo,hi)==25);
 assert(cv_pickup_distance((CvVec3){NAN,0,0},lo,hi)<0);
 assert(cv_pickup_distance(world,hi,lo)<0);
 puts("PASS: grip hysteresis, one pickup per squeeze, held reach, focus clearing, tracked world conversion, item bounds");
}
