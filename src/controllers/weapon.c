#include "weapon.h"
#include "cv_math.h"
static CvQuat product(CvQuat a,CvQuat b){return (CvQuat){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
static CvVec3 rotate(CvQuat q,CvVec3 v){CvVec3 t={2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};return (CvVec3){v.x+q.w*t.x+q.y*t.z-q.z*t.y,v.y+q.w*t.y+q.z*t.x-q.x*t.z,v.z+q.w*t.z+q.x*t.y-q.y*t.x};}
static int unit(CvQuat *q){double n=(double)q->x*q->x+(double)q->y*q->y+(double)q->z*q->z+(double)q->w*q->w;if(!isfinite(n)||n<1e-12)return 0;double s=1/sqrt(n);q->x=(float)(q->x*s);q->y=(float)(q->y*s);q->z=(float)(q->z*s);q->w=(float)(q->w*s);return 1;}
static int finite_position(CvVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static CvVec3 subtract(CvVec3 a,CvVec3 b){return (CvVec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static double dot(CvVec3 a,CvVec3 b){return (double)a.x*b.x+(double)a.y*b.y+(double)a.z*b.z;}
static CvVec3 scaled(CvVec3 a,double s){return (CvVec3){(float)(a.x*s),(float)(a.y*s),(float)(a.z*s)};}
static CvVec3 cross(CvVec3 a,CvVec3 b){return (CvVec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static CvQuat swing_between(CvVec3 a,CvVec3 b){
    a=scaled(a,1/sqrt(dot(a,a)));b=scaled(b,1/sqrt(dot(b,b)));
    CvVec3 axis=cross(a,b);CvQuat result;
    if(dot(a,b)<-.999999){CvVec3 reference=fabs(a.x)<fabs(a.y)?(CvVec3){1,0,0}:(CvVec3){0,1,0};axis=cross(a,reference);axis=scaled(axis,1/sqrt(dot(axis,axis)));return (CvQuat){axis.x,axis.y,axis.z,0};}
    result=(CvQuat){axis.x,axis.y,axis.z,(float)(1+dot(a,b))};unit(&result);return result;
}
CvPose cvweapon_compose(CvPose parent,CvPose local){CvVec3 p=rotate(parent.orientation,local.position);return (CvPose){.orientation=product(parent.orientation,local.orientation),.position={p.x+parent.position.x,p.y+parent.position.y,p.z+parent.position.z}};}
CvPose cvweapon_inverse(CvPose p){p.orientation.x=-p.orientation.x;p.orientation.y=-p.orientation.y;p.orientation.z=-p.orientation.z;p.position=rotate(p.orientation,(CvVec3){-p.position.x,-p.position.y,-p.position.z});return p;}
int cvweapon_attach(const CvPose *wrist,const CvPose *muzzle,CvVec3 grip,CvQuat aim,CvPose *out){
    if(!wrist||!muzzle||!out||!finite_position(wrist->position)||!finite_position(grip)||!finite_position(muzzle->position))return 0;
    CvQuat muzzle_q=muzzle->orientation;if(!unit(&muzzle_q)||!unit(&aim))return 0;
    muzzle_q.x=-muzzle_q.x;muzzle_q.y=-muzzle_q.y;muzzle_q.z=-muzzle_q.z;
    CvPose result={.orientation=product(aim,muzzle_q)};
    if(!unit(&result.orientation))return 0;
    CvVec3 p=rotate(result.orientation,wrist->position);
    result.position=(CvVec3){grip.x-p.x,grip.y-p.y,grip.z-p.z};
    if(!finite_position(result.position))return 0;
    *out=result;return 1;
}
int cvweapon_twohand(const CvPose *nr,const CvPose *nl,CvVec3 right,CvVec3 left,float squeeze,int active,float scale,CvPose single,CvTwoHandState *state,CvPose *out,CvPose *locked_left){
    if(!nr||!nl||!state||!out||!locked_left||!isfinite(scale)||scale<=0||!isfinite(squeeze)||!finite_position(right)||!finite_position(left))return 0;
    CvPose result=single;int latched=state->latched;
    CvVec3 native_vector=subtract(nl->position,nr->position),tracked_vector=subtract(left,right);
    double nl2=dot(native_vector,native_vector),tl2=dot(tracked_vector,tracked_vector);
    if(!active||squeeze<.35f||nl2<scale*scale*.01||tl2<scale*scale*.01)latched=0;
    else{
        CvVec3 expected=cvweapon_compose(single,*nl).position,acquire=subtract(left,expected);
        if(!latched&&squeeze>.65f&&dot(acquire,acquire)<=scale*scale*.22*.22)latched=1;
        if(latched){
            CvQuat swing=swing_between(rotate(single.orientation,native_vector),tracked_vector);
            result.orientation=product(swing,single.orientation);unit(&result.orientation);
            CvVec3 p=rotate(result.orientation,nr->position);result.position=(CvVec3){right.x-p.x,right.y-p.y,right.z-p.z};
            CvVec3 release=subtract(left,cvweapon_compose(result,*nl).position);
            if(dot(release,release)>scale*scale*.40*.40){latched=0;result=single;}
        }
    }
    state->latched=latched;*out=result;*locked_left=cvweapon_compose(result,*nl);return 1;
}

int cvweapon_reload_reference(CvReloadAttachment *state,unsigned weapon_state,
    CvPose animated_wrist,CvPose animated_muzzle,CvPose *wrist,CvPose *muzzle){
    int reloading=weapon_state>=5u&&weapon_state<=9u;
    if(!state->valid||!reloading){
        state->wrist=animated_wrist;state->muzzle=animated_muzzle;state->valid=1;
    }
    *wrist=state->wrist;*muzzle=state->muzzle;
    return reloading;
}
