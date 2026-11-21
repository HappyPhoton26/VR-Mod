#!/bin/sh
# One-click COD2 VR install/update for Steam Frame.
# Double-click "Install COD2 VR" (or this file) in Dolphin. It opens a terminal
# window, pulls the latest version if this folder is a git clone, installs the
# mod and launches VR. Extra arguments go to the installer (--check, --install-only).
set -u
package_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

# Started from the file manager: reopen in a terminal so progress and errors show.
if [ ! -t 1 ] && [ -z "${COD2VR_IN_TERMINAL:-}" ] && command -v konsole >/dev/null 2>&1; then
    COD2VR_IN_TERMINAL=1 exec konsole --hold -p tabtitle="COD2 VR install" \
        -e "$package_dir/Install-on-Steam-Frame.sh" "$@"
fi

echo "== Call of Duty 2 VR installer =="
if [ -d "$package_dir/.git" ] && command -v git >/dev/null 2>&1; then
    echo "Checking GitHub for updates..."
    if git -C "$package_dir" pull --ff-only; then
        echo "Up to date."
    else
        echo "Could not update (offline or local changes); installing the version already here."
    fi
fi

chmod +x "$package_dir/Play-COD2-VR.sh" 2>/dev/null || true
"$package_dir/Play-COD2-VR.sh" "$@"
status=$?
if [ "$status" -eq 0 ]; then
    echo
    echo "Done. Next time, start the game with 'Call of Duty 2 VR' on the desktop."
else
    echo
    echo "Install failed (code $status). See the message above and INSTALL.md."
fi
[ -n "${COD2VR_IN_TERMINAL:-}" ] && echo "You can close this window."
exit "$status"
