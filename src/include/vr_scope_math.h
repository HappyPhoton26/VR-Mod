#ifndef CV_SCOPE_MATH_H
#define CV_SCOPE_MATH_H
#include "vr_pose.h"
static int cvscope_finite(float v){union {float f;unsigned bits;} n;n.f=v;return (n.bits&0x7f800000u)!=0x7f800000u;}
static CvVec3 cvscope_rotate(CvQuat q,CvVec3 v){
 CvVec3 t={2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};
 return (CvVec3){v.x+q.w*t.x+q.y*t.z-q.z*t.y,v.y+q.w*t.y+q.z*t.x-q.x*t.z,v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
static CvVec3 cvscope_game(CvVec3 p,const float *r){
 return (CvVec3){-r[12]*p.x+r[15]*p.y-r[9]*p.z,-r[13]*p.x+r[16]*p.y-r[10]*p.z,-r[14]*p.x+r[17]*p.y-r[11]*p.z};
}
/* Original scoped weapons stop submitting their viewmodel. Build one shared
 * camera/fire basis directly from the tracked aim action in that case. */
static int cv_scope_ref(const CvFrame *f,const CvPose *anchor,const float *ref,float *out){
 if(!f||!anchor||!ref||!out||f->version!=1||f->size!=sizeof *f||(f->aim[1].valid&3)!=3)return 0;
 CvPose aim=f->aim[1].pose;CvQuat a=anchor->orientation,q=aim.orientation;
 const float *values=(const float *)&aim;for(unsigned i=0;i<7;i++)if(!cvscope_finite(values[i]))return 0;
 values=(const float *)anchor;for(unsigned i=0;i<7;i++)if(!cvscope_finite(values[i]))return 0;
 float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w,an=a.x*a.x+a.y*a.y+a.z*a.z+a.w*a.w;
 if(n<.99f||n>1.01f||an<.99f||an>1.01f)return 0;
 for(unsigned i=6;i<18;i++)if(!cvscope_finite(ref[i]))return 0;
 a.x=-a.x;a.y=-a.y;a.z=-a.z;
 CvQuat relative={a.w*q.x+a.x*q.w+a.y*q.z-a.z*q.y,a.w*q.y-a.x*q.z+a.y*q.w+a.z*q.x,a.w*q.z+a.x*q.y-a.y*q.x+a.z*q.w,a.w*q.w-a.x*q.x-a.y*q.y-a.z*q.z};
 CvVec3 delta={aim.position.x-anchor->position.x,aim.position.y-anchor->position.y,aim.position.z-anchor->position.z};
 CvVec3 p=cvscope_game(cvscope_rotate(a,delta),ref);
 CvVec3 forward=cvscope_game(cvscope_rotate(relative,(CvVec3){0,0,-1}),ref);
 CvVec3 left=cvscope_game(cvscope_rotate(relative,(CvVec3){-1,0,0}),ref);
 CvVec3 up=cvscope_game(cvscope_rotate(relative,(CvVec3){0,1,0}),ref);
 if(p.x*p.x+p.y*p.y+p.z*p.z>6.25f)return 0;
 for(unsigned i=0;i<22;i++)out[i]=ref[i];
 out[6]=ref[6]+40*p.x+8*forward.x;out[7]=ref[7]+40*p.y+8*forward.y;out[8]=ref[8]+40*p.z+8*forward.z;
 out[9]=forward.x;out[10]=forward.y;out[11]=forward.z;
 out[12]=left.x;out[13]=left.y;out[14]=left.z;out[15]=up.x;out[16]=up.y;out[17]=up.z;
 return 1;
}
#endif
