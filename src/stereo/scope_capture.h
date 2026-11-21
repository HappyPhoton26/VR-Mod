static int scope_surfaces(void){
 void *device=*(void **)(base+0x1d1bf8);if(!device)return 0;
 if(!scope_map)scope_map=map_file("Z:\\home\\steamos\\cod2-vr-work\\transport\\scope.bin",CV_SCOPE_MAP_BYTES);
 if(!scope_map)return 0;
 void **vt=*(void ***)device;
 typedef i32 (__stdcall *CreateRT)(void *,u32,u32,u32,u32,u32,i32,void **,void *);
 typedef i32 (__stdcall *CreateOffscreen)(void *,u32,u32,u32,u32,void **,void *);
 if(!scope_gpu&&((CreateRT)vt[28])(device,CV_SCOPE_SIZE,CV_SCOPE_SIZE,21,0,0,0,&scope_gpu,0)<0)return 0;
 if(!scope_readback&&((CreateOffscreen)vt[36])(device,CV_SCOPE_SIZE,CV_SCOPE_SIZE,21,2,&scope_readback,0)<0)return 0;
 return 1;
}
static void scope_header(unsigned active,unsigned native_overlay){
 scope_map->magic=CV_SCOPE_MAGIC;scope_map->version=CV_SCOPE_VERSION;
 scope_map->width=scope_map->height=CV_SCOPE_SIZE;scope_map->stride=CV_SCOPE_SIZE*4;
 scope_map->reserved[0]=active;scope_map->reserved[1]=native_overlay;fence();scope_map->sequence=(scope_map->sequence&~1u)+2;
}
static void scope_off(void){
 if(scope_map)scope_header(0,0);
}
static int scope_prepare(const float *ref){
 scope_active=0;
 if(!ipc_enabled||!tracking_valid||(*(u32 *)0x617f04&8u))return 0;
 int controller_ads=input_scope_requested&&input_scope_requested();
 unsigned weapon=*(u32 *)(0xf708e8+0xb0);
 static int request_logged;
 if(controller_ads&&!request_logged)log_hex("Scope requested for equipped weapon",weapon);
 request_logged=controller_ads;
 if(!weapon||weapon>=128)return 0;
 u8 *definition=*(u8 **)(0x1c64548+4*weapon);if(!definition)return 0;
 float zoom=*(float *)(definition+0x268);
 if(!cv_scope_requested(controller_ads,ref[4],zoom,*(u32 *)(definition+0x278)))return 0;
 if(!scope_aim){
  Handle module=LoadLibraryA("cod2vr_controller_probe.dll");
  if(module)scope_aim=(ScopeAim)GetProcAddress(module,"controller_probe_aim_snapshot");
 }
 if(!cv_scope_ref(&tracking,&tracking_anchor,ref,(float *)scope_ref)){
  /* Keep stock ADS usable when right-controller aim tracking drops. */
  CvFrame head_aim=tracking;
  head_aim.aim[CV_HAND_RIGHT]=tracking.head;
  if(!cv_scope_ref(&head_aim,&tracking_anchor,ref,(float *)scope_ref))return 0;
 }
 if(!scope_surfaces())return 0;
 scope_native_overlay=ref[4]>1&&ref[4]<=zoom+1;
 scope_fov=zoom;float *r=(float *)scope_ref;r[4]=r[5]=zoom;
 /* While the normal model is still submitted, use its exact stabilized
  * muzzle. Fully scoped weapons use the shared controller-only camera above. */
 CvPose muzzle;unsigned long long source;long long time;
 if(scope_aim&&scope_aim(&muzzle,&source,&time)&&tracking.sequence>=source&&tracking.sequence-source<=3){
  CvVec3 forward=rotate(muzzle.orientation,(CvVec3){1,0,0});
  CvVec3 left=rotate(muzzle.orientation,(CvVec3){0,1,0});
  CvVec3 up=rotate(muzzle.orientation,(CvVec3){0,0,1});
  r[6]=muzzle.position.x;r[7]=muzzle.position.y;r[8]=muzzle.position.z;
  r[9]=forward.x;r[10]=forward.y;r[11]=forward.z;
  r[12]=left.x;r[13]=left.y;r[14]=left.z;r[15]=up.x;r[16]=up.y;r[17]=up.z;
 }
 scope_active=1;return 1;
}
static int scope_capture(void *source,const Rect *crop,unsigned native_overlay){
 if(!source||!scope_active||!scope_surfaces())return 0;
 void *device=*(void **)(base+0x1d1bf8);if(!device)return 0;
 source=capture_source(device,source);if(!source)return 0;
 typedef i32 (__stdcall *Stretch)(void *,void *,const Rect *,void *,const Rect *,u32);
 if(((Stretch)(*(void ***)device)[34])(device,source,crop,scope_gpu,0,2)<0)return 0;
 typedef i32 (__stdcall *Readback)(void *,void *,void *);
 if(((Readback)(*(void ***)device)[32])(device,scope_gpu,scope_readback)<0)return 0;
 typedef struct {i32 pitch;void *bits;} ScopeLock;
 typedef i32 (__stdcall *Lock)(void *,ScopeLock *,const Rect *,u32);
 typedef i32 (__stdcall *Unlock)(void *);
 ScopeLock lock;
 if(((Lock)(*(void ***)scope_readback)[13])(scope_readback,&lock,0,0x10)<0)return 0;
 /* An empty replay can still return a successful readback. */
 int has_image=0;
 u32 start=native_overlay?CV_SCOPE_SIZE/4:16;
 u32 end=native_overlay?CV_SCOPE_SIZE*3/4:CV_SCOPE_SIZE;
 for(u32 y=start;y<end&&!has_image;y+=32){
  const u8 *row=(const u8 *)lock.bits+y*lock.pitch;
  for(u32 x=start;x<end;x+=32){
   const u8 *pixel=row+x*4;
   if(pixel[0]>4||pixel[1]>4||pixel[2]>4){has_image=1;break;}
  }
 }
 if(!has_image){
  ((Unlock)(*(void ***)scope_readback)[14])(scope_readback);
  return 0;
 }
 u32 slot=(scope_map->slot+1)%3,generation=scope_map->generation[slot]&~1u;
 scope_map->generation[slot]=generation+1;fence();
 u8 *out=(u8 *)scope_map+64+slot*CV_SCOPE_SLOT_BYTES;
 for(u32 y=0;y<CV_SCOPE_SIZE;y++)memcpy(out+y*CV_SCOPE_SIZE*4,(u8 *)lock.bits+y*lock.pitch,CV_SCOPE_SIZE*4);
 ((Unlock)(*(void ***)scope_readback)[14])(scope_readback);
 fence();scope_map->generation[slot]=generation+2;fence();scope_map->slot=slot;
 scope_header(1,native_overlay);scope_captured=1;
 if(scope_frames++<3)log_text("Magnified rifle scope image published (640 square; tracked rifle aim)\n");
 return 1;
}
static Rect scope_center_square(void){
 i32 size=surface_width<surface_height?(i32)surface_width:(i32)surface_height;
 i32 x=((i32)surface_width-size)/2,y=((i32)surface_height-size)/2;
 return (Rect){x,y,x+size,y+size};
}
static int scope_capture_world(void *source){
 Rect square=scope_center_square();
 return scope_capture(source,scope_native_overlay?&square:0,0);
}
static void scope_capture_native(void){
 if(!scope_native_overlay||!scope_view_applied||!hud_prepared||(*(u32 *)0x617f04&8u))return;
 void *target=*(void **)(base+0x1d4820+4);
 Rect square=scope_center_square();
 if(target==hud_before_target)scope_capture(target,&square,1);
}
/* Reuse a magnified crop only when the dedicated pass fails. Locate the
 * rifle's aim ray in the already rendered right eye to center the crop. */
