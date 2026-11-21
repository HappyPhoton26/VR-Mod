/* Original engine-level gameplay injection. No OS mouse/key approximation. */
#include "../include/vr_input.h"
#include "../include/vr_pose.h"
#include "../include/vr_command.h"
#include "fire_math.h"
#include "pickup_math.h"
#include "../include/vr_scope_math.h"
#include "../controllers/hand_contact.h"
typedef unsigned int U32;typedef unsigned char U8;typedef void *Handle;
#define WIN __declspec(dllimport)
WIN Handle __stdcall GetModuleHandleA(const char *);
WIN void * __stdcall GetProcAddress(Handle,const char *);
WIN Handle __stdcall CreateFileA(const char *,U32,U32,void *,U32,U32,Handle);
WIN Handle __stdcall CreateFileMappingA(Handle,void *,U32,U32,U32,const char *);
WIN void * __stdcall MapViewOfFile(Handle,U32,U32,U32,U32);
WIN int __stdcall CloseHandle(Handle);
WIN int __stdcall WriteFile(Handle,const void *,U32,U32 *,void *);
WIN int __stdcall ReadFile(Handle,void *,U32,U32 *,void *);
WIN int __stdcall FlushFileBuffers(Handle);
WIN U32 __stdcall GetTickCount(void);
WIN int __stdcall VirtualProtect(void *,U32,U32,U32 *);
WIN void * __stdcall VirtualAlloc(void *,U32,U32,U32);
WIN U32 __stdcall VirtualQuery(const void *,void *,U32);
WIN int __stdcall FlushInstructionCache(Handle,const void *,U32);
WIN Handle __stdcall GetCurrentProcess(void);
WIN U32 __stdcall GetCurrentProcessId(void);
WIN Handle __stdcall GetActiveWindow(void);
WIN int __stdcall GetClientRect(Handle,void *);
WIN int __stdcall GetWindowTextA(Handle,char *,int);
WIN U32 __stdcall GetWindowThreadProcessId(Handle,U32 *);
WIN Handle __stdcall FindWindowA(const char *,const char *);
WIN int __stdcall PostMessageA(Handle,U32,U32,int);
int _fltused;
void *memcpy(void *d,const void *s,U32 n){U8 *a=d;const U8 *b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int c,U32 n){U8 *a=d;while(n--)*a++=(U8)c;return d;}
static volatile CvInputTransport *input_map;
static volatile CvCommandTransport *command_map;
static CvButtonsFrame input;
static U32 last_sequence,last_changed_ms,held,previous_buttons;
static int installed,snap_ready=1,last_active;
static U32 pending_weapon_until,last_weapon_attempt;
static int menu_trigger_previous,navigation_direction;
static U32 navigation_next_ms;
static Handle log_file;
static int (__cdecl *snapshot)(CvFrame *,CvPose *,float *);
static void *finish_move=(void *)0x409d00;
static void *add_text=(void *)0x41afc0;
static void *fire_params_original;
static int fire_enabled;
static U32 player_fire_calls,controlled_fire_calls;
static int (__cdecl *muzzle_snapshot)(CvPose *,unsigned long long *,long long *);
#ifdef CV_ABI_SELFTEST
static float *abi_seen_parameters;
static void *abi_seen_entity;
#endif
static void log_text(const char *s){if(!log_file)return;U32 n=0,w;while(s[n])n++;WriteFile(log_file,s,n,&w,0);FlushFileBuffers(log_file);}
static void log_hex(const char *label,U32 value){char line[128];U32 n=0,w;while(*label&&n<100)line[n++]=*label++;line[n++]='=';for(int i=28;i>=0;i-=4)line[n++]="0123456789abcdef"[(value>>i)&15];line[n++]='\n';if(log_file){WriteFile(log_file,line,n,&w,0);FlushFileBuffers(log_file);}}
static void fence(void){__asm{mfence}}
/* Verified private engine event queue: EAX=time(0=now), stack arguments
 * type,key,down,payload_length,payload. No desktop input is generated. */
