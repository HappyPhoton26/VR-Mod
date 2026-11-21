/* Original read-only initialized-game skeleton probe. No third-party code.
 * x86 DLL; loaded by our renderer proxy. Only detours a validated complete
 * instruction; original code executes before logging; no bone data is written. */
typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned char U8;
typedef void *HANDLE;
#define DLL __declspec(dllimport)
DLL HANDLE __stdcall GetModuleHandleA(const char *);
DLL HANDLE __stdcall CreateFileA(const char *,U32,U32,void *,U32,U32,HANDLE);
DLL int __stdcall WriteFile(HANDLE,const void *,U32,U32 *,void *);
DLL int __stdcall CloseHandle(HANDLE);
DLL void * __stdcall VirtualAlloc(void *,U32,U32,U32);
DLL int __stdcall VirtualProtect(void *,U32,U32,U32 *);
DLL U32 __stdcall VirtualQuery(const void *,void *,U32);
DLL int __stdcall FlushInstructionCache(HANDLE,const void *,U32);
DLL HANDLE __stdcall GetCurrentProcess(void);
typedef struct {U32 base,allocation,allocation_protect,size,state,protect,type;} MBI;
static void *original_calc;
static void *original_addweapon=(void *)0x4b69b0;
static HANDLE log_handle;
static int installed,busy;
static char line[4096];
static U32 used;
static U32 seen[64],seen_model[64],seen_caller[64];
static unsigned seen_count;
static U32 last_entity_weapon=0xffffffff;
extern void __cdecl controller_live_hands(void *,void *);
static int readable(U32 address,U32 size){
    if(!address||address+size<address)return 0;
    U32 end=address+size;
    while(address<end){
        MBI m;
        if(VirtualQuery((void *)address,&m,sizeof m)!=sizeof m||m.state!=0x1000||(m.protect&0x101)||m.base+m.size<=address)return 0;
        address=m.base+m.size;
    }
    return 1;
}
static int rd32(U32 address,U32 *value){if(!readable(address,4))return 0;*value=*(U32 *)address;return 1;}
static void text(const char *s){while(*s&&used<sizeof line-1)line[used++]=*s++;}
static void hex(U32 value){const char *digits="0123456789abcdef";text("0x");for(int i=7;i>=0;i--)if(used<sizeof line-1)line[used++]=digits[(value>>(i*4))&15];}
static void string(U32 address){
    for(U32 n=0;n<96;n++){
        if(!readable(address+n,1)){text("<unreadable>");break;}
        U8 c=*(U8 *)(address+n);if(!c)break;
        if(used<sizeof line-1)line[used++]=(c>=32&&c<127)?(char)c:'?';
    }
}
static void finish(void){U32 written;text("\r\n");if(log_handle)WriteFile(log_handle,line,used,&written,0);used=0;}
void controller_probe_note(const char *message){if(busy)return;text(message);finish();}
void controller_probe_contact_report(unsigned active,unsigned held,float grip,float error_l,float error_r,float shift_l,float shift_r){
    static unsigned samples,last_held=0xffffffff;
    if(busy)return;
    if(samples++%300&&held==last_held)return;last_held=held;
    union {float f;U32 u;} value;
    text("CONTACT active=");hex(active);text(" held=");hex(held);
    text(" squeeze=");value.f=grip;hex(value.u);
    text(" errorL=");value.f=error_l;hex(value.u);text(" errorR=");value.f=error_r;hex(value.u);
    text(" shoulderShiftL=");value.f=shift_l;hex(value.u);text(" shoulderShiftR=");value.f=shift_r;hex(value.u);finish();
}
static void record_transform(U32 array,U32 bone){
    U32 at=array+32*bone;
    if(!readable(at,32)){text(" unavailable");return;}
    text(" qxyzwtxyzs(hex-f32)=");
    for(unsigned j=0;j<8;j++){hex(*(U32 *)(at+4*j));text(" ");}
}
static void __cdecl before_addweapon(void *entity,void *playerstate){
    controller_live_hands(entity,playerstate);
    U32 ent=(U32)entity,ps=(U32)playerstate,weapon=0,dobj=0;
    if(busy||!readable(ent,0x48)||!rd32(ps+0xb0,&weapon)||weapon>=128||weapon==last_entity_weapon)return;
    busy=1;last_entity_weapon=weapon;
    rd32(0xf48d80+weapon*0x1b8,&dobj);
    text("ENTITY weapon=");hex(weapon);text(" DObj=");hex(dobj);text(" ptr=");hex(ent);text(" axes-and-origin(hex-f32)=");
    for(unsigned i=0;i<9;i++){hex(*(U32 *)(ent+0x14+4*i));text(" ");}
    for(unsigned i=0;i<3;i++){hex(*(U32 *)(ent+0x3c+4*i));text(" ");}
    finish();busy=0;
}
static void __cdecl after_calc(void *object,U32 caller){
    U32 dobj=(U32)object;
    if(busy||!readable(dobj,0x40))return;
    busy=1;
    U32 model_count=*(U8 *)(dobj+0x18),bone_count=*(U8 *)(dobj+0x19),skel=*(U32 *)(dobj+4),first=*(U32 *)(dobj+0x1c);
    if(!model_count||model_count>8||!bone_count||!readable(dobj+0x1c,model_count*4)){busy=0;return;}
    for(unsigned s=0;s<seen_count;s++)if(seen[s]==dobj&&seen_model[s]==first&&seen_caller[s]==caller){busy=0;return;}
    /* Reserve slot63 for the disassembly-confirmed view-weapon calc caller. */
    if(seen_count>=63){
        if(caller!=0x4b6d2c||(seen[63]==dobj&&seen_model[63]==first&&seen_caller[63]==caller)){busy=0;return;}
        seen[63]=dobj;seen_model[63]=first;seen_caller[63]=caller;
    }else{seen[seen_count]=dobj;seen_model[seen_count]=first;seen_caller[seen_count++]=caller;}
    text("DOBJ ");hex(dobj);text(" caller=");hex(caller);text(" models=");hex(model_count);text(" bones=");hex(bone_count);text(" skel=");hex(skel);finish();
    U32 table=0;rd32(0x8aed00,&table);
    unsigned offset=0;
    for(unsigned m=0;m<model_count;m++){
        U32 model=*(U32 *)(dobj+0x1c+4*m),parts=0,name=0,info=0,names=0;
        if(!rd32(model,&parts)||!readable(parts,0x10)||!rd32(model+0x88,&name))break;
        unsigned count=*(U16 *)parts,roots=*(U16 *)(parts+2);
        text("MODEL ");hex(model);text(" name=");string(name);text(" parts=");hex(parts);text(" count=");hex(count);text(" roots=");hex(roots);finish();
        if(!count||count>128||offset+count>bone_count||!rd32(parts+4,&info)||!rd32(info,&names)||!readable(names,count*2))break;
        for(unsigned k=0;k<count;k++){
            U32 handle=*(U16 *)(names+2*k),bn=table+8*handle+4;
            text("BONE ");hex(offset+k);text(" name=");string(bn);
            if(k>=roots&&readable(info+4+k-roots,1)){
                text(" parent-delta=");hex(*(U8 *)(info+4+k-roots));
            }
            record_transform(skel+0x30,offset+k);finish();
        }
        offset+=count;
    }
    busy=0;
}
__declspec(naked) static void hooked_calc(void){
    __asm {
        push dword ptr [esp+8]
        push dword ptr [esp+8]
        call dword ptr [original_calc]
        add esp,8
        pushfd
        pushad
        mov eax,dword ptr [esp+40]
        mov edx,dword ptr [esp+36]
        push edx
        push eax
        call after_calc
        add esp,8
        popad
        popfd
        ret
    }
}
__declspec(naked) static void hooked_addweapon(void){
    __asm {
        pushfd
        pushad
        mov eax,dword ptr [esp+40]
        push eax
        push esi
        call before_addweapon
        add esp,8
        popad
        popfd
        jmp dword ptr [original_addweapon]
    }
}
__declspec(dllexport) int __cdecl controller_probe_init(void){
    if(installed)return 1;
    U32 module=(U32)GetModuleHandleA(0),pe;
    if(module!=0x400000||!readable(module,0x100)||*(U16 *)module!=0x5a4d)return 0;
    pe=module+*(U32 *)(module+0x3c);
    if(!readable(pe,0x60)||*(U32 *)pe!=0x4550||*(U32 *)(pe+8)!=0x442ca3f7)return 0;
    U8 *target=(U8 *)0x472af0;
    U8 *entity_call=(U8 *)0x4b6ecc;
    const U8 expected[]={0x81,0xec,0xa4,0,0,0,0x53,0x8b,0x9c,0x24,0xb0,0,0,0};
    if(!readable((U32)target,sizeof expected))return 0;
    for(unsigned i=0;i<sizeof expected;i++)if(target[i]!=expected[i])return 0;
    const U8 expected_call[]={0xe8,0xdf,0xfa,0xff,0xff};
    if(!readable((U32)entity_call,sizeof expected_call))return 0;
    for(unsigned i=0;i<sizeof expected_call;i++)if(entity_call[i]!=expected_call[i])return 0;
    U8 *trampoline=VirtualAlloc(0,32,0x3000,0x40);
    if(!trampoline)return 0;
    for(unsigned i=0;i<6;i++)trampoline[i]=target[i];
    trampoline[6]=0xe9;*(U32 *)(trampoline+7)=(U32)(target+6)-(U32)(trampoline+11);
    original_calc=trampoline;
    log_handle=CreateFileA("cod2vr-controller-probe.log",0x40000000,3,0,2,0x80,0);
    if(log_handle==(HANDLE)-1)log_handle=0;
    U32 old_protect;
    if(!VirtualProtect(target,6,0x40,&old_protect)){if(log_handle)CloseHandle(log_handle);log_handle=0;return 0;}
    U32 old_call_protect;
    if(!VirtualProtect(entity_call,5,0x40,&old_call_protect)){
        U32 ignored;VirtualProtect(target,6,old_protect,&ignored);
        if(log_handle)CloseHandle(log_handle);log_handle=0;return 0;
    }
    target[0]=0xe9;*(U32 *)(target+1)=(U32)hooked_calc-(U32)(target+5);target[5]=0x90;
    *(U32 *)(entity_call+1)=(U32)hooked_addweapon-(U32)(entity_call+5);
    U32 ignored;VirtualProtect(target,6,old_protect,&ignored);
    VirtualProtect(entity_call,5,old_call_protect,&ignored);
    FlushInstructionCache(GetCurrentProcess(),trampoline,11);FlushInstructionCache(GetCurrentProcess(),target,6);
    FlushInstructionCache(GetCurrentProcess(),entity_call,5);
    installed=1;text("Original CoD2 skeleton diagnostic installed at ");hex((U32)target);finish();return 1;
}