static void scope_capture_eye(void){
 float right_eye[22];memcpy(right_eye,world_ref,sizeof(right_eye));
 tracked_ref(right_eye,tracking.eyes[1].pose);
 const float *aim=(const float *)scope_ref;
 float forward=0,left=0,up=0;
 for(unsigned i=0;i<3;i++){
  forward+=aim[9+i]*right_eye[9+i];
  left+=aim[9+i]*right_eye[12+i];
  up+=aim[9+i]*right_eye[15+i];
 }
 if(forward<=.1f)return;
 CvFov f=tracking.fov[1];
 float l=tangent(f.angle_left),r=tangent(f.angle_right);
 float b=tangent(f.angle_down),t=tangent(f.angle_up);
 if(r-l<.1f||t-b<.1f)return;
 float x=(-left/forward-l)/(r-l),y=(t-up/forward)/(t-b);
 float radius=tangent(scope_fov*.00872664626f);
 float dx=radius/(r-l),dy=radius/(t-b);
 if(x-dx<0||x+dx>1||y-dy<0||y+dy>1)return;
 Rect crop={(i32)((x-dx)*surface_width),(i32)((y-dy)*surface_height),
            (i32)((x+dx)*surface_width),(i32)((y+dy)*surface_height)};
 scope_capture(right_surface,&crop,0);
}
