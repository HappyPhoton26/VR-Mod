#!/usr/bin/env python3
"""Launch the original development renderer + native headset compositor."""
from pathlib import Path
import os
import fcntl
import signal
import struct
import subprocess
import sys
import time

ROOT=Path('/home/steamos/cod2-vr-work')
GAME=Path('/home/steamos/.local/share/Steam/steamapps/common/Call of Duty 2')
sys.path.insert(0,str(ROOT/'tools'))
from game_command import send as game_command
from steam_cdp import evaluate
from hide_flat_panel import expression as hide_expression


def running(executable):
    for entry in Path('/proc').iterdir():
        if not entry.name.isdigit():continue
        try:
            if (entry/'exe').resolve()==executable:return int(entry.name)
            if executable.name=='CoD2SP_s.exe' and (entry/'comm').read_text().strip()=='CoD2SP_s.exe':return int(entry.name)
        except (FileNotFoundError,PermissionError,ProcessLookupError):pass
    return None


def tracking_advances(timeout=3):
    def sequence():
        try:
            with (ROOT/'transport/pose.bin').open('rb') as f:
                magic,version,seq,_=struct.unpack('<4I',f.read(16))
            return seq if magic==0x43565250 and version==1 else None
        except (OSError,struct.error):return None
    initial=sequence()
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        time.sleep(.1)
        current=sequence()
        if current is not None and current!=initial:return True
    return False


def main():
    resolution=os.environ.get('COD2VR_RESOLUTION','1920x1080')
    samples=os.environ.get('COD2VR_MSAA','2')
    if resolution not in ('1280x720','1920x1080','2560x1440','2880x1620') or samples not in ('1','2','4'):
        raise ValueError('Use a supported COD2VR_RESOLUTION (1280x720, 1920x1080, 2560x1440, 2880x1620) and COD2VR_MSAA (1, 2, 4).')
    (GAME/'cod2vr-mode.txt').write_text('2\n')
    (GAME/'cod2vr-hands.txt').write_text('1\n')
    (GAME/'cod2vr-fire.txt').write_text('1\n')
    viewer=ROOT/'stereo/game_viewer'
    viewer_pid=running(viewer)
    viewer_env=None
    if viewer_pid and not tracking_advances():
        # A surviving process can be stuck in the runtime after a game restart.
        try:
            raw=Path(f'/proc/{viewer_pid}/environ').read_bytes().decode()
            viewer_env=dict(item.split('=',1) for item in raw.split('\0') if '=' in item)
            os.kill(viewer_pid,signal.SIGTERM)
            deadline=time.monotonic()+3
            while running(viewer) and time.monotonic()<deadline:time.sleep(.1)
            if running(viewer):os.kill(viewer_pid,signal.SIGKILL)
        except ProcessLookupError:pass
        viewer_pid=None
    if not viewer_pid:
        log=(ROOT/'stereo/game-viewer.log').open('w')
        subprocess.Popen([str(viewer)],cwd=ROOT,env=viewer_env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
        log.close()
    if not tracking_advances(timeout=10):
        raise RuntimeError('Headset compositor is not publishing live tracking; inspect game-viewer.log')
    if not running(GAME/'CoD2SP_s.exe'):
        print(f'COD2 VR render settings: {resolution}, r_aaSamples={samples}',flush=True)
        evaluate(f'SteamClient.Apps.SetAppResolutionOverride(2630,"{resolution}")')
        args=['steam','-applaunch','2630','+set','monkeytoy','0',
              '+set','r_mode',resolution,'+set','r_fullscreen','0',
              '+set','cg_drawCrosshair','0','+set','developer','1','+set','r_smp_backend','0',
              '+set','r_glow','0','+set','r_aasamples',samples,'+map','demolition']
        subprocess.run(args,check=True,stdout=subprocess.DEVNULL)
    # Engine IPC avoids routing physical keyboard events into the chat/system UI.
    # Wait for actual fresh game eye frames before hiding the desktop panel.
    initial=None
    try:
        with (ROOT/'transport/frame.bin').open('rb') as f:initial=struct.unpack('<8I',f.read(32))[2]
    except (OSError,struct.error):pass
    started=time.monotonic();last_briefing_click=started
    end=started+120
    while time.monotonic()<end:
        try:
            with (ROOT/'transport/frame.bin').open('rb') as f:
                header=struct.unpack('<8I',f.read(32))
            if header[0]==0x43565246 and header[2]!=initial and not header[2]&1:
                for command in ['hud_fade_compass 30','cg_drawHUD 1','cg_drawCrosshair 0']:
                    try:game_command(command,timeout=2)
                    except (OSError,RuntimeError,TimeoutError) as error:print('Game command:',error,file=sys.stderr)
                evaluate(hide_expression)
                print('Original CoD2 VR: game eye frames and headset tracking active; flat panel hidden.')
                return 0
        except (OSError,struct.error,StopIteration):pass
        # This development launch explicitly starts a test mission. Its loading
        # page requires a mouse click even after all assets have loaded.
        if time.monotonic()-started>20 and time.monotonic()-last_briefing_click>10:
            try:game_command('@start_mission',timeout=.25)
            except (OSError,RuntimeError,TimeoutError):pass
            last_briefing_click=time.monotonic()
        time.sleep(.5)
    print('Game eye frames not confirmed; inspect renderer/viewer logs.',file=sys.stderr)
    return 1


if __name__=='__main__':
    with (ROOT/'launch.lock').open('a') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:
            print('Call of Duty 2 VR is already starting.')
            raise SystemExit(0)
        raise SystemExit(main())
