#!/usr/bin/env python3
"""End-to-end smoke test for the Messenger API."""
import json, os, sys, subprocess, urllib.request, urllib.error

BASE  = "https://behappy.rest/api"
PASS  = "SmokeTest@2026"
USER  = "smoketest_e2e"
EMAIL = "smoketest@example.com"

OK   = "\033[32m\u2713\033[0m"
FAIL = "\033[31m\u2717\033[0m"

results = []

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
        return e.code, json.loads(e.read())

def check(label, cond, detail=""):
    sym = OK if cond else FAIL
    suffix = "  (" + detail + ")" if detail else ""
    print("  " + sym + " " + label + suffix)
    results.append(cond)
    return cond

print("\n==========================================")
print("  Messenger E2E Smoke Test")
print("==========================================")

# ── 1. Health ──────────────────────────────────────────────────────────────
print("\n[1] Health")
s, b = req("GET", "/health")
check("GET /api/health -> 200", s == 200, "status=" + str(s))

# ── 2. Register ────────────────────────────────────────────────────────────
print("\n[2] Register")
s, b = req("POST", "/auth/register", {"username": USER, "email": EMAIL, "password": PASS})
if s == 409:
    check("Register (already exists)", True, "conflict — OK")
else:
    check("POST /auth/register -> 201", s == 201, "id=" + str(b.get("id")))

# ── 3. Login ───────────────────────────────────────────────────────────────
print("\n[3] Login")
s, b = req("POST", "/auth/login", {"username": USER, "password": PASS})
check("POST /auth/login -> 200", s == 200, "user_id=" + str(b.get("user_id")))
token   = b.get("access_token", "")
refresh = b.get("refresh_token", "")

# ── 4. Profile ─────────────────────────────────────────────────────────────
print("\n[4] Profile")
s, b = req("GET", "/me", token=token)
check("GET /me -> 200", s == 200, "username=" + str(b.get("username")))

# ── 5. Token refresh ───────────────────────────────────────────────────────
print("\n[5] Token refresh")
s, b = req("POST", "/auth/refresh", {"refresh_token": refresh})
check("POST /auth/refresh -> 200", s == 200, "new token issued")
if s == 200:
    token = b.get("access_token", token)

# ── 6. User search ─────────────────────────────────────────────────────────
print("\n[6] User search")
s, b = req("GET", "/users/search?q=alice", token=token)
check("GET /users/search -> 200", s == 200, str(len(b)) + " result(s)")
alice_id = b[0]["id"] if b else 1

# ── 7. Chats ───────────────────────────────────────────────────────────────
print("\n[7] Chats")
s, b = req("POST", "/chats", {"type": "direct", "member_ids": [alice_id]}, token=token)
check("POST /chats (direct) -> 200/201", s in (200, 201), "chat_id=" + str(b.get("id")))
chat_id = b.get("id")

# Reuse existing E2E group if one exists, otherwise create it
s, b = req("GET", "/chats", token=token)
check("GET /chats -> 200", s == 200, str(len(b)) + " chat(s)")
group_id = None
for c in (b if isinstance(b, list) else []):
    if c.get("type") == "group" and c.get("name") == "E2E Test Group":
        group_id = c["id"]
        break
if group_id:
    check("POST /chats (group) -> reused", True, "chat_id=" + str(group_id))
else:
    s, b = req("POST", "/chats", {"type": "group", "name": "E2E Test Group",
                                   "member_ids": [alice_id]}, token=token)
    check("POST /chats (group) -> 201", s == 201, "chat_id=" + str(b.get("id")))
    group_id = b.get("id")

# ── 8. Messages ────────────────────────────────────────────────────────────
print("\n[8] Messages")
s, b = req("POST", "/chats/" + str(chat_id) + "/messages",
           {"content": "Hello from smoke test!", "type": "text"}, token=token)
check("POST /chats/{id}/messages -> 201", s == 201, "msg_id=" + str(b.get("id")))

s, b = req("GET", "/chats/" + str(chat_id) + "/messages", token=token)
check("GET /chats/{id}/messages -> 200", s == 200, str(len(b)) + " message(s)")

# ── 9. File upload / download ──────────────────────────────────────────────
print("\n[9] File upload / download")

# The presigned URL redirect points to http://minio:9000/... which is a
# Docker-internal hostname — not resolvable from the host.  We only need
# to verify the backend issues a redirect (302/307); we must NOT follow it.
class _NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise urllib.error.HTTPError(req.full_url, code, msg, headers, fp)

