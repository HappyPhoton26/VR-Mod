# Installing COD2 VR on a Steam Frame

This build only runs on a **Steam Frame** (ARM64, user `steamos`). It does not
work on Windows, a Steam Deck or other headsets.

## Before you start

1. Install **Call of Duty 2** from your Steam library (app 2630) in the default
   Steam library folder. Launch it once in flat mode and quit, so its files exist.
2. Switch the Frame to **Desktop Mode**.
3. Make sure **Steam and SteamVR are running** and the headset is tracking.
4. Close Call of Duty 2 and any running COD2 VR viewer.

## Option A: one click (download the ZIP)

1. On GitHub, open the repo and choose **Code → Download ZIP**. The repo is
   private, so you need to be signed in to an account that has access.
2. Open `Downloads` in Dolphin, right-click the ZIP and choose
   **Extract → Extract archive here**.
3. Open the extracted folder and double-click **Install COD2 VR**.
   If KDE asks whether to run or trust it, choose **Execute** or **Continue**.
4. A terminal window opens, checks the files, installs the mod and starts VR.

If double-clicking opens a text editor instead of running it, the ZIP lost the
"executable" setting. Open a terminal in the folder and run:

```sh
chmod +x Install-on-Steam-Frame.sh Play-COD2-VR.sh "Install COD2 VR.desktop"
./Install-on-Steam-Frame.sh
```

## Option B: one command (git clone, easy updates)

This needs an SSH key added to a GitHub account that can see the repo. Open
Konsole and paste:

```sh
git clone git@github.com:HappyPhoton26/VR-Mod.git ~/VR-Mod && ~/VR-Mod/Install-on-Steam-Frame.sh
```

To **update** later, double-click **Install COD2 VR** in `~/VR-Mod` again. It
pulls the newest version from GitHub before installing.

## Playing

After the first install, use the **Call of Duty 2 VR** icon on the desktop.
Steam and SteamVR must be running first. The launcher starts the
`demolition` mission at 1920×1080 per eye with 2× MSAA by default.

- Menu/View button: game menu
- Right bumper: reload
- Grips: pick up nearby items. Hold the rifle's foregrip with the left hand.
- Left trigger or left bumper: hold to look through the equipped sniper rifle's scope.
- Hold B to look through binoculars when the mission provides them. B goes
  back in menus.
- D-pad Up: ready a grenade, then squeeze with your throwing hand, point that controller where you want the grenade to go, and release its grip to throw.

## Checking without installing

```sh
./Play-COD2-VR.sh --check                    # validate files without pulling or installing
./Install-on-Steam-Frame.sh --install-only   # install without launching VR
```

## Problems

| Message | Fix |
|---|---|
| `requires Steam Frame ARM64` | You are not on a Steam Frame, or not logged in as `steamos`. |
| `Install your own Steam copy of Call of Duty 2 first` | Install the game from Steam into the default library. |
| `Unsupported or modified game renderer` | In Steam: Call of Duty 2 → Properties → Installed Files → **Verify integrity**, then install again. |
| `Close Call of Duty 2 and its VR viewer` | Quit the game and viewer, then run the installer again. |
| `OpenXR loader is missing` | Start SteamVR once, then try again. |
| `Package checksum mismatch` | The download is incomplete or was changed. Download it again. |

## Uninstall (back to flat play)

1. Close the game and the VR viewer.
2. In `~/.local/share/Steam/steamapps/common/Call of Duty 2`, delete
   `gfx_d3d_x86_s.dll`, `cod2vr_input.dll` and `cod2vr_controller_probe.dll`.
3. Rename `gfx_cod2vr_original.dll` to `gfx_d3d_x86_s.dll`.
   You can also run Steam's **Verify integrity** to restore the stock renderer.
4. Delete **Call of Duty 2 VR** from the desktop.

The installer keeps copies of every file it replaces in
`~/cod2-vr-work/install-backups/`.
