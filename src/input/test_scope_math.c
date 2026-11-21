#include "../include/vr_scope_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){
 CvFrame f={.version=1,.size=sizeof f};CvPose anchor={.orientation={0,0,0,1}};
 float ref[22]={0},scope[22];ref[6]=100;ref[7]=200;ref[8]=60;ref[9]=ref[13]=ref[17]=1;
 f.aim[1]=(CvTrackedPose){.valid=3,.pose={.orientation={0,0,0,1},.position={.2f,-.3f,-.4f}}};
 assert(cv_scope_ref(&f,&anchor,ref,scope));
 assert(fabsf(scope[6]-124)<1e-5&&fabsf(scope[7]-192)<1e-5&&fabsf(scope[8]-48)<1e-5);
 assert(scope[9]==1&&scope[13]==1&&scope[17]==1);
 /* Scope's centre ray is the identical +forward basis copied to firing. */
 float ray[3]={scope[6]+scope[9]*100,scope[7]+scope[10]*100,scope[8]+scope[11]*100};
 assert(fabsf(ray[0]-224)<1e-5&&fabsf(ray[1]-192)<1e-5);
 f.aim[1].pose.orientation=(CvQuat){0,.70710678f,0,.70710678f};
 assert(cv_scope_ref(&f,&anchor,ref,scope));
 assert(fabsf(scope[9])<1e-5&&fabsf(scope[10]-1)<1e-5);
 anchor.orientation=f.aim[1].pose.orientation;
 assert(cv_scope_ref(&f,&anchor,ref,scope));assert(fabsf(scope[9]-1)<1e-5&&fabsf(scope[10])<1e-5);
 f.aim[1].valid=0;assert(!cv_scope_ref(&f,&anchor,ref,scope));
 f.aim[1].valid=3;f.aim[1].pose.position.x=20;assert(!cv_scope_ref(&f,&anchor,ref,scope));
 f.aim[1].pose.position.x=.2f;f.aim[1].pose.orientation.w=NAN;assert(!cv_scope_ref(&f,&anchor,ref,scope));
 puts("PASS: shared scoped-camera/fire ray, tracked aim yaw, recenter cancellation, validity and range guards");
}
