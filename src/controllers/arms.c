#include "arms.h"
#include "cv_math.h"
#include <float.h>

static cod2vr_vec3 v(double x,double y,double z) { return (cod2vr_vec3){(float)x,(float)y,(float)z}; }
static cod2vr_vec3 add(cod2vr_vec3 a,cod2vr_vec3 b) { return v(a.x+b.x,a.y+b.y,a.z+b.z); }
static cod2vr_vec3 sub(cod2vr_vec3 a,cod2vr_vec3 b) { return v(a.x-b.x,a.y-b.y,a.z-b.z); }
static cod2vr_vec3 mul(cod2vr_vec3 a,double s) { return v(a.x*s,a.y*s,a.z*s); }
static double dot(cod2vr_vec3 a,cod2vr_vec3 b) { return (double)a.x*b.x+(double)a.y*b.y+(double)a.z*b.z; }
static cod2vr_vec3 cross(cod2vr_vec3 a,cod2vr_vec3 b) { return v((double)a.y*b.z-(double)a.z*b.y,(double)a.z*b.x-(double)a.x*b.z,(double)a.x*b.y-(double)a.y*b.x); }
static int finitev(cod2vr_vec3 a) { return isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z); }
static int normq(cod2vr_quat q,cod2vr_quat *o) {
    double n=(double)q.x*q.x+(double)q.y*q.y+(double)q.z*q.z+(double)q.w*q.w;
    if (!isfinite(n)||n<1e-20) return 0;
    n=1/sqrt(n); *o=(cod2vr_quat){(float)(q.x*n),(float)(q.y*n),(float)(q.z*n),(float)(q.w*n)};return 1;
}
static cod2vr_quat qm(cod2vr_quat a,cod2vr_quat b) {
    return (cod2vr_quat){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
static cod2vr_vec3 rotate(cod2vr_quat q,cod2vr_vec3 p) {
    cod2vr_vec3 u=v(q.x,q.y,q.z),t=mul(cross(u,p),2);
    return add(p,add(mul(t,q.w),cross(u,t)));
}
static int finitepose(cod2vr_pose p) { cod2vr_quat q;return finitev(p.position)&&normq(p.orientation,&q); }

int cod2vr_controller_wrist(const CvTrackedPose *g,int64_t pt,int64_t ft,
    const cod2vr_pose *cal,const cod2vr_space *s,cod2vr_pose *out) {
    cod2vr_quat grip,offset,basis;
    if (!g||!cal||!s||!out||pt!=ft||
        (g->valid&(CV_POSE_POSITION_VALID|CV_POSE_ORIENTATION_VALID))!=(CV_POSE_POSITION_VALID|CV_POSE_ORIENTATION_VALID)||
        !finitepose(g->pose)||!finitepose(*cal)||!finitev(s->origin)||
        !isfinite(s->units_per_meter)||s->units_per_meter<=0||
        !normq(g->pose.orientation,&grip)||!normq(cal->orientation,&offset)||!normq(s->tracking_to_game,&basis))return 0;
    cod2vr_pose result;
    result.position=add(s->origin,mul(rotate(basis,add(g->pose.position,rotate(grip,cal->position))),s->units_per_meter));
    if (!normq(qm(basis,qm(grip,offset)),&result.orientation)||!finitev(result.position))return 0;
    *out=result;return 1;
}

static cod2vr_vec3 perpendicular(cod2vr_vec3 direction,cod2vr_vec3 hint) {
    return sub(hint,mul(direction,dot(direction,hint)));
}
static cod2vr_quat swing(cod2vr_vec3 direction) {
    cod2vr_quat q;
    /* from +X: handle antiparallel without division by zero */
    if (direction.x < -0.999999f)return (cod2vr_quat){0,0,1,0};
    normq((cod2vr_quat){0,-direction.z,direction.y,1+direction.x},&q);return q;
}
int cod2vr_solve_arm(const cod2vr_arm_input *in,cod2vr_arm_output *out) {
    if (!in||!out||!finitev(in->shoulder)||!finitepose(in->wrist_target)||!finitev(in->elbow_hint)||
        (in->previous_valid&&!finitev(in->previous_elbow))||
        !isfinite(in->upper_length)||!isfinite(in->lower_length)||in->upper_length<=0||in->lower_length<=0)return 0;
    double a=in->upper_length,b=in->lower_length,sum=a+b;
    if (!isfinite(sum)||sum>FLT_MAX/4)return 0;
    cod2vr_vec3 delta=sub(in->wrist_target.position,in->shoulder);
    double distance=sqrt(dot(delta,delta));
    cod2vr_vec3 dir;
    if(distance>sum*1e-7)dir=mul(delta,1/distance);
    else {
        cod2vr_vec3 old=in->previous_valid?sub(in->previous_elbow,in->shoulder):v(1,0,0);
        double n=sqrt(dot(old,old));dir=n>sum*1e-7?mul(old,1/n):v(1,0,0);
    }
    double low=fabs(a-b), high=sum, d=distance;
    if(d<low)d=low;
    if(d>high)d=high;
    cod2vr_arm_output result;
    result.shoulder=in->shoulder;
    result.wrist=in->wrist_target;
    normq(in->wrist_target.orientation,&result.wrist.orientation);
    result.reach_clamped=(distance<low||distance>high);
    result.wrist.position=add(in->shoulder,mul(dir,d));
    cod2vr_vec3 pole=perpendicular(dir,sub(in->elbow_hint,in->shoulder));
    double pn=sqrt(dot(pole,pole));
    if(pn<sum*1e-6&&in->previous_valid){pole=perpendicular(dir,sub(in->previous_elbow,in->shoulder));pn=sqrt(dot(pole,pole));}
    if(pn<sum*1e-6){
        cod2vr_vec3 axis=fabs(dir.x)<=fabs(dir.y)&&fabs(dir.x)<=fabs(dir.z)?v(1,0,0):(fabs(dir.y)<=fabs(dir.z)?v(0,1,0):v(0,0,1));
        pole=perpendicular(dir,axis);pn=sqrt(dot(pole,pole));
    }
    pole=mul(pole,1/pn);
    /* Coincident equal-length chain: any elbow on radius a is valid. */
    double along=d>sum*1e-7?(d*d+a*a-b*b)/(2*d):0;
    if(along>a)along=a;
    if(along< -a)along= -a;
    double height=sqrt(fmax(0,a*a-along*along));
    result.elbow=add(in->shoulder,add(mul(dir,along),mul(pole,height)));
    cod2vr_vec3 upper=mul(sub(result.elbow,result.shoulder),1/a);
    cod2vr_vec3 lower=mul(sub(result.wrist.position,result.elbow),1/b);
    result.upper_swing=swing(upper);result.lower_swing=swing(lower);
    if(!finitev(result.elbow)||!finitev(result.wrist.position))return 0;
    *out=result;return 1;
}
