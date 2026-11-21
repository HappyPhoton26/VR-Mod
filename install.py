"""Install only original VR components, retaining the user's game renderer."""
from pathlib import Path
import argparse
import hashlib
import os
import platform
import shutil
import subprocess
import sys
import time

PACKAGE = Path(__file__).resolve().parent
TARGET = Path('/home/steamos/cod2-vr-work')
GAME = Path.home() / '.local/share/Steam/steamapps/common/Call of Duty 2'
ORIGINAL = '5b364adca51d945e3af2ec321be0646b38cb61ad129849939aca548d4eefa584'
PREVIOUS_MODS = (
    '503acbe705b1370e9137c25e657dec1f154c80a08221f2fe6a4514d147a6c22b',
    'f44797045ff6ddbdd286af7ef666181ddcfbece9ef9e22717db5e4e13660089c',
    'ecd0c6e31ef7613aba79a2cd42fccc6fb5f60cda30668a81a67f18351b6f58e2',
    '5ff9a6179bd8ae58a357b869a39b459c41381eb08237d7682148777c23860884',
    '314e5e4d6c783ac22e10944fd32e2a35efa615c3b51bd5e06d44fe9a9f7a65c1',
    '8bb32ea1d39b50d332d15346c224b904b3d5c5c20e59e3932747f4d7a1bfad32',
    'c2cfc2f455f1c967fa36962495074533978965a58643dbe56001ed0a91d721a1',
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Validate without installing or launching')
    parser.add_argument('--install-only', action='store_true')
    args = parser.parse_args()
    if platform.machine() != 'aarch64' or Path.home() != Path('/home/steamos'):
        raise RuntimeError('This build requires Steam Frame ARM64 with /home/steamos. See README.md.')
    if not (GAME / 'CoD2SP_s.exe').is_file():
        raise RuntimeError(f'Install your own Steam copy of Call of Duty 2 first: {GAME}')
    for line in (PACKAGE / 'SHA256SUMS').read_text().splitlines():
        expected, name = line.split('  ', 1)
        if digest(PACKAGE / name) != expected:
            raise RuntimeError(f'Package checksum mismatch: {name}')
    renderer = GAME / 'gfx_d3d_x86_s.dll'
    original = GAME / 'gfx_cod2vr_original.dll'
    mod = PACKAGE / 'payload/mod/gfx_d3d_x86_s.dll'
    if original.exists() and digest(original) != ORIGINAL:
        raise RuntimeError('Existing original-renderer backup has an unsupported checksum.')
    installed_renderer = digest(renderer)
    if installed_renderer not in (ORIGINAL, *PREVIOUS_MODS, digest(mod)):
        raise RuntimeError('Unsupported or modified game renderer; refusing to overwrite it.')
    if installed_renderer != ORIGINAL and not original.exists():
        raise RuntimeError('Original game renderer is missing. Verify Steam game files first.')
    if not Path('/opt/steamvr/bin/linuxarm64/libopenxr_loader.so').exists():
        raise RuntimeError('Steam Frame OpenXR loader is missing.')
    libraries = subprocess.run(['ldd', str(PACKAGE / 'payload/stereo/game_viewer')],
                               check=True, capture_output=True, text=True)
    if 'not found' in libraries.stdout:
        raise RuntimeError('A viewer dependency is missing: ' + libraries.stdout)
    if args.check:
        print('Package checksums, platform and installed game renderer validated. No files changed.')
        return
    for entry in Path('/proc').iterdir():
        try:
            if (entry / 'comm').read_text().strip() in ('CoD2SP_s.exe', 'game_viewer'):
                raise RuntimeError('Close Call of Duty 2 and its VR viewer before installing.')
        except (OSError, UnicodeError):
            pass
    # Keep each overwritten file in a dated local backup; never ship these files.
    backup = TARGET / 'install-backups' / str(time.time_ns())
    destinations = [(f, TARGET / f.relative_to(PACKAGE / 'payload'))
                    for f in (PACKAGE / 'payload').rglob('*') if f.is_file() and 'mod' not in f.relative_to(PACKAGE / 'payload').parts]
    destinations += [(f, GAME / f.name) for f in (PACKAGE / 'payload/mod').iterdir()]
    for index, (_, target) in enumerate(destinations):
        if target.exists():
            saved = backup / str(index) / target.name
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(target, saved)
    if not original.exists():
        shutil.copy2(renderer, original)
    for source, target in destinations:
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    (TARGET / 'transport').mkdir(exist_ok=True)
    desktop = Path.home() / 'Desktop/Call of Duty 2 VR.desktop'
    if desktop.exists():
        backup.mkdir(parents=True, exist_ok=True)
        shutil.copy2(desktop, backup / desktop.name)
    desktop.parent.mkdir(exist_ok=True)
    desktop.write_text('[Desktop Entry]\nType=Application\nName=Call of Duty 2 VR\n'
                       f'Exec={TARGET}/launch-desktop.sh\nPath={TARGET}\n'
                       'Terminal=false\nIcon=steam_icon_2630\nCategories=Game;\n')
    desktop.chmod(0o755)
    print(f'VR mod installed. Local backups: {backup}')
    if not args.install_only:
        os.execv(str(TARGET / 'launch-desktop.sh'), [str(TARGET / 'launch-desktop.sh')])


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'COD2 VR: {error}', file=sys.stderr)
        raise SystemExit(1)
