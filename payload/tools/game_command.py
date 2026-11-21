"""Send an authorized engine command through the guest DLL, without typing."""
import argparse
import fcntl
import mmap
from pathlib import Path
import struct
import time

PATH=Path('/home/steamos/cod2-vr-work/transport/command.bin')
MAGIC=0x4356434d


def send(command,timeout=5):
    text=command.rstrip('\n')+'\n'
    payload=text.encode('ascii')
    if len(payload)>511 or any(x<32 and x not in (9,10,13) for x in payload):
        raise ValueError('Command must be printable ASCII and fit the command map')
    PATH.parent.mkdir(parents=True,exist_ok=True)
    with PATH.open('a+b') as file:
        fcntl.flock(file,fcntl.LOCK_EX)
        file.truncate(528)
        with mmap.mmap(file.fileno(),528) as data:
            magic,version,request,ack=struct.unpack_from('<4I',data)
            if magic!=MAGIC or version!=1:
                data[:]=bytes(528);struct.pack_into('<2I',data,0,MAGIC,1);request=0
            if request!=ack and magic==MAGIC:raise RuntimeError('Previous engine command is still pending')
            next_request=(request+1)&0xffffffff
            data[16:]=payload+bytes(512-len(payload))
            data.flush()
            struct.pack_into('<I',data,8,next_request)
            end=time.monotonic()+timeout
            while time.monotonic()<end:
                if struct.unpack_from('<I',data,12)[0]==next_request:return
                time.sleep(.01)
            raise TimeoutError('Game input DLL has not acknowledged the engine command')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('command',nargs='+');args=parser.parse_args()
    for command in args.command:send(command);print('Queued in game:',command)
