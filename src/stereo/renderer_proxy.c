/* Original CoD2 renderer interception experiment. Exact 1.3 renderer only.
 * Pass-through/logging by default. CV_STEREO_SBS=1 enables backend replay.
 * No other mod implementation is included. */
typedef unsigned char u8;typedef unsigned int u32;typedef int i32;
typedef void *Handle;
#include "../include/vr_transport.h"
#include "../include/vr_hud.h"
#include "../include/vr_scope.h"
#include "../include/vr_scope_math.h"
#define WIN __declspec(dllimport)
WIN Handle __stdcall LoadLibraryA(const char *);
WIN void * __stdcall GetProcAddress(Handle,const char *);
WIN u32 __stdcall GetModuleFileNameA(Handle,char *,u32);
WIN u32 __stdcall GetEnvironmentVariableA(const char *,char *,u32);
WIN Handle __stdcall CreateFileA(const char *,u32,u32,void *,u32,u32,Handle);
WIN int __stdcall WriteFile(Handle,const void *,u32,u32 *,void *);
WIN int __stdcall ReadFile(Handle,void *,u32,u32 *,void *);
WIN int __stdcall CloseHandle(Handle);
WIN int __stdcall FlushFileBuffers(Handle);
WIN int __stdcall VirtualProtect(void *,u32,u32,u32 *);
WIN Handle __stdcall CreateFileMappingA(Handle,void *,u32,u32,u32,const char *);
WIN void * __stdcall MapViewOfFile(Handle,u32,u32,u32,u32);
int _fltused;
#include "freestanding_memory.h"
typedef void (__cdecl *VoidFn)(void);
typedef void (__cdecl *RenderFn)(const void *);
typedef void (__cdecl *CmdFn)(void *);
typedef void *(__cdecl *GetApiFn)(i32,void *);
static Handle self_module,original,log_file;
static u8 *base;
static void *exports[108];
static RenderFn real_render;
static VoidFn real_begin,real_end;
static void (__cdecl *real_shutdown)(i32);
static CmdFn real_view,real_viewport;
static u32 render_count,frame_count,replay_count;
static u32 marker_start_count,marker_end_count;
static int controller_probe_attempted;
static int input_attempted;
static void (__cdecl *input_tick)(void);
static int sbs_enabled,hook_ready,eye=-1,world_seen,have_world;
static void *setup_view;
static void *left_surface,*right_surface;
static void *readback_surface;
static void *hud_gpu,*hud_before_surface,*hud_after_surface;
static CvHudTransport *hud_map;
static CvScopeTransport *scope_map;
static void *scope_gpu,*scope_readback;
static int scope_active,scope_captured;
static float scope_fov;
static u32 scope_frames;
static u8 scope_ref[0x58];
typedef int (__cdecl *ScopeAim)(CvPose *,unsigned long long *,long long *);
static ScopeAim scope_aim;
static u32 hud_before_frame,hud_prepared,hud_published;
static CvImageTransport *image_map;
static volatile CvPoseTransport *pose_map;
static CvFrame tracking;
static CvPose tracking_anchor;
static int anchor_valid,tracking_valid,ipc_enabled;
static volatile u32 tracking_publish;
static u32 surface_width,surface_height;
static u8 world_ref[0x58],eye_ref[0x58],frontend_ref[0x58];
static void *world_view;
static u8 saved_world_view[0x14c];
static u8 *replay_cursor;
static u32 replay_depth,replay_stack[32];
static const u32 START=0x43565331,FINISH=0x43565332;
static void cmd_view(void *context);
static void cmd_viewport(void *context);
static void log_text(const char *s){if(!log_file)return;u32 n=0,w;while(s[n])n++;WriteFile(log_file,s,n,&w,0);FlushFileBuffers(log_file);}
static void log_hex(const char *name,u32 v){char text[128];u32 n=0;while(*name&&n<100)text[n++]=*name++;text[n++]='=';for(int s=28;s>=0;s-=4)text[n++]="0123456789abcdef"[(v>>s)&15];text[n++]='\n';text[n]=0;log_text(text);}
static int replace(void **slot,void *value){u32 old;if(!VirtualProtect(slot,4,0x40,&old))return 0;*slot=value;u32 dummy;VirtualProtect(slot,4,old,&dummy);return 1;}
static void release_surfaces(void){
 void **surfaces_to_release[8]={&left_surface,&right_surface,&readback_surface,&hud_gpu,&hud_before_surface,&hud_after_surface,&scope_gpu,&scope_readback};
 for(int i=0;i<8;i++){void *s=*surfaces_to_release[i];if(s)((u32 (__stdcall *)(void *))(*(void ***)s)[2])(s);*surfaces_to_release[i]=0;}
 hud_prepared=0;
 surface_width=surface_height=0;
}
static void reset_owned_state(void){
 release_surfaces();
 if(base&&hook_ready){void **handlers=(void **)(base+0x190518);
  if(handlers[12]==cmd_view)replace(&handlers[12],base+0x32640);
  if(handlers[13]==cmd_viewport)replace(&handlers[13],base+0x32750);
 }
 hook_ready=0;anchor_valid=tracking_valid=0;eye=-1;world_view=0;world_seen=have_world=0;
 frame_count=render_count=replay_count=marker_start_count=marker_end_count=0;
}
static void shutdown_hook(i32 destroy_window){
 log_hex("Renderer shutdown",destroy_window);reset_owned_state();real_shutdown(destroy_window);
}
static void emit(i32 x,i32 y,i32 w,i32 h){((void (__cdecl *)(i32,i32,i32,i32))(base+0x22050))(x,y,w,h);}
static void fence(void){__asm {mfence}}
static void *map_file(const char *path,u32 bytes){
 Handle f=CreateFileA(path,0xc0000000,3,0,4,0x80,0);if(f==(void *)-1)return 0;
 Handle m=CreateFileMappingA(f,0,4,0,bytes,0);CloseHandle(f);if(!m)return 0;
 void *p=MapViewOfFile(m,6,0,0,bytes);CloseHandle(m);return p;
}
static void transport_init(void){
 if(!ipc_enabled)return;
 if(!image_map)image_map=map_file("Z:\\home\\steamos\\cod2-vr-work\\transport\\frame.bin",CV_IMAGE_MAP_BYTES);
 if(!pose_map)pose_map=map_file("Z:\\home\\steamos\\cod2-vr-work\\transport\\pose.bin",sizeof(CvPoseTransport));
}
static float root_float(float value){
 float result;
 __asm {
  fld value
  fsqrt
  fstp result
 }
 return result;
}
static CvQuat yaw_anchor(CvQuat q){
 /* Anchor only heading. Startup head roll/pitch must never tilt the world. */
 float x=-2.f*(q.x*q.z+q.w*q.y),z=-(1.f-2.f*(q.x*q.x+q.y*q.y));
 float horizontal=root_float(x*x+z*z);if(horizontal<.0001f)return (CvQuat){0,0,0,1};
 x/=horizontal;z/=horizontal;
 if(z>.99999f)return (CvQuat){0,1,0,0};
 float w=1.f-z,length=root_float(x*x+w*w);
 return (CvQuat){0,-x/length,0,w/length};
}
static void read_tracking(void){
 tracking_valid=0;transport_init();if(!pose_map)return;
 for(int retry=0;retry<3;retry++){
  u32 before=pose_map->sequence;if(before&1)continue;fence();
  CvFrame candidate;memcpy(&candidate,(const void *)&pose_map->frame,sizeof(candidate));fence();
  if(before!=pose_map->sequence)continue;
  if(pose_map->magic!=CV_POSE_MAGIC||candidate.version!=CV_FRAME_VERSION||candidate.size!=sizeof(candidate))return;
  if((candidate.head.valid&3)!=3||(candidate.eyes[0].valid&3)!=3||(candidate.eyes[1].valid&3)!=3)return;
  tracking=candidate;tracking_valid=1;
  if(!anchor_valid){tracking_anchor=tracking.head.pose;tracking_anchor.orientation=yaw_anchor(tracking_anchor.orientation);anchor_valid=1;log_text("Tracking origin anchored (yaw only; level horizon)\n");}
  return;
 }
}
static CvVec3 rotate(CvQuat q,CvVec3 v){
 CvVec3 t={2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};
 return (CvVec3){v.x+q.w*t.x+q.y*t.z-q.z*t.y,v.y+q.w*t.y+q.z*t.x-q.x*t.z,v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
static CvQuat product(CvQuat a,CvQuat b){return (CvQuat){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
static CvVec3 to_game(CvVec3 p,const float *r){return (CvVec3){-r[12]*p.x+r[15]*p.y-r[9]*p.z,-r[13]*p.x+r[16]*p.y-r[10]*p.z,-r[14]*p.x+r[17]*p.y-r[11]*p.z};}
static void tracked_ref(float *r,CvPose ep){
 CvQuat inv={-tracking_anchor.orientation.x,-tracking_anchor.orientation.y,-tracking_anchor.orientation.z,tracking_anchor.orientation.w};
 CvVec3 delta={ep.position.x-tracking_anchor.position.x,ep.position.y-tracking_anchor.position.y,ep.position.z-tracking_anchor.position.z};
 CvVec3 p=to_game(rotate(inv,delta),r);r[6]+=p.x*40;r[7]+=p.y*40;r[8]+=p.z*40;
 CvQuat relative=product(inv,ep.orientation);
 CvVec3 forward=to_game(rotate(relative,(CvVec3){0,0,-1}),r),left=to_game(rotate(relative,(CvVec3){-1,0,0}),r),up=to_game(rotate(relative,(CvVec3){0,1,0}),r);
 r[9]=forward.x;r[10]=forward.y;r[11]=forward.z;r[12]=left.x;r[13]=left.y;r[14]=left.z;r[15]=up.x;r[16]=up.y;r[17]=up.z;
}
static float tangent(float x){
 /* Argument range is runtime eye FOV (< pi/2); polynomial sin/cos ratio. */
 float x2=x*x;
 float s=x*(1+x2*(-1.f/6+x2*(1.f/120+x2*(-1.f/5040+x2/362880))));
 float c=1+x2*(-.5f+x2*(1.f/24+x2*(-1.f/720+x2*(1.f/40320-x2/3628800))));return s/c;
}
static int inverse4(const float *m,float *inverse){
 float a[4][8];
 for(int row=0;row<4;row++)for(int col=0;col<8;col++)a[row][col]=col<4?m[row*4+col]:(col-4==row?1.f:0.f);
 for(int col=0;col<4;col++){
  int pivot=col;float biggest=a[col][col];if(biggest<0)biggest=-biggest;
  for(int row=col+1;row<4;row++){float value=a[row][col];if(value<0)value=-value;if(value>biggest){biggest=value;pivot=row;}}
  if(biggest<1e-12f)return 0;
  if(pivot!=col)for(int k=0;k<8;k++){float temp=a[col][k];a[col][k]=a[pivot][k];a[pivot][k]=temp;}
  float divisor=a[col][col];for(int k=0;k<8;k++)a[col][k]/=divisor;
  for(int row=0;row<4;row++)if(row!=col){float factor=a[row][col];for(int k=0;k<8;k++)a[row][k]-=factor*a[col][k];}
 }
 for(int row=0;row<4;row++)for(int col=0;col<4;col++)inverse[row*4+col]=a[row][col+4];return 1;
}
static void eye_projection(void *view){
 if(!tracking_valid)return;float *p=(float *)((u8 *)view+0x88);
 if(eye==2){p[0]=p[5]=1/tangent(scope_fov*.00872664626f);p[8]=p[9]=0;}
 else{CvFov f=tracking.fov[eye];
 float l=tangent(f.angle_left),r=tangent(f.angle_right),u=tangent(f.angle_up),d=tangent(f.angle_down);
 if(r-l<.1f||u-d<.1f)return;
 p[0]=2/(r-l);p[5]=2/(u-d);p[8]=-(r+l)/(r-l);p[9]=-(u+d)/(u-d);}
 float result[16],*v=(float *)((u8 *)view+0x48);
 for(int row=0;row<4;row++)for(int col=0;col<4;col++){float value=0;for(int k=0;k<4;k++)value+=v[row*4+k]*p[k*4+col];result[row*4+col]=value;}
 memcpy((u8 *)view+0xc8,result,sizeof(result));
 inverse4(result,(float *)((u8 *)view+0x108));
}
/* Locally verified custom renderer convention: EBX=viewParms, stack=refdef. */
__declspec(naked) static void rebuild(void *view,const void *ref){
 __asm {
  push ebx
  mov ebx,[esp+8]
  push dword ptr [esp+12]
  call dword ptr [setup_view]
  add esp,4
  pop ebx
  ret
 }
}
static int surfaces(void){
 void *device=*(void **)(base+0x1d1bf8);if(!device)return 0;
 u32 *target=(u32 *)(base+0x1d4820);u32 w=target[3],h=target[4];
 if(marker_start_count<4){log_hex("D3D.device",(u32)device);for(u32 i=0;i<10;i++)log_hex("RT0.word",target[i]);}
 if(!w||!h||w>8192||h>8192)return 0;
 if(left_surface&&w==surface_width&&h==surface_height)return 1;
 release_surfaces();
 void **vt=*(void ***)device;
 typedef i32 (__stdcall *CreateRT)(void *,u32,u32,u32,u32,u32,i32,void **,void *);
 /* D3DFMT_A8R8G8B8=21; no multisampling; surface owns its COM reference. */
 i32 a=((CreateRT)vt[28])(device,w,h,21,0,0,0,&left_surface,0);
 i32 b=((CreateRT)vt[28])(device,w,h,21,0,0,0,&right_surface,0);
 log_hex("CreateRT.left",a);log_hex("CreateRT.right",b);
 if(a<0||b<0)return 0;surface_width=w;surface_height=h;return 1;
}
typedef struct {i32 left,top,right,bottom;} Rect;
static int blit(void *src,void *dst,Rect *rect){
 void *dev=*(void **)(base+0x1d1bf8);if(!dev||!src||!dst)return 0;
 typedef i32 (__stdcall *Stretch)(void *,void *,const Rect *,void *,const Rect *,u32);
 i32 result=((Stretch)(*(void ***)dev)[34])(dev,src,0,dst,rect,0);
 if(result<0&&replay_count<3)log_hex("StretchRect.error",result);return result>=0;
}
#include "hud_capture.h"
#include "scope_capture.h"
static int copy_pixels(void *surface,u32 hand,u32 slot){
 void *dev=*(void **)(base+0x1d1bf8);if(!dev||!image_map)return 0;void **vt=*(void ***)dev;
 if(!readback_surface){
  typedef i32 (__stdcall *CreateOffscreen)(void *,u32,u32,u32,u32,void **,void *);
  i32 r=((CreateOffscreen)vt[36])(dev,surface_width,surface_height,21,2,&readback_surface,0);
  if(r<0){log_hex("CreateOffscreen.error",r);return 0;}
 }
 typedef i32 (__stdcall *Readback)(void *,void *,void *);
 i32 r=((Readback)vt[32])(dev,surface,readback_surface);if(r<0){if(replay_count<4)log_hex("Readback.error",r);return 0;}
 typedef struct {i32 pitch;void *bits;} Locked;
 Locked lock;typedef i32 (__stdcall *Lock)(void *,Locked *,const Rect *,u32);
 r=((Lock)(*(void ***)readback_surface)[13])(readback_surface,&lock,0,0x10);
 if(r<0)return 0;
 u8 *pixels=(u8 *)image_map+64+slot*2u*CV_IMAGE_EYE_BYTES+hand*CV_IMAGE_EYE_BYTES;
 for(u32 row=0;row<surface_height;row++)memcpy(pixels+row*surface_width*4,(u8 *)lock.bits+row*lock.pitch,surface_width*4);
 ((i32 (__stdcall *)(void *))(*(void ***)readback_surface)[14])(readback_surface);return 1;
}
static void publish_images(void){
 if(!ipc_enabled||!image_map||surface_width>CV_MAX_IMAGE_WIDTH||surface_height>CV_MAX_IMAGE_HEIGHT)return;
 /* Never overwrite the most recently published image pair. The reader
  * validates the selected slot's generation, independently of newer frames. */
 u32 previous=image_map->reserved[4]==1?image_map->reserved[0]:2;
 u32 slot=(previous+1u)%3u;
 volatile u32 *slot_sequence=&image_map->reserved[1+slot];u32 generation=*slot_sequence;
 if(generation&1)generation++;
 *slot_sequence=generation+1;fence();
 int good=copy_pixels(left_surface,0,slot)&&copy_pixels(right_surface,1,slot);
 /* Image and tracking metadata are covered by the same slot generation.
  * The compositor must declare the pose used to render these pixels. */
 CvFrame *rendered=(CvFrame *)((u8 *)image_map+CV_IMAGE_POSE_OFFSET);
 if(tracking_valid)memcpy(&rendered[slot],&tracking,sizeof(CvFrame));
 else memset(&rendered[slot],0,sizeof(CvFrame));
 fence();*slot_sequence=generation+2;
 if(!good)return;
 image_map->magic=CV_IMAGE_MAGIC;image_map->version=CV_TRANSPORT_VERSION;
 image_map->width=surface_width;image_map->height=surface_height;image_map->stride=surface_width*4;
 image_map->eye_stride=CV_IMAGE_EYE_BYTES;image_map->pixel_format=CV_PIXEL_BGRA8;
 image_map->reserved[4]=1;
 image_map->reserved[5]=tracking_valid?1u:0u;
 fence();image_map->reserved[0]=slot;fence();
 volatile u32 *sequence=&image_map->sequence;u32 value=*sequence;
 value=(value&~1u)+2u;*sequence=value;
 if(replay_count<4){log_hex("IPC ring slot",slot);log_hex("IPC image sequence",value);}
}
static void cmd_view(void *context){
 u8 *cmd=*(u8 **)context;
 if(eye>=0&&have_world){
  void *view=*(void **)(cmd+0x14);
  /* Command 12 includes shadow and reflection views too. Modify only the
   * world view selected by a matching complete viewport and camera origin. */
  float *p=view,*r=(float *)(frontend_ref+0x18);
  if(view&&(view==world_view||(!world_view&&p[0]==r[0]&&p[1]==r[1]&&p[2]==r[2]&&((i32 *)view)[14]==((i32 *)world_ref)[2]))){
   if(!world_view){world_view=view;memcpy(saved_world_view,view,sizeof(saved_world_view));}
   memcpy(eye_ref,world_ref,sizeof(eye_ref));float *ref=(float *)eye_ref;
   if(eye==2)memcpy(eye_ref,scope_ref,sizeof(eye_ref));
   else if(tracking_valid)tracked_ref(ref,tracking.eyes[eye].pose);
   else{float distance=eye==0?1.28f:-1.28f;for(int i=0;i<3;i++)ref[6+i]+=ref[12+i]*distance;}
   rebuild(view,eye_ref);
   eye_projection(view);
  }
 }
 real_view(context);
}
static void cmd_viewport(void *context){
 u8 **cursor=context;u8 *command=*cursor;u32 tag=*(u32 *)(command+4);
 if(tag!=START&&tag!=FINISH){real_viewport(context);return;}
 if(*(u32 *)(base+0x70bfd0)||*(u32 *)(base+0x70bfe0))((VoidFn)(base+0x3ad10))();
 u32 *ctx=context;u32 bytes=*(unsigned short *)(command+2);
 if(tag==START){
  marker_start_count++;if(marker_start_count<4)log_hex("World BEGIN marker",marker_start_count);
  world_view=0;eye=surfaces()?0:-1;replay_cursor=command+bytes;replay_depth=ctx[1];
  if(replay_depth>32){eye=-1;log_text("Replay nesting too deep; skipping stereo\n");}
  else for(u32 i=0;i<replay_depth;i++)replay_stack[i]=ctx[2+i];
  *cursor=command+bytes;return;
 }
 marker_end_count++;if(marker_end_count<7)log_hex("World END marker",marker_end_count);
 void *target=*(void **)(base+0x1d4820+4);
 if(eye==0){
  blit(target,left_surface,0);eye=1;
  /* Restore the original world view before selecting a different eye. */
  *cursor=replay_cursor;ctx[1]=replay_depth;for(u32 i=0;i<replay_depth;i++)ctx[2+i]=replay_stack[i];
 }else{
  if(eye==1&&scope_active){
   blit(target,right_surface,0);eye=2;
   *cursor=replay_cursor;ctx[1]=replay_depth;for(u32 i=0;i<replay_depth;i++)ctx[2+i]=replay_stack[i];
   return;
  }
  if(eye==1||eye==2){
   if(eye==2)scope_capture(target);
   else blit(target,right_surface,0);
   publish_images();
   Rect left={0,0,(i32)surface_width/2,(i32)surface_height};
   Rect right={(i32)surface_width/2,0,(i32)surface_width,(i32)surface_height};
   blit(left_surface,target,&left);blit(right_surface,target,&right);replay_count++;
   hud_capture_before(target);
   if(replay_count<4)log_hex("Stereo replay frame",replay_count);
  }
  if(world_view){memcpy(world_view,saved_world_view,sizeof(saved_world_view));world_view=0;}
  eye=-1;*cursor=command+bytes;
 }
}
static void render_hook(const void *ref){
 render_count++;
 if(render_count<5){log_hex("RenderScene.ref",(u32)ref);for(u32 i=0;i<0x58;i+=4)log_hex("ref.word",*(u32 *)((u8 *)ref+i));}
 if(!sbs_enabled||!hook_ready||world_seen){real_render(ref);return;}
 const i32 *v=ref;
 if(v[2]<320||v[3]<200){real_render(ref);return;}
 world_seen=have_world=1;tracking_publish++;fence();memcpy(world_ref,ref,sizeof(world_ref));read_tracking();fence();tracking_publish++;
 scope_prepare((const float *)ref);
 memcpy(frontend_ref,ref,sizeof(frontend_ref));
 if(tracking_valid){
  tracked_ref((float *)frontend_ref,tracking.head.pose);
  /* Frontend culling must contain both runtime eye frusta while backend
   * receives precise asymmetric per-eye projection below. */
  ((float *)frontend_ref)[4]=130;((float *)frontend_ref)[5]=130;
 }
 emit(START,0,0,0);real_render(frontend_ref);emit(FINISH,0,0,0);
}
static int install_backend(void){
 if(!sbs_enabled||!base)return 0;
 void **handlers=(void **)(base+0x190518);
 if(handlers[12]==cmd_view&&handlers[13]==cmd_viewport){hook_ready=1;return 1;}
 if((handlers[12]!=base+0x32640&&handlers[12]!=cmd_view)||(handlers[13]!=base+0x32750&&handlers[13]!=cmd_viewport)){
  if(!hook_ready)log_text("Backend mismatch; stereo hooks deferred\n");return 0;
 }
 real_view=(CmdFn)(base+0x32640);real_viewport=(CmdFn)(base+0x32750);setup_view=base+0x259c0;
 if(replace(&handlers[12],cmd_view)&&replace(&handlers[13],cmd_viewport)){
  hook_ready=1;log_text("Stereo backend replay hooks active\n");return 1;
 }
 hook_ready=0;log_text("Backend table protection change failed\n");return 0;
}
static void begin_hook(void){
 frame_count++;world_seen=0;have_world=0;eye=-1;scope_active=scope_captured=0;
 /* Shutdown(false) can reset the renderer without a new GetRefAPI call.
  * Reattach only after checking the current canonical/owned handler pair. */
 if(sbs_enabled)install_backend();
 if(!controller_probe_attempted){controller_probe_attempted=1;Handle probe=LoadLibraryA("cod2vr_controller_probe.dll");
  if(probe){int (__cdecl *init)(void)=(int (__cdecl *)(void))GetProcAddress(probe,"controller_probe_init");if(init)log_hex("Controller probe installed",init());}
 }
 if(!input_attempted){input_attempted=1;Handle module=LoadLibraryA("cod2vr_input.dll");
  if(module){int (__cdecl *init)(void)=(int (__cdecl *)(void))GetProcAddress(module,"cod2vr_input_init");
   if(init&&init()){input_tick=(void (__cdecl *)(void))GetProcAddress(module,"cod2vr_input_tick");log_text("Gameplay input DLL initialized\n");}
  }
 }
 if(input_tick)input_tick();
 real_begin();
}
static void end_hook(void){real_end();hud_publish_after();if(!scope_active||!scope_captured)scope_off();if(frame_count<4)log_hex("EndFrame",frame_count);}
static void open_log(void){
 log_file=CreateFileA("cod2vr-original-renderer.log",0x40000000,3,0,2,0x80,0);
 if(log_file==(void *)-1)log_file=0;
 log_text("Original CoD2 renderer proxy entry\n");
}
__declspec(dllexport) void * __cdecl GetRefAPI(i32 version,void *imports){
 if(!log_file)open_log();
 if(!original){
  char path[1024];u32 n=GetModuleFileNameA(self_module,path,sizeof(path));
  if(!n||n>=sizeof(path)-40)return 0;
  while(n&&path[n-1]!='\\'&&path[n-1]!='/')n--;
  const char name[]="gfx_cod2vr_original.dll";for(u32 i=0;i<sizeof(name);i++)path[n+i]=name[i];
  original=LoadLibraryA(path);if(!original){log_text("Original renderer load failed\n");return 0;}base=original;
 }
 GetApiFn get=(GetApiFn)GetProcAddress(original,"GetRefAPI");if(!get)return 0;
 void **table=get(version,imports);if(!table)return 0;
 log_hex("GetRefAPI.version",version);log_hex("Original base",(u32)base);
 u32 pe=*(u32 *)(base+0x3c);
 if(*(u32 *)(base+pe+8)!=0x4456a390||*(u32 *)(base+pe+0x50)!=0x763000){log_text("Unknown renderer; pass-through only\n");return table;}
 for(int i=0;i<108;i++)exports[i]=table[i];
 real_render=(RenderFn)table[37];real_begin=(VoidFn)table[48];real_end=(VoidFn)table[49];real_shutdown=(void (__cdecl *)(i32))table[0];
 if((u8 *)real_shutdown!=base+0x131e0||(u8 *)real_render!=base+0x25bf0||(u8 *)real_begin!=base+0x223a0||(u8 *)real_end!=base+0x223f0){log_text("Slot mismatch; pass-through only\n");return table;}
 exports[0]=shutdown_hook;exports[37]=render_hook;exports[48]=begin_hook;exports[49]=end_hook;
 char mode[8];sbs_enabled=GetEnvironmentVariableA("CV_STEREO_SBS",mode,sizeof(mode))&&mode[0]=='1';
 ipc_enabled=GetEnvironmentVariableA("CV_STEREO_IPC",mode,sizeof(mode))&&mode[0]=='1';if(ipc_enabled)sbs_enabled=1;
 Handle modefile=CreateFileA("cod2vr-mode.txt",0x80000000,3,0,3,0x80,0);
 if(modefile!=(void *)-1){u32 got=0;char setting=0;ReadFile(modefile,&setting,1,&got,0);CloseHandle(modefile);
  if(got&&setting>='0'&&setting<='2'){sbs_enabled=setting!='0';ipc_enabled=setting=='2';log_hex("Mode file",setting-'0');}
 }
 if(sbs_enabled)install_backend();
 log_hex("SBS enabled",sbs_enabled);return exports;
}
int __stdcall DllMain(Handle module,u32 reason,void *reserved){(void)reserved;if(reason==1)self_module=module;return 1;}
/* Shared recenter/calibration boundary for the original controller module.
 * game_ref22 has refdef's first22 float-sized words, including viewport ints.
 * The camera origin is words6..8; forward/left/up axes are words9..17. */
__declspec(dllexport) int __cdecl cod2vr_tracking_snapshot(CvFrame *frame,CvPose *anchor,float *game_ref22){
 if(!frame||!anchor||!game_ref22||!tracking_valid||!anchor_valid)return 0;
 for(int retry=0;retry<3;retry++){
  u32 sequence=tracking_publish;if(sequence&1)continue;fence();
  memcpy(frame,&tracking,sizeof(*frame));memcpy(anchor,&tracking_anchor,sizeof(*anchor));memcpy(game_ref22,world_ref,sizeof(world_ref));fence();
  if(sequence==tracking_publish)return 1;
 }
 return 0;
}
