# COD2 VR for Steam Frame

Original experimental single-player VR mod for Call of Duty 2 SP 1.3.
Includes mod source, three original mod DLLs, the native ARM64 OpenXR viewer,
and a one-click installer/launcher. No game executable, original renderer,
game assets, extracted rigs, saves, memory dumps, or screenshots are included.
You must own and install the game separately through Steam (app 2630).

## One-click setup

Download/extract this repository, then run `Play-COD2-VR.sh` (choose Execute
in the file manager). If downloaded permissions were lost, run
`chmod +x Play-COD2-VR.sh` first. Close the game and VR viewer before installing.
The script verifies checksums, preserves your original renderer locally,
installs the mod, creates a desktop shortcut, and launches VR.
Later launches use **Call of Duty 2 VR** on the desktop.

Optional: `./Play-COD2-VR.sh --check` validates without changes;
`./Play-COD2-VR.sh --install-only` installs without launching.

## Requirements and limitations

- This prebuilt version targets **Steam Frame ARM64**, user `/home/steamos`,
  with SteamVR at `/opt/steamvr` and the game in the default Steam library.
  Source and binaries currently embed that transport path. This package is
  not a generic Windows or x86 Steam Deck release.
- Steam and SteamVR/OpenXR must already be running with headset tracking.
  The existing launcher expects Steam's local debugging endpoint on port 8080
  to set resolution and hide the flat theater panel.
- Default resolution: 2880x1620; launcher starts the `demolition` mission.
- Menu/View opens menus; right bumper reloads; grips pick up nearby items.
- D-pad Up prepares a frag. Squeeze with the throwing hand, aim, then release
  that grip to throw.
- Squeeze the left grip near the rifle foregrip to steady it with both hands.
  The headset displays the full native HUD, including context-sensitive indicators.
- Stereo, tracking and actual-game scope rendering were exercised locally.
  Recent arm-length, scope, reload and pickup changes still need user testing.
  This is an experimental build; smoothness depends on CPU image transport.

## Files and recovery

`payload/` contains only the mod and launcher. `src/` contains original C/header
sources and existing build scripts; building requires the relevant OpenXR
headers, C compiler, clang and lld-link. `stereo/build_native.sh` builds the
diagnostic viewer, not the shipped game viewer; the game viewer uses
`stereo/game_viewer.c` plus `controllers/actions.c` with OpenXR/EGL/OpenGL.

Installation uses `/home/steamos/cod2-vr-work`. Existing overwritten files are
copied into its `install-backups/` folder. The original game renderer stays
beside the mod as `gfx_cod2vr_original.dll` and is never uploaded.
To return to flat play, close the game/viewer, restore that file as
`gfx_d3d_x86_s.dll`, and remove `cod2vr_input.dll` and
`cod2vr_controller_probe.dll`. Steam Verify Files can restore the stock renderer.
