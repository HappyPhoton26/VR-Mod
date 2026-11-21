#!/bin/sh
set -eu
task_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
runtime=/opt/steamvr/bin/linuxarm64
cc -std=gnu99 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers -I"$task_dir/include" "$task_dir/stereo_test.c" "$task_dir/../controllers/actions.c" -o "$task_dir/stereo_test" -L"$runtime" -lopenxr_loader -lEGL -lOpenGL -lm -Wl,-rpath,"$runtime"
