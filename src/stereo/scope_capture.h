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
static void scope_header(unsigned active){
 scope_map->magic=CV_SCOPE_MAGIC;scope_map->version=CV_SCOPE_VERSION;
 scope_map->width=scope_map->height=CV_SCOPE_SIZE;scope_map->stride=CV_SCOPE_SIZE*4;
 scope_map->reserved[0]=active;fence();scope_map->sequence=(scope_map->sequence&~1u)+2;
}
static void scope_off(void){
 if(scope_map)scope_header(0);
}
static int scope_prepare(const float *ref){
 scope_active=0;
 if(!ipc_enabled||!tracking_valid||ref[4]>=40||ref[4]<=1)return 0;
 unsigned weapon=*(u32 *)(0xf708e8+0xb0);if(!weapon||weapon>=128)return 0;
 u8 *definition=*(u8 **)(0x1c64548+4*weapon);if(!definition)return 0;
 float zoom=*(float *)(definition+0x268);
 if(*(u32 *)(definition+0x278)==0||zoom<2||zoom>35)return 0;
 if(!scope_aim){
  Handle module=LoadLibraryA("cod2vr_controller_probe.dll");
  if(module)scope_aim=(ScopeAim)GetProcAddress(module,"controller_probe_aim_snapshot");
 }
 if(!cv_scope_ref(&tracking,&tracking_anchor,ref,(float *)scope_ref))return 0;
 if(!scope_surfaces())return 0;
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
static int scope_capture(void *source){
 if(!scope_active||!scope_surfaces()||!blit(source,scope_gpu,0))return 0;
 void *device=*(void **)(base+0x1d1bf8);if(!device)return 0;
 typedef i32 (__stdcall *Readback)(void *,void *,void *);
 if(((Readback)(*(void ***)device)[32])(device,scope_gpu,scope_readback)<0)return 0;
 typedef struct {i32 pitch;void *bits;} ScopeLock;
 typedef i32 (__stdcall *Lock)(void *,ScopeLock *,const Rect *,u32);
 typedef i32 (__stdcall *Unlock)(void *);
 ScopeLock lock;
 if(((Lock)(*(void ***)scope_readback)[13])(scope_readback,&lock,0,0x10)<0)return 0;
 u32 slot=(scope_map->slot+1)%3,generation=scope_map->generation[slot]&~1u;
 scope_map->generation[slot]=generation+1;fence();
 u8 *out=(u8 *)scope_map+64+slot*CV_SCOPE_SLOT_BYTES;
 for(u32 y=0;y<CV_SCOPE_SIZE;y++)memcpy(out+y*CV_SCOPE_SIZE*4,(u8 *)lock.bits+y*lock.pitch,CV_SCOPE_SIZE*4);
 ((Unlock)(*(void ***)scope_readback)[14])(scope_readback);
 fence();scope_map->generation[slot]=generation+2;fence();scope_map->slot=slot;
 scope_header(1);scope_captured=1;
 if(scope_frames++<3)log_text("Magnified rifle scope image published (640 square; tracked rifle aim)\n");
 return 1;
}