def try_download(fid):
    """Return HTTP status for a download attempt (no redirect follow)."""
    opener = urllib.request.build_opener(_NoRedirect)
    r = urllib.request.Request(
        BASE + "/files/" + str(fid) + "/download",
        headers={"Authorization": "Bearer " + token}, method="GET")
    try:
        with opener.open(r, timeout=10) as resp:
            return resp.status
    except urllib.error.HTTPError as e:
        return e.code
    except urllib.error.URLError:
        return 0

# Reuse a previously uploaded file if it still exists
STATE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".smoke_state.json")
cached_file_id = None
try:
    with open(STATE_FILE) as f:
        cached_file_id = json.load(f).get("file_id")
except (OSError, json.JSONDecodeError, KeyError):
    pass

file_id = None
if cached_file_id:
    dl = try_download(cached_file_id)
    if dl in (200, 302, 307):
        file_id = cached_file_id
        check("POST /files -> reused", True, "id=" + str(file_id))
    # else: stale cache, upload a new one below

if not file_id:
    boundary = "SmokeTestBoundary99"
    body = (
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"smoke.txt\"\r\n"
        "Content-Type: text/plain\r\n\r\n"
        "Hello from smoke test file upload!\r\n"
        "--" + boundary + "--\r\n"
    ).encode()
    r = urllib.request.Request(
        BASE + "/files", data=body,
        headers={"Authorization": "Bearer " + token,
                 "Content-Type": "multipart/form-data; boundary=" + boundary},
        method="POST")
    try:
        with urllib.request.urlopen(r, timeout=15) as resp:
            fb = json.loads(resp.read())
            fs = 201
    except urllib.error.HTTPError as e:
        fs = e.code
        fb = json.loads(e.read())
    check("POST /files -> 201", fs == 201,
          "id=" + str(fb.get("id")) + " key=" + str(fb.get("object_key", "?"))[:20])
    file_id = fb.get("id")
    # Cache for next run
    if file_id:
        try:
            with open(STATE_FILE, "w") as f:
                json.dump({"file_id": file_id}, f)
        except OSError:
            pass

if file_id:
    dl_status = try_download(file_id)
    check("GET /files/{id}/download -> redirect/200",
          dl_status in (200, 302, 307), "status=" + str(dl_status))

# ── 10. WebSocket ──────────────────────────────────────────────────────────
print("\n[10] WebSocket")
ws_code = "\n".join([
    "import asyncio, json, sys",
    "try:",
    "    import websockets",
    "except ImportError:",
    "    sys.exit(2)",
    "async def test():",
    "    uri = 'wss://behappy.rest/ws?token=" + token + "'",
    "    try:",
    "        async with websockets.connect(uri, open_timeout=5) as ws:",
    "            print('CONNECTED')",
    "            try:",
    "                msg = await asyncio.wait_for(ws.recv(), timeout=3)",
    "                print('RECV:' + str(msg))",
    "            except asyncio.TimeoutError:",
    "                print('RECV:TIMEOUT')",
    "    except Exception as e:",
    "        print('ERR:' + str(e))",
    "asyncio.run(test())",
])

import tempfile
_py = sys.executable
r = subprocess.run([_py, "-c", ws_code], capture_output=True, text=True, timeout=10)
if r.returncode == 2:  # websockets not installed
    _venv = os.path.join(tempfile.gettempdir(), "wsenv")
    subprocess.run([_py, "-m", "venv", _venv], capture_output=True)
    _pip = os.path.join(_venv, "Scripts" if sys.platform == "win32" else "bin", "pip")
    _vpy = os.path.join(_venv, "Scripts" if sys.platform == "win32" else "bin", "python")
    subprocess.run([_pip, "install", "-q", "websockets"],
                   capture_output=True, timeout=60)
    r = subprocess.run([_vpy, "-c", ws_code],
                       capture_output=True, text=True, timeout=15)
out = r.stdout.strip()
check("WebSocket connect -> accepted", "CONNECTED" in out, out[:80])
check("WebSocket recv (msg or timeout)", "RECV:" in out, out[:80])

# ── 11. Metrics ────────────────────────────────────────────────────────────
print("\n[11] Metrics")
try:
    with urllib.request.urlopen(BASE + "/metrics", timeout=5) as resp:
        ms  = resp.status
        txt = resp.read(120).decode().replace("\n", " ")
        check("GET /api/metrics -> 200", ms == 200, txt[:60])
except Exception as e:
    check("GET /api/metrics -> 200", False, str(e))

# ── Summary ────────────────────────────────────────────────────────────────
passed = sum(results)
total  = len(results)
print("\n==========================================")
print("  Result: " + str(passed) + "/" + str(total) + " checks passed")
print("==========================================\n")
sys.exit(0 if passed == total else 1)
