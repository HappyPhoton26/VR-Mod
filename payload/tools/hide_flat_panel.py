"""Undock CoD2's theater panel, then hide the dashboard through Steam's UI.
This preserves the flat game process producing the per-eye VR images.
"""
from steam_cdp import evaluate
import json
import time

expression='''(async()=>{
 const secret=await SteamClient.OpenVR.GetWebSecret();
 await new Promise((resolve,reject)=>{
  const ws=new WebSocket("ws://127.0.0.1:27062?secret="+encodeURIComponent(secret));
  ws.onopen=()=>{
   ws.send("mailbox_open cod2vr_control/native");
   ws.send("mailbox_send vrwebui_dashboardstore "+JSON.stringify({type:"disable_theater_mode"}));
   setTimeout(()=>{ws.close();resolve();},300);
  };
  ws.onerror=()=>reject(new Error("SteamVR control connection failed"));
 });
 SteamClient.OpenVR.VROverlay.HideDashboard();
 return "Theater panel undocked; dashboard hide requested";
})()'''

if __name__=='__main__':
    print(json.dumps(evaluate(expression)))
    time.sleep(.3)
    print(json.dumps(evaluate('vrFrameStore.frames.filter(f=>f.gameAppID===2630).map(f=>({title:f.title,visible:f.isVisible,dock:f.dockLocation}))')))
