#include "pickup_math.h"
static int finite_float(float v){union {float f;unsigned bits;} x;x.f=v;return (x.bits&0x7f800000u)!=0x7f800000u;}
static int finite_vector(CvVec3 v){return finite_float(v.x)&&finite_float(v.y)&&finite_float(v.z);}
static CvVec3 rotate(CvQuat q,CvVec3 v){
 CvVec3 t={2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};
 return (CvVec3){v.x+q.w*t.x+q.y*t.z-q.z*t.y,v.y+q.w*t.y+q.z*t.x-q.x*t.z,v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
unsigned cv_pickup_grips(CvPickupGripState *s,unsigned active,const float grip[2]){
 for(unsigned h=0;h<2;h++){
  unsigned bit=1u<<h;
  if(!(active&bit)||!finite_float(grip[h])||grip[h]<.35f){s->down&=~bit;s->consumed&=~bit;}
  else if(grip[h]>.65f)s->down|=bit;
 }
 return s->down&~s->consumed&active;
}
int cv_pickup_hand_world(const CvFrame *f,const CvPose *a,const float ref[22],unsigned h,CvVec3 *out){
 if(!f||!a||!ref||!out||h>1||f->version!=CV_FRAME_VERSION||f->size!=sizeof *f||(f->grip[h].valid&3)!=3)return 0;
 CvQuat q=a->orientation;
 if(!finite_vector(a->position)||!finite_vector(f->grip[h].pose.position)||!finite_float(q.x)||!finite_float(q.y)||!finite_float(q.z)||!finite_float(q.w))return 0;
 float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
 if(norm<.99f||norm>1.01f)return 0;
 for(unsigned i=6;i<18;i++)if(!finite_float(ref[i]))return 0;
 q.x=-q.x;q.y=-q.y;q.z=-q.z;
 CvVec3 p=f->grip[h].pose.position;
 CvVec3 v=rotate(q,(CvVec3){p.x-a->position.x,p.y-a->position.y,p.z-a->position.z});
 /* Same 40 units/metre and untracked camera basis as renderer tracked_ref. */
 *out=(CvVec3){ref[6]+40*(-ref[12]*v.x+ref[15]*v.y-ref[9]*v.z),
               ref[7]+40*(-ref[13]*v.x+ref[16]*v.y-ref[10]*v.z),
               ref[8]+40*(-ref[14]*v.x+ref[17]*v.y-ref[11]*v.z)};
 return finite_vector(*out);
}
float cv_pickup_distance(CvVec3 p,CvVec3 minimum,CvVec3 maximum){
 if(!finite_vector(p)||!finite_vector(minimum)||!finite_vector(maximum))return -1;
 float a[3]={p.x,p.y,p.z},lo[3]={minimum.x,minimum.y,minimum.z},hi[3]={maximum.x,maximum.y,maximum.z},sum=0;
 for(unsigned i=0;i<3;i++){
  if(lo[i]>hi[i]||hi[i]-lo[i]>160)return -1;
  float d=a[i]<lo[i]?lo[i]-a[i]:(a[i]>hi[i]?a[i]-hi[i]:0);
  sum+=d*d;
 }
 return sum;
}
