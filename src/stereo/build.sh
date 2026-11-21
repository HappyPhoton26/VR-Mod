#!/bin/sh
set -eu
task_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$task_dir/build"
clang --target=i686-pc-windows-msvc -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror -Wno-missing-field-initializers -c "$task_dir/cv_xr.c" -o "$task_dir/build/cv_xr.obj"
lld-link /lib /def:"$task_dir/kernel32.def" /machine:x86 /out:"$task_dir/build/kernel32.lib"
lld-link /dll /noentry /nodefaultlib /machine:x86 '/alternatename:__imp__LoadLibraryA@4=__imp__LoadLibraryA' '/alternatename:__imp__GetProcAddress@8=__imp__GetProcAddress' '/alternatename:__imp__FreeLibrary@4=__imp__FreeLibrary' /out:"$task_dir/build/cod2vr_xr.dll" "$task_dir/build/cv_xr.obj" "$task_dir/build/kernel32.lib"
