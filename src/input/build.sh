#!/bin/sh
set -eu
task_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$task_dir/build"
clang --target=i686-pc-windows-msvc -ffreestanding -fno-builtin -fno-stack-protector -fasm-blocks -O1 -Wall -Wextra -Werror -c "$task_dir/game_input.c" -o "$task_dir/build/game_input.obj"
clang --target=i686-pc-windows-msvc -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -c "$task_dir/fire_math.c" -o "$task_dir/build/fire_math.obj"
clang --target=i686-pc-windows-msvc -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -c "$task_dir/pickup_math.c" -o "$task_dir/build/pickup_math.obj"
lld-link /lib /def:"$task_dir/kernel32.def" /machine:x86 /out:"$task_dir/build/kernel32.lib"
lld-link /lib /def:"$task_dir/user32.def" /machine:x86 /out:"$task_dir/build/user32.lib"
lld-link /dll /noentry /nodefaultlib /machine:x86 /out:"$task_dir/build/cod2vr_input.dll" "$task_dir/build/game_input.obj" "$task_dir/build/fire_math.obj" "$task_dir/build/pickup_math.obj" "$task_dir/build/kernel32.lib" "$task_dir/build/user32.lib" \
 '/alternatename:__imp__GetModuleHandleA@4=__imp__GetModuleHandleA' '/alternatename:__imp__GetProcAddress@8=__imp__GetProcAddress' \
 '/alternatename:__imp__CreateFileA@28=__imp__CreateFileA' '/alternatename:__imp__CreateFileMappingA@24=__imp__CreateFileMappingA' \
 '/alternatename:__imp__MapViewOfFile@20=__imp__MapViewOfFile' '/alternatename:__imp__CloseHandle@4=__imp__CloseHandle' \
 '/alternatename:__imp__WriteFile@20=__imp__WriteFile' '/alternatename:__imp__FlushFileBuffers@4=__imp__FlushFileBuffers' \
 '/alternatename:__imp__GetTickCount@0=__imp__GetTickCount' '/alternatename:__imp__VirtualProtect@16=__imp__VirtualProtect' \
 '/alternatename:__imp__FlushInstructionCache@12=__imp__FlushInstructionCache' '/alternatename:__imp__GetCurrentProcess@0=__imp__GetCurrentProcess' \
 '/alternatename:__imp__ReadFile@20=__imp__ReadFile' '/alternatename:__imp__VirtualAlloc@16=__imp__VirtualAlloc' \
 '/alternatename:__imp__GetCurrentProcessId@0=__imp__GetCurrentProcessId' '/alternatename:__imp__GetActiveWindow@0=__imp__GetActiveWindow' \
 '/alternatename:__imp__GetClientRect@8=__imp__GetClientRect' '/alternatename:__imp__GetWindowTextA@12=__imp__GetWindowTextA' \
 '/alternatename:__imp__GetWindowThreadProcessId@8=__imp__GetWindowThreadProcessId' '/alternatename:__imp__FindWindowA@8=__imp__FindWindowA' \
 '/alternatename:__imp__PostMessageA@16=__imp__PostMessageA' '/alternatename:__imp__VirtualQuery@12=__imp__VirtualQuery'
