typedef unsigned int u32;
#include "freestanding_memory.h"
__declspec(dllimport) void __stdcall ExitProcess(u32);
static unsigned char source[520],destination[520];
void __stdcall test_entry(void){
 for(u32 i=0;i<520;i++)source[i]=(unsigned char)(i*17u+3u);
 for(u32 shift=0;shift<4;shift++)for(u32 n=0;n<=257;n++){
  for(u32 i=0;i<520;i++)destination[i]=0xcc;
  if(memcpy(destination+shift,source+3,n)!=destination+shift)ExitProcess(1);
  for(u32 i=0;i<520;i++){
   unsigned char expected=i>=shift&&i<shift+n?source[3+i-shift]:0xcc;
   if(destination[i]!=expected)ExitProcess(2);
  }
  if(memset(destination+shift,0xa5,n)!=destination+shift)ExitProcess(3);
  for(u32 i=0;i<520;i++){
   unsigned char expected=i>=shift&&i<shift+n?0xa5:0xcc;
   if(destination[i]!=expected)ExitProcess(4);
  }
 }
 ExitProcess(0);
}
