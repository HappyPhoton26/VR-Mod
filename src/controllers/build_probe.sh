#!/bin/sh
set -eu
task_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$task_dir/build"
clang --target=i686-pc-windows-msvc -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -c "$task_dir/controller_probe.c" -o "$task_dir/build/controller_probe.obj"
for src in arms game_hands weapon controller_live; do
 clang --target=i686-pc-windows-msvc -DCV_FREESTANDING -ffreestanding -fno-builtin -fno-stack-protector -fno-math-errno -O1 -Wall -Wextra -Werror -c "$task_dir/$src.c" -o "$task_dir/build/$src.obj"
done
lld-link /lib /def:"$task_dir/probe_kernel32.def" /machine:x86 /out:"$task_dir/build/probe_kernel32.lib"
lld-link /dll /noentry /nodefaultlib /machine:x86 \
 '/alternatename:__imp__GetModuleHandleA@4=__imp__GetModuleHandleA' \
 '/alternatename:__imp__CreateFileA@28=__imp__CreateFileA' \
 '/alternatename:__imp__WriteFile@20=__imp__WriteFile' \
 '/alternatename:__imp__CloseHandle@4=__imp__CloseHandle' \
 '/alternatename:__imp__VirtualAlloc@16=__imp__VirtualAlloc' \
 '/alternatename:__imp__VirtualProtect@16=__imp__VirtualProtect' \
 '/alternatename:__imp__VirtualQuery@12=__imp__VirtualQuery' \
 '/alternatename:__imp__FlushInstructionCache@12=__imp__FlushInstructionCache' \
 '/alternatename:__imp__GetCurrentProcess@0=__imp__GetCurrentProcess' \
 '/alternatename:__imp__GetProcAddress@8=__imp__GetProcAddress' \
 '/alternatename:__imp__ReadFile@20=__imp__ReadFile' \
 '/alternatename:__imp__CreateFileMappingA@24=__imp__CreateFileMappingA' \
 '/alternatename:__imp__MapViewOfFile@20=__imp__MapViewOfFile' \
 /out:"$task_dir/build/cod2vr_controller_probe.dll" "$task_dir/build/controller_probe.obj" "$task_dir/build/controller_live.obj" "$task_dir/build/arms.obj" "$task_dir/build/game_hands.obj" "$task_dir/build/weapon.obj" "$task_dir/build/probe_kernel32.lib"
