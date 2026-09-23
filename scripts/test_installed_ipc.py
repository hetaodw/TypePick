# SPDX-License-Identifier: AGPL-3.0-only
"""CI-only real installed server IPC test (synthetic input, no network)."""
import ctypes as c
from ctypes import wintypes as w
import getpass
import json
import os
from pathlib import Path
import struct
import time
import re

if os.environ.get('GITHUB_ACTIONS') != 'true':
    raise SystemExit('This installed-system test is restricted to disposable GitHub CI runners.')
k = c.WinDLL('kernel32', use_last_error=True)
u = c.WinDLL('user32', use_last_error=True)
k.CreateFileW.argtypes = [w.LPCWSTR,w.DWORD,w.DWORD,w.LPVOID,w.DWORD,w.DWORD,w.HANDLE]
k.CreateFileW.restype = w.HANDLE
k.ReadFile.argtypes = [w.HANDLE,w.LPVOID,w.DWORD,c.POINTER(w.DWORD),w.LPVOID]
k.WriteFile.argtypes = k.ReadFile.argtypes
k.CloseHandle.argtypes = [w.HANDLE]
k.SetNamedPipeHandleState.argtypes = [w.HANDLE,c.POINTER(w.DWORD),w.LPVOID,w.LPVOID]
u.FindWindowW.argtypes = [w.LPCWSTR,w.LPCWSTR]
u.FindWindowW.restype = w.HWND
u.IsWindowVisible.argtypes = [w.HWND]
u.GetWindowTextW.argtypes = [w.HWND,w.LPWSTR,c.c_int]
pipe_name = '\\\\.\\pipe\\' + getpass.getuser() + '\\TypePickNamedPipe'
deadline = time.monotonic()+90
while True:
    handle = k.CreateFileW(pipe_name, 0xc0000000, 0, None, 3, 0, None)
    if handle != c.c_void_p(-1).value:
        break
    if time.monotonic() > deadline:
        raise RuntimeError('Installed server did not create its private pipe')
    time.sleep(.25)
mode=w.DWORD(2)
if not k.SetNamedPipeHandleState(handle,c.byref(mode),None,None):
    raise c.WinError(c.get_last_error())

def request(command, wp=0, sid=0, body=''):
    payload=struct.pack('<III',0x8000+command,wp,sid)+body.encode('utf-16-le')
    written=w.DWORD()
    if not k.WriteFile(handle,payload,len(payload),c.byref(written),None):
        raise c.WinError(c.get_last_error())
    buf=c.create_string_buffer(65536); read=w.DWORD()
    if not k.ReadFile(handle,buf,len(buf),c.byref(read),None):
        raise c.WinError(c.get_last_error())
    data=buf.raw[:read.value]
    if len(data)<4:raise RuntimeError('Short IPC response')
    return struct.unpack('<I',data[:4])[0], data[4:].decode('utf-16-le').rstrip('\0')

def key(sid, code):
    # Match the TSF client: one down and one compact-mask key-up per key.
    result=request(4,code,sid)
    request(4,code | (0x4000 << 16),sid)
    return result

