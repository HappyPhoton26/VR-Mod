"""Small local CDP client for authorized Steam UI inspection/actions."""
import base64
import json
import os
import socket
import struct
import urllib.parse
import urllib.request


def evaluate(expression,title='SharedJSContext'):
    pages=json.load(urllib.request.urlopen('http://127.0.0.1:8080/json',timeout=3))
    page=next(p for p in pages if p.get('title')==title)
    url=urllib.parse.urlsplit(page['webSocketDebuggerUrl'])
    sock=socket.create_connection((url.hostname,url.port),timeout=5)
    key=base64.b64encode(os.urandom(16)).decode()
    sock.sendall((f'GET {url.path} HTTP/1.1\r\nHost: {url.netloc}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n').encode())
    handshake=bytearray()
    while b'\r\n\r\n' not in handshake:handshake.extend(sock.recv(1))
    if b' 101 ' not in handshake:raise RuntimeError(handshake.decode())
    payload=json.dumps({'id':1,'method':'Runtime.evaluate','params':{'expression':expression,'returnByValue':True,'awaitPromise':True}}).encode()
    mask=os.urandom(4);size=len(payload)
    header=bytes([0x81,0x80|size]) if size<126 else bytes([0x81,0xfe])+struct.pack('>H',size)
    sock.sendall(header+mask+bytes(b^mask[i%4] for i,b in enumerate(payload)))
    def read(n):
        data=bytearray()
        while len(data)<n:
            block=sock.recv(n-len(data))
            if not block:raise EOFError()
            data.extend(block)
        return bytes(data)
    while True:
        first,second=read(2);length=second&127
        if length==126:length=struct.unpack('>H',read(2))[0]
        elif length==127:length=struct.unpack('>Q',read(8))[0]
        masked=read(4) if second&128 else None
        body=read(length)
        if masked:body=bytes(b^masked[i%4] for i,b in enumerate(body))
        if first&15==8:raise EOFError()
        if first&15!=1:continue
        result=json.loads(body)
        if result.get('id')==1:
            sock.close();return result


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('expression');parser.add_argument('--title',default='SharedJSContext');args=parser.parse_args()
    print(json.dumps(evaluate(args.expression,args.title),indent=2))
