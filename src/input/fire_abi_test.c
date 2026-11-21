/* Executes the actual production wrapper with a mock engine convention. */
#define CV_ABI_SELFTEST
#include "game_input.c"
WIN void __stdcall ExitProcess(U32);
static U32 abi_output[16],abi_stack;
__declspec(naked) static void mock_engine(void){
 __asm{
  mov edx,offset abi_output
  cmp eax,edx
  jne bad
  cmp dword ptr [esp+4],0x1234
  jne bad
  mov dword ptr [eax],0xdeadbeef
  mov eax,0xabc12345
  stc
  ret
 bad:
  mov eax,1
  clc
  ret
 }
}
__declspec(naked) static U32 run_abi(void){
 __asm{
  push ebx
  push esi
  push edi
  push ebp
  mov dword ptr [abi_stack],esp
  mov ebx,0x11223344
  mov esi,0x22334455
  mov edi,0x33445566
  mov ebp,0x44556677
  mov eax,offset abi_output
  push 0x1234
  call hook_fire_params
  lea esp,[esp+4]
  pushfd
  pop edx
  cmp eax,0xabc12345
  jne failed_eax
  and edx,1
  cmp edx,1
  jne failed_flags
  cmp esp,dword ptr [abi_stack]
  jne failed_stack
  cmp ebx,0x11223344
  jne failed_ebx
  cmp esi,0x22334455
  jne failed_esi
  cmp edi,0x33445566
  jne failed_edi
  cmp ebp,0x44556677
  jne failed_ebp
  cmp dword ptr [abi_output],0xdeadbeef
  jne failed_output
  mov eax,offset abi_output
  cmp dword ptr [abi_seen_parameters],eax
  jne failed_parameters
  cmp dword ptr [abi_seen_entity],0x1234
  jne failed_entity
  xor eax,eax
  jmp done
 failed_eax:
  mov eax,1
  jmp failed
 failed_flags:
  mov eax,2
  jmp failed
 failed_stack:
  mov eax,3
  jmp failed
 failed_ebx:
  mov eax,4
  jmp failed
 failed_esi:
  mov eax,5
  jmp failed
 failed_edi:
  mov eax,6
  jmp failed
 failed_ebp:
  mov eax,7
  jmp failed
 failed_output:
  mov eax,8
  jmp failed
 failed_parameters:
  mov eax,9
  jmp failed
 failed_entity:
  mov eax,10
 failed:
  mov esp,dword ptr [abi_stack]
 done:
  pop ebp
  pop edi
  pop esi
  pop ebx
  ret
 }
}
void __stdcall abi_entry(void){fire_params_original=mock_engine;ExitProcess(run_abi());}