results={}
try:
    sid,body=request(2,body='action=session\nsession.client_app=notepad.exe\nsession.client_type=tsf\n.\n')
    if not sid: raise RuntimeError('Session creation failed')
    # A functioning engine can commit text even when the frontend has zero-sized
    # fonts. Check the cold-start frontend configuration and the transmitted style.
    frontend=Path(os.environ['APPDATA'])/'TypePick/build/weasel.yaml'
    if not frontend.is_file():raise RuntimeError('Cold start did not deploy candidate window configuration')
    font=re.search(r'^  font_point:\s*(\d+)', frontend.read_text(encoding='utf-8'), re.M)
    if not font or int(font[1])<=0:raise RuntimeError('Candidate font size is not positive')
    if 'Microsoft YaHei' not in body:raise RuntimeError('Configured candidate font was not sent to the client')
    results['candidate_font_point']=int(font[1])
    results['candidate_style_transmitted']=True
    request(6,sid=sid)
    for ch in 'yanjiu':key(sid,ord(ch))
    time.sleep(.5)
    window=u.FindWindowW('TypePick.Recommendation.0.1',None)
    if window and u.IsWindowVisible(window):raise RuntimeError('AI appeared without committed context')
    results['empty_context']='no AI popup'
    key(sid,0xff1b)
    for ch in 'nihao':key(sid,ord(ch))
    _,body=key(sid,32)
    if 'commit=你好' not in body:
        raise RuntimeError('Expected real Pinyin commit, got: '+body[:1500])
    results['pinyin_commit']='你好'
    # Editing active Pinyin must preserve the preceding committed context.
    key(sid,ord('a'))
    key(sid,ord('='))
    key(sid,ord('-'))
    key(sid,0xff08)
    for ch in 'yanjiu':key(sid,ord(ch))
    deadline=time.monotonic()+5
    label=''
    while time.monotonic()<deadline:
        window=u.FindWindowW('TypePick.Recommendation.0.1',None)
        if window and u.IsWindowVisible(window):
            text=c.create_unicode_buffer(512);u.GetWindowTextW(window,text,512);label=text.value;break
        time.sleep(.05)
    if '研究' not in label:raise RuntimeError('Recommendation popup did not display expected fixture: '+label)
    if '100.0%' not in label:raise RuntimeError('Recommendation popup did not display numeric confidence')
    results['numeric_confidence']='100.0%'
    request(4,ord('u') | (0x4000 << 16),sid)
    if not u.IsWindowVisible(window):raise RuntimeError('Key release hid a valid recommendation')
    _,body=key(sid,0xff09)
    if 'commit=研究' not in body:raise RuntimeError('Tab did not commit expected recommendation: '+body[:1000])
    results['demo_tab_commit']='研究'
    results['preedit_edit_preserves_context']=True
    for ch in 'yanjiu':key(sid,ord(ch))
    deadline=time.monotonic()+5
    while time.monotonic()<deadline and not u.IsWindowVisible(window):time.sleep(.05)
    if not u.IsWindowVisible(window):raise RuntimeError('Tab release lost context for the next word')
    results['real_key_up_flow']='popup and context preserved after letter and Tab release'
    # A second connection catches accidental loss of thread-local request data.
    request(3,sid=sid)
    sid,_=request(2,body='action=session\nsession.client_app=chrome.exe\nsession.client_type=tsf\n.\n')
    for ch in 'yanjiu':request(4,ord(ch),sid)
    time.sleep(.3)
    window=u.FindWindowW('TypePick.Recommendation.0.1',None)
    if window and u.IsWindowVisible(window):raise RuntimeError('AI appeared for disallowed app')
    results['disallowed_app']='no AI popup'
    request(3,sid=sid)
    sid,_=request(2,body='action=session\nsession.client_app=msedge.exe\nsession.client_type=tsf\n.\n')
    caps,_=request(17,sid=sid)
    if caps != 3:raise RuntimeError('Extended host capability negotiation failed')
    for ch in 'tp':key(sid,ord(ch))
    time.sleep(.15)
    if u.IsWindowVisible(window):raise RuntimeError('Unknown scope allowed a custom recommendation')
    key(sid,0xff1b)
    request(17,3,sid,'今天\0')
    for ch in 'tp':key(sid,ord(ch))
    deadline=time.monotonic()+3
    while time.monotonic()<deadline and not u.IsWindowVisible(window):time.sleep(.03)
    label=c.create_unicode_buffer(512);u.GetWindowTextW(window,label,512)
    if '自定义词' not in label.value:raise RuntimeError('Custom term failed: '+label.value)
    _,body=key(sid,0xff09)
    if 'commit=测试项目' not in body:raise RuntimeError('Custom term did not use TSF commit response')
    results['custom_term_tab']='normal IPC commit'
    request(17,1,sid)
    for ch in 'tp':key(sid,ord(ch))
    time.sleep(.15)
    if u.IsWindowVisible(window):raise RuntimeError('Sensitive scope displayed a term')
    _,body=key(sid,0xff09)
    if 'commit=测试项目' in body:raise RuntimeError('Sensitive scope accepted a stale term')
    results['sensitive_and_unknown_scope']='blocked'
    key(sid,0xff1b)
    request(17,3,sid,'现有文字，\n\0')
    for ch in 'nihao':key(sid,ord(ch))
    _,body=key(sid,32)
    if 'commit=你好' not in body:raise RuntimeError('Extended host normal commit failed')
    deadline=time.monotonic()+3
    while time.monotonic()<deadline and not u.IsWindowVisible(window):time.sleep(.03)
    u.GetWindowTextW(window,label,512)
    if '短句补全' not in label.value:raise RuntimeError('Phrase completion not shown: '+label.value)
    _,body=key(sid,0xff09)
    if 'commit=，很高兴认识你' not in body:raise RuntimeError('Phrase Tab did not commit suffix only: '+body[:500])
    results['phrase_completion_tab']='suffix only'
    request(17,3,sid,'移动光标之后\0')
    _,body=key(sid,0xff09)
    if 'commit=' in body:raise RuntimeError('Caret move allowed stale phrase')
    results['caret_move_rejects_completion']=True
    request(3,sid=sid)
    log=Path(os.environ['APPDATA'])/'TypePick/logs/ai-diagnostics.jsonl'
    records=[json.loads(line) for line in log.read_text(encoding='utf-8').splitlines()]
    if not any(r.get('reason')=='empty_context' for r in records):raise RuntimeError('Missing no-context diagnostic')
    if not any(r.get('event')=='request_finished' for r in records):raise RuntimeError('Missing request diagnostic')
    if not any(r.get('event')=='displayed' and r.get('shown') for r in records):raise RuntimeError('Missing popup diagnostic')
    if any(any(key in r for key in ('context','input','candidates','api_key')) for r in records):raise RuntimeError('Sensitive diagnostic field')
    results['ai_diagnostics']='metadata only; empty-context, request and display events verified'
finally:
    k.CloseHandle(handle)
path=Path('build/installer-test-results.json')
path.write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(results,ensure_ascii=True))
