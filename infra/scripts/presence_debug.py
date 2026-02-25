#!/usr/bin/env python3
"""Debug presence test - mirrors smoke_test.py section 12 exactly."""
import json, os, sys, subprocess, tempfile, urllib.request, urllib.error

BASE = os.environ.get("SMOKE_API_URL", "https://api.behappy.rest")
PASS = "SmokeTest@2026"

def req(method, path, body=None, token=None):
    url = BASE + path
    data = json.dumps(body).encode() if body else None
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
    r = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(r, timeout=10) as resp:
            return resp.status, json.loads(resp.read())
    except urllib.error.HTTPError as e:
        body = {}
        try:
            body = json.loads(e.read())
        except Exception:
            pass
        return e.code, body

# Login user A
s, b = req("POST", "/auth/login", {"username": "smoketest_e2e", "password": PASS})
token = b.get("access_token", "")
# Refresh token (like section 5 does)
s2, b2 = req("POST", "/auth/refresh", {"refresh_token": b.get("refresh_token", "")})
if s2 == 200:
    token = b2.get("access_token", token)

# Register + login user B
req("POST", "/auth/register", {"username": "smoketest_e2e_b",
                                "email": "smoketest_b@example.com",
                                "password": PASS})
s, b = req("POST", "/auth/login", {"username": "smoketest_e2e_b", "password": PASS})
token_b = b.get("access_token", "")
user_b_id = b.get("user_id", 0)

# Ensure they share a DM
s, b = req("POST", "/chats", {"type": "direct", "member_ids": [user_b_id]}, token=token)
presence_chat = b.get("id", 0)

print(f"Setup: user_b_id={user_b_id}, chat={presence_chat}")

WS_URL = os.environ.get("SMOKE_WS_URL", "wss://ws.behappy.rest/ws")

# Build the subprocess code exactly like smoke_test.py does
ws_presence_code = "\n".join([
    "import asyncio, json, sys",
    "try:",
    "    import websockets",
    "except ImportError:",
    "    sys.exit(2)",
    "async def test():",
    "    URI = '" + WS_URL + "'",
    "    TOKEN_A = '" + token + "'",
    "    TOKEN_B = '" + token_b + "'",
    "    CHAT_ID = " + str(presence_chat),
    "    USER_B  = " + str(user_b_id),
    "    try:",
    "        ws_a = await websockets.connect(URI, open_timeout=5)",
    "        await ws_a.send(json.dumps({'type':'auth','token':TOKEN_A}))",
    "        r = json.loads(await asyncio.wait_for(ws_a.recv(), timeout=3))",
    "        print('AUTH_A:'+json.dumps(r))",
    "        if r.get('type') != 'auth_ok':",
    "            print('AUTH_A_FAIL'); return",
    "        await ws_a.send(json.dumps({'type':'subscribe','chat_id':CHAT_ID}))",
    "        r = json.loads(await asyncio.wait_for(ws_a.recv(), timeout=3))",
    "        print('SUB:'+json.dumps(r))",
    "        if r.get('type') != 'subscribed':",
    "            print('SUB_FAIL:got '+r.get('type','?')); return",
    "        await asyncio.sleep(1)",
    "        try:",
    "            while True:",
    "                m = await asyncio.wait_for(ws_a.recv(), timeout=0.3)",
    "                print('DRAIN:'+str(m)[:120])",
    "        except asyncio.TimeoutError:",
    "            pass",
    "        ws_b = await websockets.connect(URI, open_timeout=5)",
    "        await ws_b.send(json.dumps({'type':'auth','token':TOKEN_B}))",
    "        r2 = await asyncio.wait_for(ws_b.recv(), timeout=3)",
    "        print('AUTH_B:'+str(r2)[:120])",
    "        got_online = False",
    "        for i in range(5):",
    "            try:",
    "                m = json.loads(await asyncio.wait_for(ws_a.recv(), timeout=3))",
    "                print('RECV_ONLINE_%d:'%i+json.dumps(m))",
    "                if m.get('type')=='presence' and m.get('status')=='online' and m.get('user_id')==USER_B:",
    "                    got_online = True; break",
    "            except asyncio.TimeoutError:",
    "                print('TIMEOUT_ONLINE_%d'%i)",
    "                break",
    "        print('ONLINE:' + ('OK' if got_online else 'FAIL'))",
    "        await ws_b.close()",
    "        got_offline = False",
    "        for i in range(5):",
    "            try:",
    "                m = json.loads(await asyncio.wait_for(ws_a.recv(), timeout=3))",
    "                print('RECV_OFFLINE_%d:'%i+json.dumps(m))",
    "                if m.get('type')=='presence' and m.get('status')=='offline' and m.get('user_id')==USER_B:",
    "                    got_offline = True; break",
    "            except asyncio.TimeoutError:",
    "                print('TIMEOUT_OFFLINE_%d'%i)",
    "                break",
    "        print('OFFLINE:' + ('OK' if got_offline else 'FAIL'))",
    "        await ws_a.close()",
    "    except Exception as e:",
    "        import traceback; traceback.print_exc()",
    "        print('ERR:'+str(e))",
    "asyncio.run(test())",
])

# Find python with websockets (same logic as smoke_test.py)
_py = sys.executable
r = subprocess.run([_py, "-c", "import websockets"], capture_output=True)
if r.returncode != 0:
    _venv = os.path.join(tempfile.gettempdir(), "wsenv")
    _vpy = os.path.join(_venv, "Scripts" if sys.platform == "win32" else "bin", "python")
    if not os.path.exists(_vpy):
        subprocess.run([_py, "-m", "venv", _venv], capture_output=True)
        _pip = os.path.join(_venv, "Scripts" if sys.platform == "win32" else "bin", "pip")
        subprocess.run([_pip, "install", "-q", "websockets"], capture_output=True, timeout=60)
    _ws_py = _vpy
else:
    _ws_py = _py

print(f"Python: {_ws_py}")
r = subprocess.run([_ws_py, "-c", ws_presence_code],
                   capture_output=True, text=True, timeout=30)
print("=== STDOUT ===")
print(r.stdout)
print("=== STDERR ===")
print(r.stderr[:300] if r.stderr else "(none)")
print(f"=== RC: {r.returncode} ===")
