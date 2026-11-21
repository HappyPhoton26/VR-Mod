/* Original low-bandwidth HUD extraction. The world is captured before 2D UI;
 * compare that image against the completed frame to preserve the game's own
 * HUD artwork and indicators without another flat desktop panel. */
static int hud_surfaces(void){
 if(!ipc_enabled)return 0;
 void *device=*(void **)(base+0x1d1bf8);if(!device)return 0;
 if(!hud_map)hud_map=map_file("Z:\\home\\steamos\\cod2-vr-work\\transport\\hud.bin",CV_HUD_MAP_BYTES);
 if(!hud_map)return 0;
 void **vt=*(void ***)device;
 typedef i32 (__stdcall *CreateRT)(void *,u32,u32,u32,u32,u32,i32,void **,void *);
 typedef i32 (__stdcall *CreateOffscreen)(void *,u32,u32,u32,u32,void **,void *);
 if(!hud_gpu&&((CreateRT)vt[28])(device,CV_HUD_WIDTH,CV_HUD_HEIGHT,21,0,0,0,&hud_gpu,0)<0)return 0;
 if(!hud_before_surface&&((CreateOffscreen)vt[36])(device,CV_HUD_WIDTH,CV_HUD_HEIGHT,21,2,&hud_before_surface,0)<0)return 0;
 if(!hud_after_surface&&((CreateOffscreen)vt[36])(device,CV_HUD_WIDTH,CV_HUD_HEIGHT,21,2,&hud_after_surface,0)<0)return 0;
 return 1;
}
static int hud_snapshot(void *source,void *destination){
 if(!source||!hud_gpu||!destination)return 0;
 void *device=*(void **)(base+0x1d1bf8);if(!device)return 0;
 source=capture_source(device,source);if(!source)return 0;
 typedef i32 (__stdcall *Stretch)(void *,void *,const Rect *,void *,const Rect *,u32);
 typedef i32 (__stdcall *Readback)(void *,void *,void *);
 void **vt=*(void ***)device;
 if(((Stretch)vt[34])(device,source,0,hud_gpu,0,2)<0)return 0;
 return ((Readback)vt[32])(device,hud_gpu,destination)>=0;
}
static void hud_capture_before(void *source){
 hud_prepared=0;
 if(hud_surfaces()&&hud_snapshot(source,hud_before_surface)){
  hud_before_target=source;hud_before_frame=frame_count;hud_prepared=1;
 }
}
static void hud_publish_after(void){
 if(!ipc_enabled||!base)return;
 u32 menu=(*(u32 *)0x617f04&8u)!=0;
 if(world_seen)hud_frames_without_world=0;
 else if(hud_frames_without_world<90)hud_frames_without_world++;
 if(!hud_surfaces())return;
 void *source=*(void **)(base+0x1d4820+4);
 int compare=hud_prepared&&hud_before_frame==frame_count&&source==hud_before_target;
 /* Keep recent gameplay transitions transparent even if the UI catcher
  * briefly takes over while the world is missing or changing targets. */
 int opaque_menu=menu&&(hud_menu_clean||hud_frames_without_world>=90);
 int captured=(opaque_menu||(compare&&!scope_binoculars))&&hud_snapshot(source,hud_after_surface);
 typedef struct {i32 pitch;void *bits;} HudLock;
 typedef i32 (__stdcall *Lock)(void *,HudLock *,const Rect *,u32);
 typedef i32 (__stdcall *Unlock)(void *);
 HudLock before={0},after={0};int locked_before=0;
 if(captured&&compare){
  locked_before=((Lock)(*(void ***)hud_before_surface)[13])(hud_before_surface,&before,0,0x10)>=0;
  captured=locked_before;
 }
 if(captured&&((Lock)(*(void ***)hud_after_surface)[13])(hud_after_surface,&after,0,0x10)<0){
  captured=0;
 }
 u32 slot=(hud_map->slot+1u)%3u,generation=hud_map->generation[slot]&~1u;
 hud_map->generation[slot]=generation+1u;fence();
 u8 *pixels=(u8 *)hud_map+64u+slot*CV_HUD_SLOT_BYTES;
 memset(pixels,0,CV_HUD_SLOT_BYTES);
 /* A hit vignette can tint a dense patch while leaving most pixels alone.
  * Count filled tiles as well as overall coverage so those patches cannot
  * be presented as a head-relative slice of the world. */
 enum {tile_size=20,tiles_x=CV_HUD_WIDTH/tile_size,tiles_y=CV_HUD_HEIGHT/tile_size};
 unsigned short changed[tiles_y][tiles_x]={0};
 u32 visible=0;
 for(u32 y=0;captured&&y<CV_HUD_HEIGHT;y++){
  const u8 *a=(const u8 *)after.bits+y*after.pitch;
  const u8 *b=locked_before?(const u8 *)before.bits+y*before.pitch:0;
  for(u32 x=0;x<CV_HUD_WIDTH;x++){
   u32 index=4*x;int difference=0;
   if(compare)for(u32 c=0;c<3;c++){int d=(int)a[index+c]-(int)b[index+c];difference+=d<0?-d:d;}
   if(compare&&scope_native_overlay){
    /* The composed scope travels on its own layer. */
    i32 dx=(i32)x-CV_HUD_WIDTH/2,dy=(i32)y-CV_HUD_HEIGHT/2;
    if(dx*dx+dy*dy<=(i32)(CV_HUD_HEIGHT*CV_HUD_HEIGHT/4))continue;
   }
   if(opaque_menu||difference>9){
    u8 *out=pixels+(y*CV_HUD_WIDTH+x)*4u;
    out[0]=a[index];out[1]=a[index+1];out[2]=a[index+2];out[3]=255;visible++;
    if(compare)changed[y/tile_size][x/tile_size]++;
   }
  }
 }
 if(captured)((Unlock)(*(void ***)hud_after_surface)[14])(hud_after_surface);
 if(locked_before)((Unlock)(*(void ***)hud_before_surface)[14])(hud_before_surface);
 /* Full-screen fades and scene changes affect the world beneath the HUD.
  * Suppress their difference image so it cannot become a flat world panel. */
 if(compare){
  u32 dense=0;
  for(u32 y=0;y<tiles_y;y++)for(u32 x=0;x<tiles_x;x++){
   if(changed[y][x]>=tile_size*tile_size*7u/10u)dense++;
  }
  if(dense>=16||visible>CV_HUD_WIDTH*CV_HUD_HEIGHT/8u)visible=0;
 }
 hud_map->magic=CV_HUD_MAGIC;hud_map->version=CV_HUD_VERSION;
 hud_map->width=CV_HUD_WIDTH;hud_map->height=CV_HUD_HEIGHT;hud_map->stride=CV_HUD_WIDTH*4u;
 hud_map->reserved[0]=visible?menu:0;hud_map->reserved[1]=visible;
 fence();hud_map->generation[slot]=generation+2u;fence();hud_map->slot=slot;
 fence();hud_map->sequence=(hud_map->sequence&~1u)+2u;
 if(hud_published++<3){log_hex("HUD pixels",visible);log_hex("HUD menu",menu);}
}
