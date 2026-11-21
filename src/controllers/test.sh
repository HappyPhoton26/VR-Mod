#!/bin/sh
set -eu
task_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cc -std=c99 -Wall -Wextra -Werror -pedantic -fsanitize=undefined \
  "$task_dir/arms.c" "$task_dir/test_arms.c" -lm -o "$task_dir/test_arms"
cc -std=c99 -Wall -Wextra -Werror -pedantic -fsanitize=undefined \
  "$task_dir/actions.c" "$task_dir/test_actions.c" -o "$task_dir/test_actions"
"$task_dir/test_arms"
"$task_dir/test_actions"
cc -std=c99 -Wall -Wextra -Werror -pedantic -fsanitize=undefined \
  "$task_dir/arms.c" "$task_dir/game_hands.c" "$task_dir/test_game_hands.c" -lm -o "$task_dir/test_game_hands"
"$task_dir/test_game_hands"
cc -std=c99 -Wall -Wextra -Werror -pedantic -fsanitize=undefined \
  "$task_dir/weapon.c" "$task_dir/test_weapon.c" -lm -o "$task_dir/test_weapon"
"$task_dir/test_weapon"
python "$task_dir/test_rigs.py"
