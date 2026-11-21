#include "game_hands.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static double length(CvVec3 a,CvVec3 b){double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return sqrt(x*x+y*y+z*z);}
int main(void){
    CvFrame frame={.version=CV_FRAME_VERSION,.size=sizeof frame};
    frame.grip[0]=(CvTrackedPose){.valid=3,.pose={.orientation={0,0,0,1},.position={1.1f,1.9f,2.7f}}};
    CvPose anchor={.orientation={0,0,0,1},.position={1,2,3}},calibration[2]={{{0,0,0,1},{0,0,0}},{{0,0,0,1},{0,0,0}}};
    float ref[22]={0},axes[9]={1,0,0,0,1,0,0,0,1};ref[6]=10;ref[7]=20;ref[8]=30;ref[9]=ref[13]=ref[17]=1;
    CvTrackedPose targets[2];
    assert(cvhands_tracking_to_model(&frame,&anchor,ref,axes,(CvVec3){10,20,30},40,calibration,targets));
    assert(length(targets[0].pose.position,(CvVec3){12,-4,-4})<2e-5);assert(!targets[1].valid);
    axes[0]=0;axes[1]=1;axes[3]=-1;axes[4]=0;
    assert(cvhands_tracking_to_model(&frame,&anchor,ref,axes,(CvVec3){10,20,30},40,calibration,targets));
    assert(length(targets[0].pose.position,(CvVec3){-4,-12,-4})<2e-5);
    axes[0]=1;axes[1]=0;axes[3]=0;axes[4]=1;
    anchor.orientation=(CvQuat){0,0,.70710678f,.70710678f};
    frame.grip[0].pose.position=(CvVec3){1.1f,2.1f,2.7f};
    assert(cvhands_tracking_to_model(&frame,&anchor,ref,axes,(CvVec3){10,20,30},40,calibration,targets));
    assert(length(targets[0].pose.position,(CvVec3){12,-4,-4})<2e-5);
    axes[0]=0;assert(!cvhands_tracking_to_model(&frame,&anchor,ref,axes,(CvVec3){10,20,30},40,calibration,targets));
    const char *names[]={"root","j_shoulder_le","j_elbow_le","j_wrist_le","j_index_le_0","j_shoulder_ri","j_elbow_ri","j_wrist_ri","j_thumb_ri_0"};
    const int parents[]={-1,0,1,2,3,0,5,6,7};
    CvGameRig rig;assert(cvhands_bind(&rig,9,names,parents));
    CvPose src[9],out[9];
    for(int i=0;i<9;i++)src[i]=(CvPose){.orientation={0,0,0,1},.position={0,0,0}};
    src[1].position=(CvVec3){0,.3f,0};src[2].position=(CvVec3){.5f,.3f,0};src[3].position=(CvVec3){1,.3f,0};src[4].position=(CvVec3){1.1f,.3f,0};
    src[5].position=(CvVec3){0,-.3f,0};src[6].position=(CvVec3){.5f,-.3f,0};src[7].position=(CvVec3){1,-.3f,0};src[8].position=(CvVec3){1.1f,-.3f,0};
    CvTrackedPose wrists[2]={{.valid=3,.pose={.orientation={0,0,.70710678f,.70710678f},.position={.5f,.3f,.5f}}},{.valid=0}};
    CvGameHandsState state={0};
    assert(cvhands_retarget(&rig,src,wrists,&state,out));
    assert(length(out[3].position,wrists[0].pose.position)<1e-5);
    assert(fabs(length(out[1].position,out[2].position)-.5)<1e-5);
    assert(fabs(length(out[2].position,out[3].position)-.5)<1e-5);
    assert(length(out[4].position,(CvVec3){.5f,.4f,.5f})<1e-5);
    assert(out[2].position.y>src[1].position.y); /* left elbow points outward */
    for(int i=5;i<9;i++)assert(length(out[i].position,src[i].position)<1e-5);
    /* In-place output and alternating lost tracking are supported. */
    CvPose inplace[9];memcpy(inplace,src,sizeof src);
    assert(cvhands_retarget(&rig,inplace,wrists,&state,inplace));
    assert(length(inplace[4].position,out[4].position)<1e-5);
    wrists[0].valid=0;assert(cvhands_retarget(&rig,src,wrists,&state,out));assert(!state.previous_valid[0]);
    for(int i=0;i<9;i++)assert(length(out[i].position,src[i].position)<1e-5);
    state=(CvGameHandsState){.wrist_limit_mask=1};
    wrists[0]=(CvTrackedPose){.valid=3,.pose={.orientation={0,.8660254f,0,.5f},.position={1,.3f,0}}};
    assert(cvhands_retarget(&rig,src,wrists,&state,out));
    assert(fabs(fabs(out[3].orientation.w)-.88701083)<1e-5); /* bendlimited55deg */
    state.previous_valid[0]=0;wrists[0].pose.orientation=(CvQuat){.8660254f,0,0,.5f};
    assert(cvhands_retarget(&rig,src,wrists,&state,out));assert(fabs(fabs(out[3].orientation.w)-.5f)<1e-5); /* axialroll preserved */
    state=(CvGameHandsState){0};wrists[0].pose.position=(CvVec3){3,.3f,0};
    assert(cvhands_retarget(&rig,src,wrists,&state,out));assert(length(out[3].position,wrists[0].pose.position)>1.9); /* oldclamp breaks contact */
    state=(CvGameHandsState){.contact_lock_mask=1};
    assert(cvhands_retarget(&rig,src,wrists,&state,out));assert(length(out[3].position,wrists[0].pose.position)>1.9);
    assert(length(out[1].position,src[1].position)<2e-5); /* shoulder remains attached */
    assert(fabs(length(out[1].position,out[2].position)-.5)<2e-5&&fabs(length(out[2].position,out[3].position)-.5)<2e-5);
    CvVec3 constrained;assert(cvhands_clamp_wrist(&rig,src,src[1].position,0,wrists[0].pose.position,&constrained));
    assert(length(constrained,src[1].position)<.971);
    wrists[0].pose.position=constrained;assert(cvhands_retarget(&rig,src,wrists,&state,out));
    assert(length(out[3].position,constrained)<2e-5);
    int bad[9];memcpy(bad,parents,sizeof bad);bad[3]=0;assert(!cvhands_bind(&rig,9,names,bad));
    assert(cvhands_bind(&rig,9,names,parents));
    src[2].position=src[1].position;wrists[0].valid=3;assert(!cvhands_retarget(&rig,src,wrists,&state,out));
    puts("PASS: game-bone binding, animation-derived lengths, original finger animation propagation, tracking loss");
}