__declspec(naked) static void queue_key(int key,int down){
 __asm{
  mov ecx,[esp+4]
  mov edx,[esp+8]
  push 0
  push 0
  push edx
  push ecx
  push 1
  xor eax,eax
  mov ecx,0x4500d0
  call ecx
  add esp,20
  ret
 }
}
static void key_tap(int key){queue_key(key,1);queue_key(key,0);}
__declspec(naked) static void command(const char *text){
 __asm{
  mov eax,[esp+4]
  jmp dword ptr [add_text]
 }
}
static void map_input(void){
 if(input_map)return;
 Handle file=CreateFileA("Z:\\home\\steamos\\cod2-vr-work\\transport\\input.bin",0x80000000,3,0,3,0x80,0);
 if(file==(Handle)-1)return;
 Handle mapping=CreateFileMappingA(file,0,2,0,sizeof(CvInputTransport),0);CloseHandle(file);if(!mapping)return;
 input_map=MapViewOfFile(mapping,4,0,0,sizeof(CvInputTransport));CloseHandle(mapping);
}
static int equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
/* Local analysis verified UI_MouseEvent uses EAX=dx, EDX=dy and a640x480
 * virtual cursor. This updates only the game's cursor, never the desktop. */
__declspec(naked) static void point_continue(void){
 __asm{
  mov eax,320
  sub eax,dword ptr [0x10fec3c]
  mov edx,446
  sub edx,dword ptr [0x10fec40]
  mov ecx,0x4c6470
  jmp ecx
 }
}
static void start_mission(void){
 if(!(*(U32 *)0x617f04&8)){log_text("Mission click skipped: game UI inactive\n");return;}
 Handle window=GetActiveWindow();char title[80];U32 pid=0;
 if(!window||!GetWindowTextA(window,title,sizeof title)||!equal(title,"Call of Duty 2"))window=FindWindowA(0,"Call of Duty 2");
 if(!window||!GetWindowTextA(window,title,sizeof title)||!equal(title,"Call of Duty 2")){log_text("Mission click skipped: game HWND absent\n");return;}
 GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()){log_text("Mission click skipped: foreign HWND\n");return;}
 struct {int left,top,right,bottom;} rect;
 if(!GetClientRect(window,&rect)||rect.right<=rect.left||rect.bottom<=rect.top)return;
 const U8 expected[]={0x8b,0x0d,0x3c,0xec,0x0f,0x01,0x03,0xc8};
 for(U32 i=0;i<sizeof expected;i++)if(((U8 *)0x4c6470)[i]!=expected[i])return;
 point_continue();
 U32 x=(U32)(rect.right-rect.left)/2u,y=(U32)(rect.bottom-rect.top)*93u/100u;
 int where=(int)((x&65535u)|((y&65535u)<<16));
 PostMessageA(window,0x200,0,where);PostMessageA(window,0x201,1,where);PostMessageA(window,0x202,0,where);
 log_text("Mission Continue queued only to validated game HWND\n");
}
static void poll_admin_command(void){
 if(!command_map){
  Handle f=CreateFileA("Z:\\home\\steamos\\cod2-vr-work\\transport\\command.bin",0xc0000000,3,0,3,0x80,0);
  if(f!=(Handle)-1){Handle m=CreateFileMappingA(f,0,4,0,sizeof(CvCommandTransport),0);CloseHandle(f);if(m){command_map=MapViewOfFile(m,6,0,0,sizeof(CvCommandTransport));CloseHandle(m);}}
 }
 if(!command_map||command_map->magic!=CV_COMMAND_MAGIC||command_map->version!=CV_COMMAND_VERSION)return;
 U32 request=command_map->request;if(!request||request==command_map->ack)return;fence();
 char text[514];U32 n=0;int valid=0;
 for(;n<512;n++){
  unsigned char c=(unsigned char)command_map->text[n];
  if(!c){valid=1;break;}
  if((c<32&&c!='\n'&&c!='\r'&&c!='\t')||c>126)break;
  text[n]=(char)c;
 }
 fence();if(request!=command_map->request)return;
 while(n&&(text[n-1]=='\n'||text[n-1]=='\r'))n--;
 if(valid&&n){
  text[n]=0;
  if(equal(text,"@start_mission"))start_mission();
  else{if(text[n-1]!='\n')text[n++]='\n';text[n]=0;command(text);}
  log_hex("Admin command queued",request);
 }
 else log_hex("Admin command rejected",request);
 fence();command_map->ack=request;
}
static int read_input(void){
 map_input();U32 now=GetTickCount();
 if(!input_map||input_map->magic!=CV_INPUT_MAGIC||input_map->version!=CV_INPUT_VERSION)return 0;
 for(int attempt=0;attempt<3;attempt++){
  U32 before=input_map->sequence;if(before&1)continue;fence();
  CvButtonsFrame candidate;memcpy(&candidate,(const void *)&input_map->frame,sizeof candidate);fence();
  if(before!=input_map->sequence)continue;
  if(candidate.version!=CV_INPUT_VERSION||candidate.size!=sizeof candidate)return 0;
  if(before!=last_sequence){last_sequence=before;last_changed_ms=now;}
  if((U32)(now-last_changed_ms)>250)return 0;
  input=candidate;return input.active_mask!=0;
 }
 return 0;
}
static void holds(U32 desired){
 static const char *down[]={"+attack 250 0\n","+gostand 251 0\n","+reload 252 0\n","+activate 253 0\n","toggleads\n","+frag 254 0\n","+smoke 255 0\n","+melee_breath 249 0\n"};
 static const char *up[]={"-attack 250 0\n","-gostand 251 0\n","-reload 252 0\n","-activate 253 0\n","leaveads\n","-frag 254 0\n","-smoke 255 0\n","-melee_breath 249 0\n"};
 U32 changed=held^desired;for(U32 i=0;i<8;i++)if(changed&(1u<<i))command((desired&(1u<<i))?down[i]:up[i]);
 if(changed)log_hex("Held gameplay mask",desired);held=desired;
}
static float axis(float value){if(value!=value)return 0;if(value>1)value=1;if(value<-1)value=-1;float a=value<0?-value:value;return a<.18f?0:(value<0?-1:1)*(a-.18f)/.82f;}
static int clamp_move(int v){return v>127?127:(v<-127?-127:v);}
static CvQuat multiply(CvQuat a,CvQuat b){return (CvQuat){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
static int readable(U32 address,U32 bytes){
 U32 end=address+bytes;if(!address||end<address)return 0;
 while(address<end){struct {U32 base,allocation,allocation_protect,size,state,protect,type;} m;
  if(VirtualQuery((void *)address,&m,sizeof m)!=sizeof m||m.state!=0x1000||(m.protect&0x101)||m.base+m.size<=address)return 0;
  address=m.base+m.size;
 }return 1;
}
/* Original SP1.3 server item adapter. Run at the validated cursor-hint
 * callsite in player simulation, not during rendering or desktop input. */
static void *cursor_hints_original=(void *)0x534d40;
static CvPickupGripState pickup_grips;
static U32 pickup_last_scan;
static int frag_primed,frag_hand=-1,frag_aim_valid;
static U32 frag_aim_time,crouch_after;
static float frag_basis[22];
static volatile CvHandContactDebug *pickup_contact;
__declspec(dllexport) struct {
 U32 version,scans,attempts,successes,last_entity,last_hand,last_item;
 float last_distance_squared;
 CvVec3 hands[2];
} cod2vr_pickup_debug;
__declspec(naked) static int native_can_pickup(void *client,void *item){
 __asm{
  mov eax,[esp+4]
  mov ecx,[esp+8]
  push 0
  mov edx,0x4fad50
  call edx
  add esp,4
  ret
 }
}
static int left_on_foregrip(const CvFrame *frame){
 if(!pickup_contact){
  Handle module=GetModuleHandleA("cod2vr_controller_probe.dll");
  if(module)pickup_contact=(volatile CvHandContactDebug *)GetProcAddress(module,"cod2vr_hand_contact");
 }
 if(!pickup_contact)return 0;
 U32 sequence=pickup_contact->publish_sequence;
 if(sequence&1)return 0;
 fence();
 U32 magic=pickup_contact->magic,held_left=pickup_contact->held;
 unsigned long long source=pickup_contact->frame_sequence;
 fence();
 return sequence==pickup_contact->publish_sequence&&magic==0x43485652&&held_left&&
        frame->sequence>=source&&frame->sequence-source<=12;
}
static U32 pickup_inventory_signature(U32 client){
 U32 hash=2166136261u;
 for(U32 offset=0x118;offset<0x528;offset+=4)hash=(hash^*(U32 *)(client+offset))*16777619u;
 return hash;
}
static void __cdecl pickup_update(void *player){
 if(player!=(void *)0x1b15f00||!snapshot||*(U32 *)0x617f04)return;
 if(!read_input()){pickup_grips=(CvPickupGripState){0};return;}
 unsigned available=cv_pickup_grips(&pickup_grips,input.active_mask,input.grip);
 if(frag_primed)available=0;
 if(!available)return;
 U32 now=GetTickCount();if((U32)(now-pickup_last_scan)<100)return;pickup_last_scan=now;
 U32 ent=(U32)player;
 if(!readable(ent,0x1fc)||!*(U8 *)(ent+0xa4)||*(int *)(ent+0x13c)<=0)return;
 U32 client=*(U32 *)(ent+0xfc);
 if(!readable(client,0x528))return;
 U32 weapon_state=*(U32 *)(client+0xb4);
 if(weapon_state>=5&&weapon_state<=9)return;
 CvFrame frame;CvPose anchor;float ref[22];
 if(!snapshot(&frame,&anchor,ref))return;
 long long age=frame.predicted_display_time_ns-input.predicted_time_ns;
 if(age>250000000||age< -250000000)return;
 if(left_on_foregrip(&frame))available&=~1u;
 CvVec3 hand[2];
 for(unsigned h=0;h<2;h++)if((available&(1u<<h))&&!cv_pickup_hand_world(&frame,&anchor,ref,h,&hand[h]))available&=~(1u<<h);
 if(!available)return;
 U32 count=*(U32 *)0x1c6050c;
 if(count<1||count>2048||!readable(0x1b15f00,count*0x1fc))return;
 U32 best=0,best_hand=0;float best_distance=26.f*26.f; /* 65 cm */
 cod2vr_pickup_debug.version=1;cod2vr_pickup_debug.scans++;
 for(unsigned h=0;h<2;h++)if(available&(1u<<h))cod2vr_pickup_debug.hands[h]=hand[h];
 for(U32 i=1;i<count;i++){
  U32 item=0x1b15f00+i*0x1fc;
  if(!*(U8 *)(item+0xa4)||!*(U8 *)(item+0xa0)||*(U8 *)item!=2)continue;
  U32 index=*(unsigned short *)(item+0x158);
  if(index<1||index>=133||index!=*(unsigned short *)(item+0xe))continue;
  U32 kind=*(U32 *)(0x5a66f8+index*44+0x1c);
  if(kind!=1&&kind!=2)continue; /* weapon/grenade or ammo, not use triggers */
  U32 handler=*(U8 *)(item+0x116);
  if(handler>31||*(U32 *)(0x5a573c+44*handler)!=0x521a90)continue;
  CvVec3 lo=*(CvVec3 *)(item+0xc4),hi=*(CvVec3 *)(item+0xd0);
  for(unsigned h=0;h<2;h++)if(available&(1u<<h)){
   float distance=cv_pickup_distance(hand[h],lo,hi);
   if(distance<0||distance>best_distance)continue;
   if(kind==2&&!native_can_pickup((void *)client,(void *)item))continue;
   best=item;best_hand=h;best_distance=distance;
  }
 }
 if(!best)return;
 U32 before=pickup_inventory_signature(client),item_before=*(unsigned short *)(best+0x158);
 cod2vr_pickup_debug.attempts++;cod2vr_pickup_debug.last_entity=(best-0x1b15f00)/0x1fc;
 cod2vr_pickup_debug.last_hand=best_hand;cod2vr_pickup_debug.last_item=item_before;
 cod2vr_pickup_debug.last_distance_squared=best_distance;
 /* Exact original item touch/use ABI: (item, player, automatic=0). This
  * preserves native inventory limits, weapon replacement, scripts and sound. */
 ((void (__cdecl *)(void *,void *,int))0x521a90)((void *)best,player,0);
 log_hex("Grip pickup item",item_before);log_hex("Grip pickup hand",best_hand);
 if(!*(U8 *)(best+0xa4)||*(unsigned short *)(best+0x158)!=item_before||pickup_inventory_signature(client)!=before){
  pickup_grips.consumed|=1u<<best_hand;
  cod2vr_pickup_debug.successes++;log_text("Grip pickup completed through native inventory rules\n");
 }
}
__declspec(naked) static void hook_cursor_hints(void){
 __asm{
  pushfd
  pushad
  push ebx
  call pickup_update
  add esp,4
  popad
  popfd
  jmp dword ptr [cursor_hints_original]
 }
}
static void install_pickup_hook(void){
 U8 *site=(U8 *)0x4e80a6;
 const U8 expected[]={0xe8,0x95,0xcc,0x04,0x00};
 const U8 touch[]={0x8b,0x54,0x24,0x0c,0x85,0xd2,0x8b,0x44};
 const U8 can_grab[]={0x56,0x57,0x8b,0xf9,0x8b,0xf0};
 for(U32 i=0;i<sizeof expected;i++)if(site[i]!=expected[i])return;
 for(U32 i=0;i<sizeof touch;i++)if(((U8 *)0x521a90)[i]!=touch[i])return;
 for(U32 i=0;i<sizeof can_grab;i++)if(((U8 *)0x4fad50)[i]!=can_grab[i])return;
 U32 old,ignored;if(!VirtualProtect(site,5,0x40,&old))return;
 *(U32 *)(site+1)=(U32)hook_cursor_hints-(U32)(site+5);
 VirtualProtect(site,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),site,5);
 log_text("Grip pickup server hook installed: 65cm reach, one item per squeeze\n");
}
static void __cdecl after_fire_params(float *parameters,void *entity){
#ifdef CV_ABI_SELFTEST
 abi_seen_parameters=parameters;abi_seen_entity=entity;
#endif
 /* This engine helper is shared by many actors. Never alter AI or non-player
  * fire. */
 if(entity!=(void *)0x1b15f00)return;
 player_fire_calls++;if(player_fire_calls<=8)log_hex("Player fire parameters",player_fire_calls);
 if(!fire_enabled||!snapshot)return;
 if(!muzzle_snapshot){Handle module=GetModuleHandleA("cod2vr_controller_probe.dll");if(module)muzzle_snapshot=(int (__cdecl *)(CvPose *,unsigned long long *,long long *))GetProcAddress(module,"controller_probe_aim_snapshot");}
 if(!readable((U32)parameters,0x40))return;
 U32 definition=*(U32 *)((U8 *)parameters+0x3c);
 if(!readable(definition,0x7c))return;
 U32 weapon_type=*(U32 *)(definition+0x78);
 if(weapon_type==1&&frag_aim_valid&&(U32)(GetTickCount()-frag_aim_time)<1500){
  for(unsigned i=0;i<3;i++){
   parameters[i]=frag_basis[9+i];parameters[3+i]=-frag_basis[12+i];parameters[6+i]=frag_basis[15+i];
  }
  frag_aim_valid=0;
  return;
 }
 if(weapon_type!=0)return;
 CvFrame frame;CvPose anchor,muzzle;float ref[22];unsigned long long source_sequence;long long source_time;
 if(!snapshot(&frame,&anchor,ref))return;
 int applied=muzzle_snapshot&&muzzle_snapshot(&muzzle,&source_sequence,&source_time)&&
  cv_fire_apply(parameters,(U32)entity,*(U32 *)(definition+0x78),&muzzle,source_sequence,source_time,&frame,ref+6);
 if(!applied){
  float scoped[22];
  if(*(U32 *)(definition+0x78)!=0||!readable(definition,0x284)||!*(U32 *)(definition+0x278)||ref[4]>=40||
     !cv_scope_ref(&frame,&anchor,ref,scoped))return;
  for(unsigned i=0;i<3;i++){
   parameters[i]=scoped[9+i];parameters[3+i]=-scoped[12+i];
   parameters[6+i]=scoped[15+i];parameters[9+i]=scoped[6+i];
  }
  source_sequence=frame.sequence;
 }
 controlled_fire_calls++;if(controlled_fire_calls<=8){log_hex("Controller muzzle shot",controlled_fire_calls);log_hex("Muzzle frame sequence",(U32)source_sequence);}
}
__declspec(naked) static void hook_fire_params(void){
 __asm{
  push eax
  push dword ptr [esp+8]
  call dword ptr [fire_params_original]
  lea esp,[esp+4]
  pushfd
  pushad
  mov eax,[esp+36]
  mov edx,[esp+44]
  push edx
  push eax
  call after_fire_params
  add esp,8
  popad
  popfd
  lea esp,[esp+4]
  ret
 }
}
static void install_fire_hook(void){
 Handle flag=CreateFileA("cod2vr-fire.txt",0x80000000,3,0,3,0x80,0);char setting=0;U32 got=0;
 if(flag!=(Handle)-1){ReadFile(flag,&setting,1,&got,0);CloseHandle(flag);}
 fire_enabled=got&&setting=='1';log_hex("Controller muzzle firing enabled",fire_enabled);
 if(!fire_enabled)return;
 U8 *target=(U8 *)0x531b90;const U8 expected[]={0x83,0xec,0x0c,0x53,0x55,0x8b,0x6c,0x24,0x18};
 for(U32 i=0;i<sizeof expected;i++)if(target[i]!=expected[i]){log_text("Fire helper signature mismatch; unchanged\n");return;}
 U8 *trampoline=VirtualAlloc(0,16,0x3000,0x40);if(!trampoline)return;
 memcpy(trampoline,target,5);trampoline[5]=0xe9;*(U32 *)(trampoline+6)=(U32)(target+5)-(U32)(trampoline+10);fire_params_original=trampoline;
 U32 old;if(!VirtualProtect(target,5,0x40,&old))return;
 target[0]=0xe9;*(U32 *)(target+1)=(U32)hook_fire_params-(U32)(target+5);U32 ignored;VirtualProtect(target,5,old,&ignored);
 FlushInstructionCache(GetCurrentProcess(),target,5);FlushInstructionCache(GetCurrentProcess(),trampoline,10);
}
static void update_frag(int active,U32 rising){
 if(!active){
  frag_primed=0;frag_hand=-1;frag_aim_valid=0;
  return;
 }
 if((rising&CV_BUTTON_DPAD_UP)&&!frag_primed){
  frag_primed=1;frag_hand=-1;frag_aim_valid=0;
 }
 if(!frag_primed)return;
 if(frag_hand<0){
  for(int h=CV_HAND_RIGHT;h>=(int)CV_HAND_LEFT;h--){
   if((input.active_mask&(1u<<h))&&input.grip[h]>.65f){frag_hand=h;break;}
  }
 }
 if(frag_hand<0)return;
 CvFrame frame;CvPose anchor;float ref[22];
 if((input.active_mask&(1u<<frag_hand))&&snapshot&&snapshot(&frame,&anchor,ref)){
  frame.aim[CV_HAND_RIGHT]=frame.aim[frag_hand];
  if(cv_scope_ref(&frame,&anchor,ref,frag_basis)){frag_aim_valid=1;frag_aim_time=GetTickCount();}
 }
 if((input.active_mask&(1u<<frag_hand))&&input.grip[frag_hand]<.35f){
  frag_primed=0;frag_hand=-1;
 }
}
static void process_buttons(int active){
 if(active!=last_active){log_hex("Controller input active",active);last_active=active;}
 U32 buttons=active?input.buttons:0,rising=buttons&~previous_buttons;previous_buttons=buttons;
 if(rising)log_hex("Pressed buttons",rising);
 int trigger=active&&input.trigger[1]>.55f,trigger_rising=trigger&&!menu_trigger_previous;menu_trigger_previous=trigger;
 if(rising&CV_BUTTON_MENU)key_tap(27);
 int menu=(*(U32 *)0x617f04)!=0;
 if(active&&menu){
  if((rising&CV_BUTTON_A)||trigger_rising)key_tap(13);
  if(rising&CV_BUTTON_B)key_tap(27);
  int direction=0;
  if(input.stick[0][1]>.65f||(buttons&CV_BUTTON_DPAD_UP))direction=154;
  else if(input.stick[0][1]<-.65f||(buttons&CV_BUTTON_DPAD_DOWN))direction=155;
  else if(input.stick[0][0]<-.65f||(buttons&CV_BUTTON_DPAD_LEFT))direction=156;
  else if(input.stick[0][0]>.65f||(buttons&CV_BUTTON_DPAD_RIGHT))direction=157;
  U32 now=GetTickCount();
  if(direction&&(direction!=navigation_direction||(int)(now-navigation_next_ms)>=0)){key_tap(direction);navigation_next_ms=now+250;}
  navigation_direction=direction;
 }else navigation_direction=0;
 U32 desired=0;
 update_frag(active&&!menu,rising);
 if(active&&!menu){
  if(input.trigger[1]>.55f)desired|=1;
  if(buttons&CV_BUTTON_A)desired|=2;
  if(buttons&CV_BUTTON_RIGHT_BUMPER)desired|=4;
  if(buttons&CV_BUTTON_X)desired|=8;
  if(input.trigger[0]>.55f)desired|=16;
  if(frag_primed)desired|=32;
  if(buttons&CV_BUTTON_DPAD_DOWN)desired|=64;
  if(buttons&CV_BUTTON_DPAD_LEFT)desired|=128;
  if(rising&CV_BUTTON_DPAD_RIGHT)command("toggleprone\n");
  if(rising&CV_BUTTON_Y){pending_weapon_until=GetTickCount()+500;last_weapon_attempt=0;}
  U32 now=GetTickCount();
  if(rising&CV_BUTTON_RIGHT_STICK)crouch_after=now+80u;
  if(frag_primed||(buttons&CV_BUTTON_DPAD_UP))crouch_after=0;
  if(crouch_after&&(int)(now-crouch_after)>=0){
   command("togglecrouch\n");crouch_after=0;
  }
  if(pending_weapon_until){
   U32 now=GetTickCount();
   if((int)(pending_weapon_until-now)<0)pending_weapon_until=0;
   else if(!last_weapon_attempt||(U32)(now-last_weapon_attempt)>=40){
    U32 before=*(U32 *)0xf796e0;((void (__cdecl *)(void))0x4b6fd0)();U32 after=*(U32 *)0xf796e0;last_weapon_attempt=now;
    log_hex("Weapon selection before",before);log_hex("Weapon selection after",after);
    if(before!=after)pending_weapon_until=0;
   }
  }
 }
 holds(desired);
 if(!active||menu){snap_ready=1;pending_weapon_until=0;crouch_after=0;}
}
__declspec(dllexport) void __cdecl cod2vr_input_tick(void){poll_admin_command();process_buttons(read_input());}
__declspec(dllexport) int __cdecl cod2vr_input_menu_active(void){return (*(U32 *)0x617f04&8)!=0;}
static void __cdecl after_finish(void *cmd){
 int active=read_input();process_buttons(active);
 if(!active||*(U32 *)0x617f04)return;
 if(input.active_mask&CV_INPUT_LEFT_ACTIVE){
  float right=axis(input.stick[0][0]),forward=axis(input.stick[0][1]);
  CvFrame frame;CvPose anchor;float ref[22];
  if(snapshot&&snapshot(&frame,&anchor,ref)&&(frame.head.valid&3)==3){
   CvQuat inv={-anchor.orientation.x,-anchor.orientation.y,-anchor.orientation.z,anchor.orientation.w};
   CvQuat q=multiply(inv,frame.head.pose.orientation);
   float heading_right=-2.f*(q.x*q.z+q.w*q.y),heading_forward=1.f-2.f*(q.x*q.x+q.y*q.y);
   float f=forward*heading_forward-right*heading_right;
   right=right*heading_forward+forward*heading_right;forward=f;
  }
  signed char *movement=(signed char *)cmd;
  movement[0x18]=(signed char)clamp_move((int)movement[0x18]+(int)(forward*127));
  movement[0x19]=(signed char)clamp_move((int)movement[0x19]+(int)(right*127));
 }
 if(input.active_mask&CV_INPUT_RIGHT_ACTIVE){
  float turn=input.stick[1][0];if(turn<.3f&&turn>-.3f)snap_ready=1;
  if(snap_ready&&(turn>.75f||turn<-.75f)){
   float *yaw=(float *)0x621588;*yaw+=turn>0?-45.f:45.f;snap_ready=0;
   ((U32 *)cmd)[4]=((int)(*yaw*(65536.f/360.f)))&65535;
   log_hex("Snap turn",turn>0?45:315);
  }
 }
}
__declspec(naked) static void hook_finish(void){
 __asm{
  call dword ptr [finish_move]
  pushfd
  pushad
  push ebx
  call after_finish
  add esp,4
  popad
  popfd
  ret
 }
}
__declspec(dllexport) int __cdecl cod2vr_input_init(void){
 if(installed)return 1;
 U8 *module=GetModuleHandleA(0);if(module!=(U8 *)0x400000)return 0;
 U32 pe=*(U32 *)(module+0x3c);if(*(U32 *)(module+pe+8)!=0x442ca3f7)return 0;
 U8 *call=(U8 *)0x409e27;const U8 expected[]={0xe8,0xd4,0xfe,0xff,0xff};
 const U8 cbuf[]={0x53,0x56,0x57,0x8b,0xf0};
 const U8 queue[]={0x8b,0x0d,0xec,0x6f,0x82,0x00};
 for(U32 i=0;i<sizeof expected;i++)if(call[i]!=expected[i])return 0;
 for(U32 i=0;i<sizeof cbuf;i++)if(((U8 *)add_text)[i]!=cbuf[i])return 0;
 for(U32 i=0;i<sizeof queue;i++)if(((U8 *)0x4500d0)[i]!=queue[i])return 0;
 log_file=CreateFileA("cod2vr-input.log",0x40000000,3,0,2,0x80,0);if(log_file==(Handle)-1)log_file=0;
 U32 old;if(!VirtualProtect(call,5,0x40,&old))return 0;
 *(U32 *)(call+1)=(U32)hook_finish-(U32)(call+5);U32 ignored;VirtualProtect(call,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),call,5);
 Handle renderer=GetModuleHandleA("gfx_d3d_x86_s.dll");if(renderer)snapshot=(int (__cdecl *)(CvFrame *,CvPose *,float *))GetProcAddress(renderer,"cod2vr_tracking_snapshot");
 install_fire_hook();
 install_pickup_hook();
 installed=1;log_text("Original engine gameplay injection installed; analog usercmd + console action transitions\n");return 1;
}
