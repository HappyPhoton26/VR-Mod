/* Original x86 helpers. String instructions let FEX use bulk copies rather
 * than translating a byte loop for every pixel of the high-resolution eyes. */
__declspec(naked) void *memcpy(void *destination,const void *source,u32 count){
 __asm {
  push edi
  push esi
  mov edi,[esp+12]
  mov esi,[esp+16]
  mov ecx,[esp+20]
  mov eax,edi
  mov edx,ecx
  shr ecx,2
  rep movsd
  mov ecx,edx
  and ecx,3
  rep movsb
  pop esi
  pop edi
  ret
 }
}
__declspec(naked) void *memset(void *destination,int value,u32 count){
 __asm {
  push edi
  mov edi,[esp+8]
  mov eax,[esp+12]
  mov ecx,[esp+16]
  rep stosb
  mov eax,[esp+8]
  pop edi
  ret
 }
}
