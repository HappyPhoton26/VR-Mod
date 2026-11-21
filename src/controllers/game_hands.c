#include "game_hands.h"
#include "cv_math.h"
#ifdef CV_FREESTANDING
void *memcpy(void *,const void *,unsigned int);
void *memset(void *,int,unsigned int);
int strcmp(const char *,const char *);
#else
#include <string.h>
#endif
static CvVec3 vv(double x,double y,double z){return (CvVec3){(float)x,(float)y,(float)z};}
static CvVec3 plus(CvVec3 a,CvVec3 b){return vv(a.x+b.x,a.y+b.y,a.z+b.z);}
static CvVec3 minus(CvVec3 a,CvVec3 b){return vv(a.x-b.x,a.y-b.y,a.z-b.z);}
static CvVec3 scale(CvVec3 a,double s){return vv(a.x*s,a.y*s,a.z*s);}
static double inner(CvVec3 a,CvVec3 b){return (double)a.x*b.x+(double)a.y*b.y+(double)a.z*b.z;}
static CvVec3 outer(CvVec3 a,CvVec3 b){return vv(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static CvQuat conjugate(CvQuat q){return (CvQuat){-q.x,-q.y,-q.z,q.w};}
static CvQuat product(CvQuat a,CvQuat b){return (CvQuat){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
static int unitq(CvQuat q,CvQuat *out){
    double n=(double)q.x*q.x+(double)q.y*q.y+(double)q.z*q.z+(double)q.w*q.w;
    if(!isfinite(n)||n<1e-20)return 0;
    n=1/sqrt(n);*out=(CvQuat){(float)(q.x*n),(float)(q.y*n),(float)(q.z*n),(float)(q.w*n)};return 1;
}
static CvVec3 rot(CvQuat q,CvVec3 v){CvVec3 t=scale(outer(vv(q.x,q.y,q.z),v),2);return plus(v,plus(scale(t,q.w),outer(vv(q.x,q.y,q.z),t)));}
static CvQuat between(CvVec3 a,CvVec3 b){
    a=scale(a,1/sqrt(inner(a,a)));b=scale(b,1/sqrt(inner(b,b)));
    double d=inner(a,b);CvVec3 axis;CvQuat q;
    if(d<-.999999){
        CvVec3 basis=fabs(a.x)<fabs(a.y)?vv(1,0,0):vv(0,1,0);
        axis=outer(a,basis);axis=scale(axis,1/sqrt(inner(axis,axis)));
        return (CvQuat){axis.x,axis.y,axis.z,0};
    }
    axis=outer(a,b);unitq((CvQuat){axis.x,axis.y,axis.z,(float)(1+d)},&q);return q;
}
static CvQuat axial_twist(CvQuat delta,CvVec3 axis){
    if(delta.w<0)delta=(CvQuat){-delta.x,-delta.y,-delta.z,-delta.w};
    double projection=delta.x*axis.x+delta.y*axis.y+delta.z*axis.z;
    CvQuat result={(float)(axis.x*projection),(float)(axis.y*projection),(float)(axis.z*projection),delta.w};
    if(!unitq(result,&result))result=(CvQuat){0,0,0,1};
    return result;
}
static CvQuat limited_wrist(CvQuat neutral,CvQuat desired,CvVec3 forearm_axis){
    CvQuat delta=product(desired,conjugate(neutral));unitq(delta,&delta);
    CvQuat twist=axial_twist(delta,forearm_axis),swing=product(delta,conjugate(twist));unitq(swing,&swing);
    if(swing.w<0)swing=(CvQuat){-swing.x,-swing.y,-swing.z,-swing.w};
    const double cos_limit=.8870108331782217,sin_limit=.4617486132350339;
    if(swing.w<cos_limit){double n=sqrt((double)swing.x*swing.x+(double)swing.y*swing.y+(double)swing.z*swing.z);if(n>1e-8)swing=(CvQuat){(float)(swing.x*sin_limit/n),(float)(swing.y*sin_limit/n),(float)(swing.z*sin_limit/n),(float)cos_limit};}
    return product(product(swing,twist),neutral);
}
static int basis_quat(CvVec3 x,CvVec3 y,CvVec3 z,CvQuat *out){
    if(fabs(inner(x,x)-1)>.01||fabs(inner(y,y)-1)>.01||fabs(inner(z,z)-1)>.01||
       fabs(inner(x,y))>.01||fabs(inner(x,z))>.01||fabs(inner(y,z))>.01||inner(outer(x,y),z)<.99)return 0;
    double trace=x.x+y.y+z.z;CvQuat q;
    if(trace>0){double s=sqrt(trace+1)*2;q=(CvQuat){(float)((y.z-z.y)/s),(float)((z.x-x.z)/s),(float)((x.y-y.x)/s),(float)(s/4)};}
    else if(x.x>y.y&&x.x>z.z){double s=sqrt(1+x.x-y.y-z.z)*2;q=(CvQuat){(float)(s/4),(float)((y.x+x.y)/s),(float)((z.x+x.z)/s),(float)((y.z-z.y)/s)};}
    else if(y.y>z.z){double s=sqrt(1+y.y-x.x-z.z)*2;q=(CvQuat){(float)((y.x+x.y)/s),(float)(s/4),(float)((z.y+y.z)/s),(float)((z.x-x.z)/s)};}
    else{double s=sqrt(1+z.z-x.x-y.y)*2;q=(CvQuat){(float)((z.x+x.z)/s),(float)((z.y+y.z)/s),(float)(s/4),(float)((x.y-y.x)/s)};}
    return unitq(q,out);
}
int cvhands_entity_pose(const float axes[9],CvVec3 origin,CvPose *pose){
    if(!axes||!pose||!isfinite(origin.x)||!isfinite(origin.y)||!isfinite(origin.z))return 0;
    CvPose result={.position=origin};
    if(!basis_quat(vv(axes[0],axes[1],axes[2]),vv(axes[3],axes[4],axes[5]),vv(axes[6],axes[7],axes[8]),&result.orientation))return 0;
    *pose=result;return 1;
}
int cvhands_tracking_to_model(const CvFrame *frame,const CvPose *anchor,const float ref[22],const float axes[9],CvVec3 origin,float scale_factor,const CvPose calibration[2],CvTrackedPose targets[2]){
    if(!frame||!anchor||!ref||!axes||!calibration||!targets||frame->version!=CV_FRAME_VERSION||frame->size!=sizeof *frame||!isfinite(scale_factor)||scale_factor<=0)return 0;
    CvQuat game_basis,entity_basis,anchor_q;
    CvVec3 right=vv(-ref[12],-ref[13],-ref[14]),up=vv(ref[15],ref[16],ref[17]),back=vv(-ref[9],-ref[10],-ref[11]);
    if(!basis_quat(right,up,back,&game_basis)||!basis_quat(vv(axes[0],axes[1],axes[2]),vv(axes[3],axes[4],axes[5]),vv(axes[6],axes[7],axes[8]),&entity_basis)||!unitq(anchor->orientation,&anchor_q))return 0;
    CvQuat inverse_entity=conjugate(entity_basis);
    cod2vr_space space={.tracking_to_game=product(inverse_entity,product(game_basis,conjugate(anchor_q))),.units_per_meter=scale_factor};
    space.origin=minus(rot(inverse_entity,minus(vv(ref[6],ref[7],ref[8]),origin)),scale(rot(space.tracking_to_game,anchor->position),scale_factor));
    CvTrackedPose result[2]={0};
    for(int h=0;h<2;h++){
        if((frame->grip[h].valid&3)!=3)continue;
        if(!cod2vr_controller_wrist(&frame->grip[h],frame->predicted_display_time_ns,frame->predicted_display_time_ns,&calibration[h],&space,&result[h].pose))return 0;
        result[h].valid=3;
    }
    memcpy(targets,result,sizeof result);return 1;
}
int cvhands_clamp_wrist(const CvGameRig *rig,const CvPose *native,CvVec3 shoulder,unsigned h,CvVec3 target,CvVec3 *result){
    if(!rig||!native||!result||h>1)return 0;
    int s=rig->shoulder[h],e=rig->elbow[h],w=rig->wrist[h];
    if(s<0||e<0||w<0||s>=(int)rig->count||e>=(int)rig->count||w>=(int)rig->count)return 0;
    CvVec3 upper=minus(native[e].position,native[s].position),lower=minus(native[w].position,native[e].position);
    double a=sqrt(inner(upper,upper)),b=sqrt(inner(lower,lower));
    if(!isfinite(a)||!isfinite(b)||a<=0||b<=0)return 0;
    if(a>12)a=12;
    if(b>10.4)b=10.4;
    CvVec3 reach=minus(target,shoulder);double distance=sqrt(inner(reach,reach));
    if(!isfinite(distance))return 0;
    double maximum=(a+b)*.97,minimum=fabs(a-b)+.001;
    if(distance>maximum)target=plus(shoulder,scale(reach,maximum/distance));
    else if(distance<minimum){
        CvVec3 direction=distance>1e-6?scale(reach,1/distance):vv(1,0,0);
        target=plus(shoulder,scale(direction,minimum));
    }
    *result=target;return 1;
}
int cvhands_bind(CvGameRig *out,unsigned n,const char *const *names,const int *parents){
    if(!out||!names||!parents||!n||n>CV_RIG_MAX_BONES)return 0;
    CvGameRig rig={.count=n,.shoulder={-1,-1},.elbow={-1,-1},.wrist={-1,-1},.wrist_twist={-1,-1}};
    const char *wanted[3][2]={{"j_shoulder_le","j_shoulder_ri"},{"j_elbow_le","j_elbow_ri"},{"j_wrist_le","j_wrist_ri"}};
    for(unsigned i=0;i<n;i++){
        if(!names[i]||parents[i]<-1||parents[i]>=(int)i)return 0;
        rig.parent[i]=parents[i];
        if(!strcmp(names[i],"j_wristtwist_le"))rig.wrist_twist[0]=(int)i;
        if(!strcmp(names[i],"j_wristtwist_ri"))rig.wrist_twist[1]=(int)i;
        for(int k=0;k<3;k++)for(int h=0;h<2;h++)if(!strcmp(names[i],wanted[k][h])){
            int *index=(k==0?rig.shoulder:k==1?rig.elbow:rig.wrist);
            if(index[h]>=0)return 0;
            index[h]=(int)i;
        }
    }
    for(int h=0;h<2;h++)if(rig.shoulder[h]<0||rig.elbow[h]<0||rig.wrist[h]<0||
        rig.parent[rig.elbow[h]]!=rig.shoulder[h]||rig.parent[rig.wrist[h]]!=rig.elbow[h])return 0;
    *out=rig;return 1;
}
int cvhands_retarget_scratch(const CvGameRig *rig,const CvPose *src,const CvTrackedPose wrist[2],CvGameHandsState *state,CvPose *out,CvGameHandsScratch *scratch){
    if(!rig||!src||!wrist||!state||!out||!scratch||!rig->count||rig->count>CV_RIG_MAX_BONES)return 0;
    CvPose *native=scratch->native,*result=scratch->result,*override=scratch->override;
    unsigned char *override_valid=scratch->override_valid;
    memset(override_valid,0,rig->count);
    CvGameHandsState next=*state;
    for(unsigned i=0;i<rig->count;i++){
        if(rig->parent[i]<-1||rig->parent[i]>=(int)i||!isfinite(src[i].position.x)||!isfinite(src[i].position.y)||!isfinite(src[i].position.z))return 0;
        native[i]=src[i];if(!unitq(src[i].orientation,&native[i].orientation))return 0;
    }
    for(int h=0;h<2;h++){
        if((wrist[h].valid&3)!=3){next.previous_valid[h]=0;continue;}
        int s=rig->shoulder[h],e=rig->elbow[h],w=rig->wrist[h];
        if(s<0||e<0||w<0||s>=(int)rig->count||e>=(int)rig->count||w>=(int)rig->count)return 0;
        CvVec3 upper=minus(native[e].position,native[s].position),lower=minus(native[w].position,native[e].position);
        cod2vr_arm_input input={.shoulder=native[s].position,.wrist_target=wrist[h].pose,
            .elbow_hint=native[e].position,.previous_elbow=state->previous_elbow[h],.previous_valid=(int)state->previous_valid[h],
            .upper_length=sqrt(inner(upper,upper)),.lower_length=sqrt(inner(lower,lower))};
        if(state->anatomical_mask&(1u<<h)){
            if(input.upper_length>12)input.upper_length=12;
            if(input.lower_length>10.4)input.lower_length=10.4;
        }
        /* Body-relative outward/downward pole avoids reproducing a bent
         * scripted/reload elbow pose at startup. History only damps the pole. */
        CvVec3 pole=rot(native[0].orientation,vv(.10*input.upper_length,(h==0?1:-1)*.85*input.upper_length,-.75*input.upper_length));
        input.elbow_hint=plus(input.shoulder,pole);
        if(input.previous_valid)input.elbow_hint=plus(scale(input.elbow_hint,.8),scale(input.previous_elbow,.2));
        cod2vr_arm_output solved;if(!cod2vr_solve_arm(&input,&solved))return 0;
        CvQuat upper_delta=between(upper,minus(solved.elbow,solved.shoulder)),lower_delta=between(lower,minus(solved.wrist.position,solved.elbow));
        CvQuat neutral_wrist=product(lower_delta,native[w].orientation);
        CvVec3 lower_axis=scale(minus(solved.wrist.position,solved.elbow),1/input.lower_length);
        if(state->wrist_limit_mask&(1u<<h))solved.wrist.orientation=limited_wrist(neutral_wrist,solved.wrist.orientation,lower_axis);
        override[s]=(CvPose){.position=solved.shoulder,.orientation=product(upper_delta,native[s].orientation)};
        override[e]=(CvPose){.position=solved.elbow,.orientation=product(lower_delta,native[e].orientation)};
        override[w]=solved.wrist;override_valid[s]=override_valid[e]=override_valid[w]=1;
        int t=rig->wrist_twist[h];
        if(t>=0&&t<(int)rig->count&&rig->parent[t]==e){
            CvVec3 offset=minus(native[t].position,native[e].position);
            double authored_lower=sqrt(inner(lower,lower));
            double fraction=sqrt(inner(offset,offset))/authored_lower;if(fraction<0)fraction=0;if(fraction>1)fraction=1;
            CvQuat twist=axial_twist(product(solved.wrist.orientation,conjugate(neutral_wrist)),lower_axis);
            CvQuat partial={(float)(twist.x*fraction),(float)(twist.y*fraction),(float)(twist.z*fraction),(float)(1-fraction+twist.w*fraction)};unitq(partial,&partial);
            override[t]=(CvPose){.position=plus(solved.elbow,rot(lower_delta,scale(offset,input.lower_length/authored_lower))),.orientation=product(partial,product(lower_delta,native[t].orientation))};override_valid[t]=1;
        }
        next.previous_elbow[h]=solved.elbow;next.previous_valid[h]=1;
    }
    for(unsigned i=0;i<rig->count;i++){
        int p=rig->parent[i];
        if(override_valid[i])result[i]=override[i];
        else if(p<0)result[i]=native[i];
        else{
            CvQuat correction=product(result[p].orientation,conjugate(native[p].orientation));
            result[i].position=plus(result[p].position,rot(correction,minus(native[i].position,native[p].position)));
            result[i].orientation=product(correction,native[i].orientation);
        }
        if(!unitq(result[i].orientation,&result[i].orientation))return 0;
    }
    memcpy(out,result,sizeof(CvPose)*rig->count);*state=next;return 1;
}
#ifndef CV_FREESTANDING
int cvhands_retarget(const CvGameRig *rig,const CvPose *src,const CvTrackedPose wrist[2],CvGameHandsState *state,CvPose *out){
    CvGameHandsScratch scratch;
    return cvhands_retarget_scratch(rig,src,wrist,state,out,&scratch);
}
#endif
