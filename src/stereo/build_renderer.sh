#!/bin/sh
set -eu
task_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$task_dir/build"
clang --target=i686-pc-windows-msvc -ffreestanding -fno-builtin -fno-stack-protector -fasm-blocks -O1 -Wall -Wextra -Werror -Wno-unused-parameter -c "$task_dir/renderer_proxy.c" -o "$task_dir/build/renderer_proxy.obj"
lld-link /lib /def:"$task_dir/renderer_kernel32.def" /machine:x86 /out:"$task_dir/build/renderer_kernel32.lib"
lld-link /dll /entry:DllMain@12 /nodefaultlib /machine:x86 /out:"$task_dir/build/gfx_d3d_x86_s.dll" "$task_dir/build/renderer_proxy.obj" "$task_dir/build/renderer_kernel32.lib" \
 '/alternatename:__imp__LoadLibraryA@4=__imp__LoadLibraryA' '/alternatename:__imp__GetProcAddress@8=__imp__GetProcAddress' \
 '/alternatename:__imp__GetModuleFileNameA@12=__imp__GetModuleFileNameA' '/alternatename:__imp__GetEnvironmentVariableA@12=__imp__GetEnvironmentVariableA' \
 '/alternatename:__imp__CreateFileA@28=__imp__CreateFileA' '/alternatename:__imp__WriteFile@20=__imp__WriteFile' \
 '/alternatename:__imp__FlushFileBuffers@4=__imp__FlushFileBuffers' '/alternatename:__imp__VirtualProtect@16=__imp__VirtualProtect' \
 '/alternatename:__imp__CreateFileMappingA@24=__imp__CreateFileMappingA' '/alternatename:__imp__MapViewOfFile@20=__imp__MapViewOfFile' \
 '/alternatename:__imp__ReadFile@20=__imp__ReadFile' '/alternatename:__imp__CloseHandle@4=__imp__CloseHandle'
