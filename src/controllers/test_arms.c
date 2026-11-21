#include "arms.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static const CvQuat identity={0,0,0,1};
static double dist(CvVec3 a,CvVec3 b){return sqrt(pow(a.x-b.x,2)+pow(a.y-b.y,2)+pow(a.z-b.z,2));}
static CvVec3 point(float x,float y,float z){return (CvVec3){x,y,z};}
static float random_value(void){return (float)rand()/RAND_MAX*6-3;}
static void check_chain(const cod2vr_arm_input *in,const cod2vr_arm_output *o){
    assert(fabs(dist(o->shoulder,o->elbow)-in->upper_length)<2e-5);
    assert(fabs(dist(o->elbow,o->wrist.position)-in->lower_length)<2e-5);
    double d=dist(in->shoulder,in->wrist_target.position);
    if(d>=fabs(in->upper_length-in->lower_length)&&d<=in->upper_length+in->lower_length)
        assert(dist(o->wrist.position,in->wrist_target.position)<2e-5);
}
int main(void){
    CvTrackedPose grip={.pose={.orientation={0,0,0,1},.position={1,2,3}},.valid=3};
    CvPose cal={.orientation={0,0,0,1},.position={.1f,0,0}}, wrist;
    cod2vr_space space={.origin={10,20,30},.tracking_to_game={0,0,0,1},.units_per_meter=2};
    assert(cod2vr_controller_wrist(&grip,42,42,&cal,&space,&wrist));
    assert(dist(wrist.position,point(12.2f,24,36))<1e-5);
    /* OpenXR +X right -> game -Y, +Y up -> game +Z, -Z forward -> game +X.
       Proper cyclic basis rotation; this is caller calibration, not hardcoded. */
    space=(cod2vr_space){.origin={0,0,0},.tracking_to_game={-.5f,.5f,.5f,-.5f},.units_per_meter=1};
    cal.position=point(0,0,0);grip.pose.position=point(1,2,-3);
    assert(cod2vr_controller_wrist(&grip,42,42,&cal,&space,&wrist));
    assert(dist(wrist.position,point(3,-1,2))<1e-5);
    /* Grip-local wrist offset rotates with controller. */
    space.tracking_to_game=identity;grip.pose.orientation=(CvQuat){0,0,.70710678f,.70710678f};
    grip.pose.position=point(0,0,0);cal.position=point(.1f,0,0);
    assert(cod2vr_controller_wrist(&grip,42,42,&cal,&space,&wrist));
    assert(dist(wrist.position,point(0,.1f,0))<1e-5);
    CvPose saved=wrist;
    assert(!cod2vr_controller_wrist(&grip,41,42,&cal,&space,&wrist));
    assert(!memcmp(&saved,&wrist,sizeof wrist));
    grip.valid=CV_POSE_ORIENTATION_VALID;
    assert(!cod2vr_controller_wrist(&grip,42,42,&cal,&space,&wrist));
    grip.valid=3;grip.pose.orientation=(CvQuat){0,0,0,0};
    assert(!cod2vr_controller_wrist(&grip,42,42,&cal,&space,&wrist));
    cod2vr_arm_input in={.shoulder={0,0,0},.wrist_target={.orientation={0,0,0,1},.position={1,0,0}},.elbow_hint={0,1,0},.upper_length=1,.lower_length=1};
    cod2vr_arm_output o;
    assert(cod2vr_solve_arm(&in,&o));check_chain(&in,&o);
    assert(o.elbow.y>0);assert(!o.reach_clamped);
    /* Full extension, beyond reach, fully folded, unequal lengths, and pole parallel. */
    const CvVec3 targets[]={{2,0,0},{4,0,0},{0,0,0},{-2,0,0},{1,0,0}};
    for(unsigned k=0;k<sizeof targets/sizeof targets[0];++k){
        in.wrist_target.position=targets[k];in.elbow_hint=point(10,0,0);
        assert(cod2vr_solve_arm(&in,&o));check_chain(&in,&o);
    }
    in.upper_length=1;in.lower_length=.4;in.wrist_target.position=point(0,0,0);
    assert(cod2vr_solve_arm(&in,&o));check_chain(&in,&o);assert(o.reach_clamped);
    /* Previous elbow supplies bend when pole degenerates. */
    in.upper_length=in.lower_length=1;in.wrist_target.position=point(1,0,0);
    in.previous_valid=1;in.previous_elbow=point(.5f,-.8f,0);
    assert(cod2vr_solve_arm(&in,&o));assert(o.elbow.y<0);
    srand(12630);
    for(int k=0;k<10000;++k){
        in.shoulder=point(random_value(),random_value(),random_value());
        in.wrist_target.position=point(random_value(),random_value(),random_value());
        in.elbow_hint=point(random_value(),random_value(),random_value());
        in.upper_length=.3+fabs(random_value());in.lower_length=.3+fabs(random_value());
        assert(cod2vr_solve_arm(&in,&o));check_chain(&in,&o);
    }
    cod2vr_arm_output before=o;
    in.upper_length=0;assert(!cod2vr_solve_arm(&in,&o));assert(!memcmp(&before,&o,sizeof o));
    in.upper_length=1;in.wrist_target.position.x=NAN;assert(!cod2vr_solve_arm(&in,&o));
    puts("PASS: pose conversion, tracking guards, reach/singularity cases, 10000 IK chains");
}
