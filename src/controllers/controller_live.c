/* Original opt-in adapter for the game's own FPS arm/finger skeleton.
 * No replacement assets. This runs only at our validated view-weapon callsite.
 * Mathematical helpers are original, tested modules in arms/game_hands.c. */
#include "game_hands.h"
#include "weapon.h"
#include "../include/vr_input.h"
#include "hand_contact.h"
#include "cv_math.h"
typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned char U8;
typedef void *HANDLE;
#define DLL __declspec(dllimport)
DLL HANDLE __stdcall GetModuleHandleA(const char *);
DLL void * __stdcall GetProcAddress(HANDLE,const char *);
DLL HANDLE __stdcall CreateFileA(const char *,U32,U32,void *,U32,U32,HANDLE);
DLL int __stdcall ReadFile(HANDLE,void *,U32,U32 *,void *);
DLL int __stdcall CloseHandle(HANDLE);
DLL U32 __stdcall VirtualQuery(const void *,void *,U32);
DLL HANDLE __stdcall CreateFileMappingA(HANDLE,void *,U32,U32,U32,const char *);
DLL void * __stdcall MapViewOfFile(HANDLE,U32,U32,U32,U32);
typedef struct {U32 base,allocation,allocation_protect,size,state,protect,type;} MBI;
typedef int (__cdecl *Snapshot)(CvFrame *,CvPose *,float *);
extern void controller_probe_note(const char *);
extern void controller_probe_contact_report(unsigned,unsigned,float,float,float,float,float);
__declspec(dllexport) volatile CvHandContactDebug cod2vr_hand_contact;
int _fltused=0;
void *memset(void *p,int value,unsigned int n){U8 *b=p;while(n--)*b++=(U8)value;return p;}
void *memcpy(void *p,const void *src,unsigned int n){U8 *d=p;const U8 *s=src;while(n--)*d++=*s++;return p;}
int strcmp(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return (U8)*a-(U8)*b;}
static CvGameRig rig;
static CvGameHandsScratch scratch;
static CvGameHandsState state;
static CvPose native[CV_RIG_MAX_BONES],body_native[CV_RIG_MAX_BONES],output[CV_RIG_MAX_BONES],calibration[2],anchor;
static CvTrackedPose targets[2],aim_targets[2];
static CvFrame frame,aim_frame,hand_frame;
static volatile CvInputTransport *input_map;
static CvButtonsFrame buttons;
static CvTwoHandState offhand;
static CvReloadAttachment reload_attachment;
static int was_reloading;
static float ref[22];
static const char *names[CV_RIG_MAX_BONES];
static int parents[CV_RIG_MAX_BONES];
static U8 computed[CV_RIG_MAX_BONES],affected[CV_RIG_MAX_BONES],hand_model[CV_RIG_MAX_BONES];
static U32 cached_dobj,cached_signature,calls;
static int enabled,busy,committed,missing_snapshot,calibrated;
static int muzzle_index=-1,weapon_committed;
static CvPose state_entity;
static int state_entity_valid;
static CvPose aim_world;
static unsigned long long aim_frame_sequence;
static long long aim_time_ns;
static volatile U32 aim_publish;
static int aim_valid;
static Snapshot snapshot;
static float distance(CvVec3 a,CvVec3 b){double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return (float)sqrt(x*x+y*y+z*z);}
static int read_buttons(void){
    if(!input_map){
        HANDLE file=CreateFileA("Z:\\home\\steamos\\cod2-vr-work\\transport\\input.bin",0x80000000,3,0,3,0x80,0);
        if(file==(HANDLE)-1)return 0;
        HANDLE mapping=CreateFileMappingA(file,0,2,0,sizeof(CvInputTransport),0);CloseHandle(file);
        if(!mapping)return 0;
        input_map=(volatile CvInputTransport *)MapViewOfFile(mapping,4,0,0,sizeof(CvInputTransport));CloseHandle(mapping);
        if(!input_map)return 0;
    }
    for(int n=0;n<3;n++){
        U32 sequence=input_map->sequence;if(sequence&1)continue;__atomic_signal_fence(__ATOMIC_SEQ_CST);
        if(input_map->magic!=CV_INPUT_MAGIC||input_map->version!=CV_INPUT_VERSION)return 0;
        memcpy(&buttons,(const void *)&input_map->frame,sizeof buttons);__atomic_signal_fence(__ATOMIC_SEQ_CST);
        if(sequence==input_map->sequence){
            long long age=frame.predicted_display_time_ns-buttons.predicted_time_ns;
            return buttons.version==CV_INPUT_VERSION&&buttons.size==sizeof buttons&&age<250000000&&age> -250000000;
        }
    }
    return 0;
}
static int readable(U32 address,U32 size){
    if(!address||address+size<address)return 0;
    U32 end=address+size;
    while(address<end){MBI m;if(VirtualQuery((void *)address,&m,sizeof m)!=sizeof m||m.state!=0x1000||(m.protect&0x101)||m.base+m.size<=address)return 0;address=m.base+m.size;}
    return 1;
}
static int writable(U32 address,U32 size){
    MBI m;if(!readable(address,size)||VirtualQuery((void *)address,&m,sizeof m)!=sizeof m)return 0;
    return (m.protect&(0x04|0x08|0x40|0x80))!=0 && address+size<=m.base+m.size;
}
static int cstring(U32 ptr){for(U32 i=0;i<128;i++){if(!readable(ptr+i,1))return 0;if(!*(U8 *)(ptr+i))return 1;}return 0;}
static int prefix(const char *s,const char *p){while(*p)if(*s++!=*p++)return 0;return 1;}
static U32 rig_signature(U32 dobj){
    if(!readable(dobj,0x44))return 0;
    unsigned models=*(U8 *)(dobj+0x18);if(!models||models>8)return 0;
    U32 hash=*(U8 *)(dobj+0x19);
    for(unsigned i=0;i<models;i++)hash=(hash*33)^*(U32 *)(dobj+0x1c+4*i);
    return hash?hash:1;
}
static int bind_rig(U32 dobj){
    if(!readable(dobj,0x44))return 0;
    unsigned model_count=*(U8 *)(dobj+0x18),count=*(U8 *)(dobj+0x19),offset=0;
    U32 table=*(U32 *)0x8aed00;
    if(!model_count||model_count>8||count<16||count>128)return 0;
    int original_hands=0;int flash=-1;
    for(unsigned m=0;m<model_count;m++){
        U32 model=*(U32 *)(dobj+0x1c+4*m);
        if(!readable(model,0x8c))return 0;
        U32 parts=*(U32 *)model,name=*(U32 *)(model+0x88);
        if(!cstring(name)||!readable(parts,0x10))return 0;
        int is_hands=prefix((const char *)name,"viewmodel_hands");
        if(is_hands)original_hands=1;
        unsigned total=*(U16 *)parts,roots=*(U16 *)(parts+2);
        U32 info=*(U32 *)(parts+4);
        if(!total||!roots||roots>total||offset+total>count||!readable(info,4+total-roots))return 0;
        U32 handles=*(U32 *)info;
        if(!readable(handles,total*2))return 0;
        unsigned root_parent=*(U8 *)(dobj+0x3c+m);
        for(unsigned b=0;b<total;b++){
            unsigned index=offset+b,handle=*(U16 *)(handles+2*b);
            U32 bone_name=table+8*handle+4;
            if(!cstring(bone_name))return 0;
            names[index]=(const char *)bone_name;
            hand_model[index]=(U8)is_hands;
            if(!is_hands&&!strcmp(names[index],"tag_flash"))flash=(int)index;
            parents[index]=b<roots?(root_parent==255?-1:(int)root_parent):(int)index-*(U8 *)(info+4+b-roots);
        }
        offset+=total;
    }
    if(!original_hands||offset!=count||!cvhands_bind(&rig,count,names,parents))return 0;
    cached_dobj=dobj;cached_signature=rig_signature(dobj);muzzle_index=flash;
    memset(&reload_attachment,0,sizeof reload_attachment);was_reloading=0;
    memset(&state,0,sizeof state);state_entity_valid=0;calibrated=0;committed=0;weapon_committed=0;offhand.latched=0;
    for(int h=0;h<2;h++)calibration[h]=(CvPose){.orientation={0,0,0,1},.position={0,0,0}};
    /* Game left wrist +X follows fingers, +Y thumbward, +Z dorsal. A fixed
     * palm reference avoids baking an arbitrary startup hand/controller bend. */
    calibration[CV_HAND_LEFT].orientation=(CvQuat){.5f,-.5f,-.5f,-.5f};
    for(unsigned i=0;i<count;i++){
        affected[i]=parents[i]>=0?affected[parents[i]]:0;
        for(int h=0;h<2;h++)if((int)i==rig.shoulder[h])affected[i]|=(U8)(1<<h);
    }
    controller_probe_note("HANDS: original FPS rig bound; animated lengths required");return 1;
}
static CvQuat product(CvQuat a,CvQuat b){return (CvQuat){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
static CvVec3 rotate(CvQuat q,CvVec3 v){CvVec3 t={2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};return (CvVec3){v.x+q.w*t.x+q.y*t.z-q.z*t.y,v.y+q.w*t.y+q.z*t.x-q.x*t.z,v.z+q.w*t.z+q.x*t.y-q.y*t.x};}
static int normalize(CvQuat *q){double n=(double)q->x*q->x+(double)q->y*q->y+(double)q->z*q->z+(double)q->w*q->w;if(!isfinite(n)||n<1e-12)return 0;n=1/sqrt(n);q->x=(float)(q->x*n);q->y=(float)(q->y*n);q->z=(float)(q->z*n);q->w=(float)(q->w*n);return 1;}
static void check_mode(void){
    if(calls++%60)return;
    HANDLE f=CreateFileA("cod2vr-hands.txt",0x80000000,3,0,3,0x80,0);
    char ch=0;U32 got=0;
    if(f!=(HANDLE)-1){ReadFile(f,&ch,1,&got,0);CloseHandle(f);}
    int value=got&&ch=='1';
    if(value!=enabled){enabled=value;calibrated=0;state_entity_valid=0;offhand.latched=0;memset(&state,0,sizeof state);controller_probe_note(enabled?"HANDS: opt-in enabled":"HANDS: opt-in disabled; native animation retained");}
}
void __cdecl controller_live_hands(void *entity,void *playerstate){
    check_mode();if(busy)return;
    aim_publish++;__atomic_signal_fence(__ATOMIC_SEQ_CST);aim_valid=0;__atomic_signal_fence(__ATOMIC_SEQ_CST);aim_publish++;
    if(!enabled)return;busy=1;
    U32 ent=(U32)entity,ps=(U32)playerstate;
    if(!readable(ent,0x48)||!readable(ps,0xb8))goto done;
    U32 weapon=*(U32 *)(ps+0xb0);if(!weapon||weapon>=128)goto done;
    U32 dobj=*(U32 *)(0xf48d80+weapon*0x1b8);
    if(!readable(dobj,0x44))goto done;
    if(dobj!=cached_dobj||rig_signature(dobj)!=cached_signature)if(!bind_rig(dobj))goto done;
    U32 cache=*(U32 *)(dobj+4),array=cache+0x30;
    if(!readable(cache,0x30)||!writable(array,rig.count*32))goto done;
    for(unsigned i=0;i<rig.count;i++){
        const float *bone=(const float *)(array+32*i);
        native[i].orientation=(CvQuat){bone[0],bone[1],bone[2],bone[3]};
        native[i].position=(CvVec3){bone[4],bone[5],bone[6]};
        U32 mask=*(U32 *)(cache+0x20+4*(i>>5));
        computed[i]=(U8)((mask&(1u<<(i&31)))&&bone[7]>0&&normalize(&native[i].orientation));
        if(!computed[i])native[i].orientation=(CvQuat){0,0,0,1};
    }
    for(int h=0;h<2;h++)if(!computed[rig.shoulder[h]]||!computed[rig.elbow[h]]||!computed[rig.wrist[h]])goto done;
    if(!snapshot){HANDLE renderer=GetModuleHandleA("gfx_d3d_x86_s.dll");if(renderer)snapshot=(Snapshot)GetProcAddress(renderer,"cod2vr_tracking_snapshot");}
    if(!snapshot){if(!missing_snapshot){missing_snapshot=1;controller_probe_note("HANDS: shared stereo tracking export unavailable");}goto done;}
    if(!snapshot(&frame,&anchor,ref))goto done;
    hand_frame=frame;
    if((frame.aim[CV_HAND_LEFT].valid&3)==3)hand_frame.grip[CV_HAND_LEFT].pose.orientation=frame.aim[CV_HAND_LEFT].pose.orientation;
    CvVec3 origin=*(const CvVec3 *)(ent+0x3c);
    if(!cvhands_tracking_to_model(&hand_frame,&anchor,ref,(const float *)(ent+0x14),origin,40,calibration,targets))goto done;
    for(int h=CV_HAND_RIGHT;h<2;h++)if(!(calibrated&(1<<h))&&targets[h].valid==3){
        CvQuat inv=targets[h].pose.orientation;inv.x=-inv.x;inv.y=-inv.y;inv.z=-inv.z;
        calibration[h].orientation=product(inv,native[rig.wrist[h]].orientation);
        if(!normalize(&calibration[h].orientation))goto done;
        calibrated|=1<<h;controller_probe_note(h?"HANDS: right wrist rotation calibrated":"HANDS: left wrist rotation calibrated");
    }
    if(!cvhands_tracking_to_model(&hand_frame,&anchor,ref,(const float *)(ent+0x14),origin,40,calibration,targets))goto done;
    if(!targets[0].valid&&!targets[1].valid)goto done;
    CvPose original_entity,new_entity,delta;
    if(!cvhands_entity_pose((const float *)(ent+0x14),origin,&original_entity))goto done;
    int drive_weapon=0,reloading=0;
    CvPose attachment_wrist=native[rig.wrist[CV_HAND_RIGHT]],attachment_muzzle;
    if(muzzle_index>=0&&computed[muzzle_index]){
        reloading=cvweapon_reload_reference(&reload_attachment,*(U32 *)(ps+0xb4),
            attachment_wrist,native[muzzle_index],&attachment_wrist,&attachment_muzzle);
    }
    if(reloading!=was_reloading){
        controller_probe_note(reloading?"RELOAD: native animated hands released from controller locks; stable gun attachment":"RELOAD: animation complete; controller hands resumed");
        was_reloading=reloading;offhand.latched=0;
        memset(&state,0,sizeof state);state_entity_valid=0;
    }
    int was_latched=offhand.latched;
    memcpy(body_native,native,rig.count*sizeof(CvPose));
    /* FPS animation shoulders are authored for a flat camera, not the user's
     * torso. Locate shoulders 20 cm below the tracked head, 18 cm either side,
     * with yaw only: looking up/down must not swing the torso around the gun. */
    CvFrame body_frame=frame;
    CvQuat body_yaw={0,frame.head.pose.orientation.y,0,frame.head.pose.orientation.w};
    if(!normalize(&body_yaw))body_yaw=anchor.orientation;
    const CvPose identity_cal[2]={{{0,0,0,1},{0,0,0}},{{0,0,0,1},{0,0,0}}};
    CvTrackedPose shoulders[2]={{0},{0}};
    for(int h=0;h<2;h++){
        CvVec3 offset=rotate(body_yaw,(CvVec3){h==0?-.18f:.18f,-.20f,.04f});
        body_frame.grip[h].pose=(CvPose){body_yaw,{frame.head.pose.position.x+offset.x,frame.head.pose.position.y+offset.y,frame.head.pose.position.z+offset.z}};
        body_frame.grip[h].valid=frame.head.valid;
    }
    if(cvhands_tracking_to_model(&body_frame,&anchor,ref,(const float *)(ent+0x14),origin,40,identity_cal,shoulders)){
        for(int h=0;h<2;h++)if(shoulders[h].valid==3){
            CvVec3 old=native[rig.shoulder[h]].position;
            CvVec3 shift={shoulders[h].pose.position.x-old.x,shoulders[h].pose.position.y-old.y,shoulders[h].pose.position.z-old.z};
            for(unsigned i=0;i<rig.count;i++)if(affected[i]&(1u<<h)){
                body_native[i].position.x+=shift.x;body_native[i].position.y+=shift.y;body_native[i].position.z+=shift.z;
            }
        }
    }

    /* Constrain the gun attachment as well as the visible hands. Otherwise
     * fixed-length IK would leave a floating rifle beyond the real arm reach. */
    for(int h=0;h<2;h++)if(targets[h].valid==3&&shoulders[h].valid==3){
        CvVec3 limited;
        if(cvhands_clamp_wrist(&rig,native,shoulders[h].pose.position,(unsigned)h,targets[h].pose.position,&limited))targets[h].pose.position=limited;
    }

    if(targets[CV_HAND_RIGHT].valid==3&&muzzle_index>=0&&computed[muzzle_index]&&
       (frame.aim[CV_HAND_RIGHT].valid&3)==3&&writable(ent+0x14,0x34)){
        aim_frame=frame;aim_frame.grip[0]=frame.aim[0];aim_frame.grip[1]=frame.aim[1];
        /* Native gun tag_flash +X is forward. OpenXR aim -Z is forward.
         * This fixed basis maps native muzzle axes into an aim action's axes. */
        const CvPose aim_cal[2]={{{.5f,-.5f,-.5f,-.5f},{0,0,0}},{{.5f,-.5f,-.5f,-.5f},{0,0,0}}};
        if(cvhands_tracking_to_model(&aim_frame,&anchor,ref,(const float *)(ent+0x14),origin,40,aim_cal,aim_targets)&&
           aim_targets[CV_HAND_RIGHT].valid==3&&
           cvweapon_attach(&attachment_wrist,&attachment_muzzle,targets[CV_HAND_RIGHT].pose.position,aim_targets[CV_HAND_RIGHT].pose.orientation,&delta)){
            int have_input=read_buttons();
            CvPose locked_left,stabilized;
            int active=!reloading&&have_input&&(buttons.active_mask&CV_INPUT_LEFT_ACTIVE)&&targets[CV_HAND_LEFT].valid==3;
            float squeeze=have_input?buttons.grip[CV_HAND_LEFT]:0;
            CvPose single_delta=delta;
            if(cvweapon_twohand(&native[rig.wrist[CV_HAND_RIGHT]],&native[rig.wrist[CV_HAND_LEFT]],targets[CV_HAND_RIGHT].pose.position,targets[CV_HAND_LEFT].pose.position,squeeze,active,40,delta,&offhand,&stabilized,&locked_left))delta=stabilized;
            else offhand.latched=0;
            if(offhand.latched){
                CvVec3 shoulder=body_native[rig.shoulder[CV_HAND_LEFT]].position;
                CvVec3 limited;
                /* Move the virtual shoulder only as far as needed to keep the
                 * authored grip reachable with the capped arm lengths. */
                if(cvhands_clamp_wrist(&rig,native,shoulder,CV_HAND_LEFT,locked_left.position,&limited)){
                    CvVec3 shift={locked_left.position.x-limited.x,locked_left.position.y-limited.y,locked_left.position.z-limited.z};
                    for(unsigned i=0;i<rig.count;i++)if(affected[i]&(1u<<CV_HAND_LEFT)){
                        body_native[i].position.x+=shift.x;
                        body_native[i].position.y+=shift.y;
                        body_native[i].position.z+=shift.z;
                    }
                    targets[CV_HAND_LEFT].pose=locked_left;targets[CV_HAND_LEFT].valid=3;
                }else{
                    offhand.latched=0;delta=single_delta;
                }
            }
            new_entity=cvweapon_compose(original_entity,delta);
            CvPose inverse_delta=cvweapon_inverse(delta);
            targets[CV_HAND_RIGHT].pose.orientation=cvweapon_compose(delta,native[rig.wrist[CV_HAND_RIGHT]]).orientation;
            for(unsigned i=0;i<rig.count;i++)body_native[i]=cvweapon_compose(inverse_delta,body_native[i]);
            for(int h=0;h<2;h++)if(targets[h].valid==3)targets[h].pose=cvweapon_compose(inverse_delta,targets[h].pose);
            drive_weapon=1;
        }
    }
    if(!drive_weapon)offhand.latched=0;
    if(offhand.latched!=was_latched)controller_probe_note(offhand.latched?"TWOHAND: left squeeze acquired original foregrip; dual-controller direction active":"TWOHAND: left foregrip released; independent hand tracking active");
    /* Native reload wrist/finger motion must survive the IK pass. The gun
     * still follows the tracked right controller through its stable reference. */
    if(reloading&&drive_weapon)for(int h=0;h<2;h++){
        targets[h].pose=native[rig.wrist[h]];targets[h].valid=3;
    }
    state.anatomical_mask=3;
    state.wrist_limit_mask=(offhand.latched||reloading)?0u:1u;
    state.contact_lock_mask=(drive_weapon?2u:0u)|((offhand.latched||reloading)?1u:0u);
    CvPose active_entity=drive_weapon?new_entity:original_entity;
    if(state_entity_valid){
        CvPose previous_to_current=cvweapon_compose(cvweapon_inverse(active_entity),state_entity);
        for(int h=0;h<2;h++)if(state.previous_valid[h])state.previous_elbow[h]=cvweapon_compose(previous_to_current,(CvPose){.orientation={0,0,0,1},.position=state.previous_elbow[h]}).position;
    }
    if(!cvhands_retarget_scratch(&rig,body_native,targets,&state,output,&scratch)){state_entity_valid=0;memset(&state,0,sizeof state);goto done;}
    state_entity=active_entity;state_entity_valid=1;
    unsigned active=(targets[0].valid==3?1u:0u)|(targets[1].valid==3?2u:0u),writes=0;
    float contact_error[2],shoulder_shift[2];
    cod2vr_hand_contact.publish_sequence++;__atomic_signal_fence(__ATOMIC_SEQ_CST);
    cod2vr_hand_contact.magic=0x43485652;cod2vr_hand_contact.version=1;cod2vr_hand_contact.size=sizeof(CvHandContactDebug);
    cod2vr_hand_contact.frame_sequence=frame.sequence;cod2vr_hand_contact.held=(unsigned)offhand.latched;cod2vr_hand_contact.active_mask=active;
    cod2vr_hand_contact.left_squeeze=buttons.grip[CV_HAND_LEFT];
    for(int h=0;h<2;h++){
        contact_error[h]=distance(output[rig.wrist[h]].position,targets[h].pose.position);
        shoulder_shift[h]=distance(output[rig.shoulder[h]].position,body_native[rig.shoulder[h]].position);
        cod2vr_hand_contact.error[h]=contact_error[h];cod2vr_hand_contact.shoulder_shift[h]=shoulder_shift[h];
        cod2vr_hand_contact.target[h]=targets[h].pose;cod2vr_hand_contact.solved[h]=output[rig.wrist[h]];
        cod2vr_hand_contact.native_shoulder[h]=body_native[rig.shoulder[h]].position;cod2vr_hand_contact.solved_shoulder[h]=output[rig.shoulder[h]].position;
    }
    __atomic_signal_fence(__ATOMIC_SEQ_CST);cod2vr_hand_contact.publish_sequence++;
    controller_probe_contact_report(active,(unsigned)offhand.latched,buttons.grip[CV_HAND_LEFT],contact_error[0],contact_error[1],shoulder_shift[0],shoulder_shift[1]);
    for(unsigned i=0;i<rig.count;i++)if(computed[i]&&hand_model[i]&&(drive_weapon||(affected[i]&active))){
        float *bone=(float *)(array+32*i);
        bone[0]=output[i].orientation.x;bone[1]=output[i].orientation.y;bone[2]=output[i].orientation.z;bone[3]=output[i].orientation.w;
        bone[4]=output[i].position.x;bone[5]=output[i].position.y;bone[6]=output[i].position.z;bone[7]=2;writes++;
    }
    if(drive_weapon){
        CvVec3 basis[3]={rotate(new_entity.orientation,(CvVec3){1,0,0}),rotate(new_entity.orientation,(CvVec3){0,1,0}),rotate(new_entity.orientation,(CvVec3){0,0,1})};
        memcpy((void *)(ent+0x14),basis,sizeof basis);memcpy((void *)(ent+0x3c),&new_entity.position,sizeof new_entity.position);
        aim_publish++;__atomic_signal_fence(__ATOMIC_SEQ_CST);
        aim_world=cvweapon_compose(new_entity,native[muzzle_index]);aim_frame_sequence=frame.sequence;aim_time_ns=frame.predicted_display_time_ns;aim_valid=1;
        __atomic_signal_fence(__ATOMIC_SEQ_CST);aim_publish++;
        if(!weapon_committed){weapon_committed=1;controller_probe_note("WEAPON: original entity root follows right grip position and absolute aim rotation; original body-space arms retained");}
    }
    if(writes&&!committed){committed=1;controller_probe_note("HANDS: committed original game arm/finger transforms from 6DoF tracking");}
done:
    busy=0;
}
/* Shared actual visible barrel pose, in game-world coordinates. +X is forward.
 * Gameplay agent must reject old timestamps; absent tracking returns zero. */
__declspec(dllexport) int __cdecl controller_probe_aim_snapshot(CvPose *pose,unsigned long long *sequence,long long *time_ns){
    if(!pose||!sequence||!time_ns)return 0;
    for(int n=0;n<3;n++){
        U32 stamp=aim_publish;if(stamp&1)continue;__atomic_signal_fence(__ATOMIC_SEQ_CST);
        if(!aim_valid)return 0;
        *pose=aim_world;*sequence=aim_frame_sequence;*time_ns=aim_time_ns;__atomic_signal_fence(__ATOMIC_SEQ_CST);
        if(stamp==aim_publish)return 1;
    }
    return 0;
}
