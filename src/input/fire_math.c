#include "fire_math.h"
static int finite_float(float v){union {float f;unsigned int bits;} x;x.f=v;return (x.bits&0x7f800000u)!=0x7f800000u;}
static CvVec3 rotate(CvQuat q,CvVec3 v){
 CvVec3 t={2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};
 return (CvVec3){v.x+q.w*t.x+q.y*t.z-q.z*t.y,v.y+q.w*t.y+q.z*t.x-q.x*t.z,v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
int cv_fire_apply(float *out,unsigned int entity,unsigned int weapon_type,const CvPose *muzzle,unsigned long long source_sequence,long long source_time,const CvFrame *frame,const float *camera){
 if(!out||!muzzle||!frame||!camera||entity!=0x1b15f00u||weapon_type!=0)return 0;
 if(frame->version!=CV_FRAME_VERSION||frame->size!=sizeof *frame||(frame->aim[1].valid&3)!=3||(frame->grip[1].valid&3)!=3)return 0;
 if(frame->sequence<source_sequence||frame->sequence-source_sequence>3||source_time<=0||frame->predicted_display_time_ns<=0)return 0;
 unsigned long long a=(unsigned long long)frame->predicted_display_time_ns,b=(unsigned long long)source_time,age=a>b?a-b:b-a;
 if(age>100000000u)return 0;
 CvQuat q=muzzle->orientation;
 if(!finite_float(q.x)||!finite_float(q.y)||!finite_float(q.z)||!finite_float(q.w)||!finite_float(muzzle->position.x)||!finite_float(muzzle->position.y)||!finite_float(muzzle->position.z))return 0;
 float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;if(norm<.99f||norm>1.01f)return 0;
 for(unsigned int i=0;i<3;i++)if(!finite_float(camera[i]))return 0;
 float dx=muzzle->position.x-camera[0],dy=muzzle->position.y-camera[1],dz=muzzle->position.z-camera[2];if(dx*dx+dy*dy+dz*dz>256.f*256.f)return 0;
 CvVec3 forward=rotate(q,(CvVec3){1,0,0}),right=rotate(q,(CvVec3){0,-1,0}),up=rotate(q,(CvVec3){0,0,1});
 out[0]=forward.x;out[1]=forward.y;out[2]=forward.z;
 out[3]=right.x;out[4]=right.y;out[5]=right.z;
 out[6]=up.x;out[7]=up.y;out[8]=up.z;
 out[9]=muzzle->position.x;out[10]=muzzle->position.y;out[11]=muzzle->position.z;return 1;
}
