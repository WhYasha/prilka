#!/usr/bin/env python3
"""End-to-end smoke test for the Messenger API."""
import json, os, sys, subprocess, urllib.request, urllib.error

BASE  = os.environ.get("SMOKE_API_URL", "https://api.behappy.rest")
PASS  = "SmokeTest@2026"
USER  = "smoketest_e2e"
EMAIL = "smoketest@example.com"

OK   = "\033[32m\u2713\033[0m"
FAIL = "\033[31m\u2717\033[0m"

results = []

def _parse_json(raw):
    try:
        return json.loads(raw)
    except (json.JSONDecodeError, ValueError):
        return {"_raw": raw.decode("utf-8", errors="replace")[:200] if isinstance(raw, bytes) else str(raw)[:200]}

def req(method, path, body=None, token=None):
    url = BASE + path
    data = json.dumps(body).encode() if body else None
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
    r = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(r, timeout=10) as resp:
            return resp.status, _parse_json(resp.read())
    except urllib.error.HTTPError as e:
        return e.code, _parse_json(e.read())
    except urllib.error.URLError as e:
        return 0, {"_raw": str(e.reason)}

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
s, b = req("GET", "/chats/" + str(chat_id) + "/messages", token=token)
check("GET /chats/{id}/messages -> 200", s == 200, str(len(b)) + " message(s)")

SMOKE_MSG = "Hello from smoke test!"
has_smoke_msg = any(m.get("content") == SMOKE_MSG for m in (b if isinstance(b, list) else []))
if has_smoke_msg:
    check("POST /chats/{id}/messages -> reused", True, "already exists")
else:
    s, b = req("POST", "/chats/" + str(chat_id) + "/messages",
               {"content": SMOKE_MSG, "type": "text"}, token=token)
    check("POST /chats/{id}/messages -> 201", s == 201, "msg_id=" + str(b.get("id")))

# ── 8b. Reply to message ──────────────────────────────────────────────────
print("\n[8b] Reply to message")
# Find the last message to reply to
s, b = req("GET", "/chats/" + str(chat_id) + "/messages?limit=1", token=token)
reply_target_id = None
if s == 200 and isinstance(b, list) and b:
    reply_target_id = b[0].get("id")

if reply_target_id:
    REPLY_MSG = "Reply from smoke test"
    s, b = req("POST", "/chats/" + str(chat_id) + "/messages",
               {"content": REPLY_MSG, "type": "text",
                "reply_to_message_id": reply_target_id}, token=token)
    check("POST reply message -> 201", s == 201 and b.get("reply_to_message_id") == reply_target_id,
          "reply_to=" + str(b.get("reply_to_message_id")))
    reply_msg_id = b.get("id")

    # Fetch messages and verify enrichment
    s, b = req("GET", "/chats/" + str(chat_id) + "/messages?limit=5", token=token)
    reply_msg = None
    for m in (b if isinstance(b, list) else []):
        if m.get("id") == reply_msg_id:
            reply_msg = m
            break
    if reply_msg:
        has_reply_fields = (reply_msg.get("reply_to_message_id") == reply_target_id
                            and reply_msg.get("reply_to_sender_name") is not None)
        check("GET reply msg -> enriched reply fields", has_reply_fields,
              "sender=" + str(reply_msg.get("reply_to_sender_name")))
    else:
        check("GET reply msg -> enriched reply fields", False, "reply msg not found in list")
else:
    check("POST reply message -> 201", False, "no message to reply to")
    check("GET reply msg -> enriched reply fields", False, "skipped")

# ── 8c. Edit message ─────────────────────────────────────────────────────
print("\n[8c] Edit message")
# Find a text message to edit (from our smoke user)
s, b = req("GET", "/chats/" + str(chat_id) + "/messages?limit=10", token=token)
edit_msg_id = None
for m in (b if isinstance(b, list) else []):
    if m.get("message_type") == "text" and m.get("content") == SMOKE_MSG:
        edit_msg_id = m.get("id")
        break
if not edit_msg_id:
    # Fallback: use any text message
    for m in (b if isinstance(b, list) else []):
        if m.get("message_type") == "text":
            edit_msg_id = m.get("id")
            break

EDITED_CONTENT = "Hello from smoke test (edited)!"
if edit_msg_id:
    # Edit the message
    s, b = req("PUT", "/chats/" + str(chat_id) + "/messages/" + str(edit_msg_id),
               {"content": EDITED_CONTENT}, token=token)
    check("PUT edit message -> 200", s == 200 and b.get("is_edited") == True,
          "is_edited=" + str(b.get("is_edited")) + " updated_at=" + str(b.get("updated_at", "?"))[:20])

    # Verify via GET
    s, b = req("GET", "/chats/" + str(chat_id) + "/messages?limit=10", token=token)
    edited_msg = None
    for m in (b if isinstance(b, list) else []):
        if m.get("id") == edit_msg_id:
            edited_msg = m
            break
    check("GET edited message -> is_edited + new content",
          edited_msg is not None and edited_msg.get("is_edited") == True
          and edited_msg.get("content") == EDITED_CONTENT,
          "content=" + str(edited_msg.get("content", "?"))[:40] if edited_msg else "not found")

    # Try editing a non-owned message (should fail)
    # Use a message from alice (if any)
    other_msg_id = None
    for m in (b if isinstance(b, list) else []):
        if m.get("sender_id") != b[0].get("sender_id") if b else None:
            other_msg_id = m.get("id")
            break
    if other_msg_id:
        s, b = req("PUT", "/chats/" + str(chat_id) + "/messages/" + str(other_msg_id),
                   {"content": "hacked!"}, token=token)
        check("PUT edit non-owned message -> 403", s == 403, "status=" + str(s))
    else:
        # No non-owned message — try editing with invalid ID
        s, b = req("PUT", "/chats/" + str(chat_id) + "/messages/999999999",
                   {"content": "hacked!"}, token=token)
        check("PUT edit non-existent message -> 403", s == 403, "status=" + str(s))
else:
    check("PUT edit message -> 200", False, "no text message to edit")
    check("GET edited message -> is_edited + new content", False, "skipped")
    check("PUT edit non-owned message -> 403", False, "skipped")

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

# ── 10. Presigned URL ─────────────────────────────────────────────────────
print("\n[10] Presigned URL")
s, b = req("GET", "/stickers", token=token)
check("GET /stickers -> 200", s == 200, str(len(b)) + " sticker(s)")
if isinstance(b, list) and b:
    sticker_url = b[0].get("url", "")
    has_encoded_cred = "%2F" in sticker_url or "%2f" in sticker_url
    check("Presigned URL has %2F encoding", has_encoded_cred, sticker_url[:80])
    try:
        r = urllib.request.Request(sticker_url, method="GET")
        with urllib.request.urlopen(r, timeout=10) as resp:
            ps = resp.status
            ct = resp.headers.get("Content-Type", "")
    except urllib.error.HTTPError as e:
        ps = e.code
        ct = ""
    check("Presigned sticker fetch -> 200", ps == 200,
          "status=" + str(ps) + " type=" + ct)

# ── 10b. Voice message ───────────────────────────────────────────────────
print("\n[10b] Voice message")

# Upload a tiny audio file (WAV header — valid but silent)
import struct
wav_header = struct.pack('<4sI4s4sIHHIIHH4sI',
    b'RIFF', 36 + 16000, b'WAVE',           # RIFF header (1s of 8kHz 16-bit mono PCM)
    b'fmt ', 16, 1, 1, 8000, 16000, 2, 16,  # fmt chunk
    b'data', 16000)                           # data chunk header
wav_data = wav_header + b'\x00' * 16000       # 1 second of silence

boundary_v = "VoiceBoundary42"
voice_body = (
    "--" + boundary_v + "\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"voice.wav\"\r\n"
    "Content-Type: audio/wav\r\n\r\n"
).encode() + wav_data + ("\r\n--" + boundary_v + "--\r\n").encode()

vr = urllib.request.Request(
    BASE + "/files", data=voice_body,
    headers={"Authorization": "Bearer " + token,
             "Content-Type": "multipart/form-data; boundary=" + boundary_v},
    method="POST")
try:
    with urllib.request.urlopen(vr, timeout=15) as resp:
        vfb = json.loads(resp.read())
        vfs = 201
except urllib.error.HTTPError as e:
    vfs = e.code
    vfb = json.loads(e.read())
voice_file_id = vfb.get("id")
check("POST /files (voice) -> 201", vfs == 201, "id=" + str(voice_file_id))

if voice_file_id and chat_id:
    # Send voice message
    s, b = req("POST", "/chats/" + str(chat_id) + "/messages",
               {"content": "", "type": "voice",
                "file_id": voice_file_id, "duration_seconds": 3}, token=token)
    voice_msg_id = b.get("id") if s == 201 else None
    check("POST voice message -> 201", s == 201 and b.get("message_type") == "voice",
          "msg_id=" + str(voice_msg_id) + " type=" + str(b.get("message_type")))

    if voice_msg_id:
        # Fetch messages and verify voice message fields
        s, b = req("GET", "/chats/" + str(chat_id) + "/messages?limit=5", token=token)
        voice_msg = None
        for m in (b if isinstance(b, list) else []):
            if m.get("id") == voice_msg_id:
                voice_msg = m
                break
        if voice_msg:
            has_voice_fields = (voice_msg.get("message_type") == "voice"
                                and voice_msg.get("duration_seconds") == 3
                                and voice_msg.get("attachment_url") is not None
                                and voice_msg.get("attachment_url") != "")
            check("GET voice msg -> has type/duration/attachment_url", has_voice_fields,
                  "dur=" + str(voice_msg.get("duration_seconds"))
                  + " url=" + str(voice_msg.get("attachment_url", ""))[:50])

            # Fetch the presigned attachment URL
            att_url = voice_msg.get("attachment_url", "")
            if att_url:
                try:
                    ar = urllib.request.Request(att_url, method="GET")
                    with urllib.request.urlopen(ar, timeout=10) as resp:
                        att_status = resp.status
                except urllib.error.HTTPError as e:
                    att_status = e.code
                except urllib.error.URLError:
                    att_status = 0
                check("GET voice attachment_url -> 200", att_status == 200,
                      "status=" + str(att_status))
            else:
                check("GET voice attachment_url -> 200", False, "no url")
        else:
            check("GET voice msg -> has type/duration/attachment_url", False, "msg not found")
            check("GET voice attachment_url -> 200", False, "skipped")
    else:
        check("GET voice msg -> has type/duration/attachment_url", False, "skipped")
        check("GET voice attachment_url -> 200", False, "skipped")
else:
    check("POST voice message -> 201", False, "no file_id or chat_id")
    check("GET voice msg -> has type/duration/attachment_url", False, "skipped")
    check("GET voice attachment_url -> 200", False, "skipped")

# ── 11. WebSocket ──────────────────────────────────────────────────────────
print("\n[11] WebSocket")
ws_code = "\n".join([
    "import asyncio, json, sys",
    "try:",
    "    import websockets",
    "except ImportError:",
    "    sys.exit(2)",
    "async def test():",
    "    uri = '" + os.environ.get("SMOKE_WS_URL", "wss://ws.behappy.rest/ws") + "?token=" + token + "'",
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

# Determine which python has websockets available
try:
    _ws_py = _vpy
except NameError:
    _ws_py = _py

# ── 12. WebSocket Presence ────────────────────────────────────────────────
print("\n[12] WebSocket Presence")

# Register + login a second test user for presence testing
req("POST", "/auth/register", {"username": "smoketest_e2e_b",
                                "email": "smoketest_b@example.com",
                                "password": PASS})
s2, b2 = req("POST", "/auth/login", {"username": "smoketest_e2e_b",
                                       "password": PASS})
token_b = b2.get("access_token", "")
user_b_id = b2.get("user_id", 0)

# Ensure they share a DM
s2, b2 = req("POST", "/chats", {"type": "direct", "member_ids": [user_b_id]},
             token=token)
presence_chat = b2.get("id", chat_id)

if token_b and user_b_id and presence_chat:
    ws_presence_code = "\n".join([
        "import asyncio, json, sys",
        "try:",
        "    import websockets",
        "except ImportError:",
        "    sys.exit(2)",
        "async def test():",
        "    URI = '" + os.environ.get("SMOKE_WS_URL", "wss://ws.behappy.rest/ws") + "'",
        "    TOKEN_A = '" + token + "'",
        "    TOKEN_B = '" + token_b + "'",
        "    CHAT_ID = " + str(presence_chat),
        "    USER_B  = " + str(user_b_id),
        "    try:",
        "        ws_a = await websockets.connect(URI, open_timeout=5)",
        "        await ws_a.send(json.dumps({'type':'auth','token':TOKEN_A}))",
        "        r = json.loads(await asyncio.wait_for(ws_a.recv(), timeout=3))",
        "        if r.get('type') != 'auth_ok':",
        "            print('AUTH_A_FAIL'); return",
        "        await ws_a.send(json.dumps({'type':'subscribe','chat_id':CHAT_ID}))",
        "        r = json.loads(await asyncio.wait_for(ws_a.recv(), timeout=3))",
        "        if r.get('type') != 'subscribed':",
        "            print('SUB_FAIL'); return",
        "        await asyncio.sleep(1)",
        "        try:",
        "            while True:",
        "                await asyncio.wait_for(ws_a.recv(), timeout=0.3)",
        "        except asyncio.TimeoutError:",
        "            pass",
        "        ws_b = await websockets.connect(URI, open_timeout=5)",
        "        await ws_b.send(json.dumps({'type':'auth','token':TOKEN_B}))",
        "        await asyncio.wait_for(ws_b.recv(), timeout=3)",
        "        got_online = False",
        "        for _ in range(5):",
        "            try:",
        "                m = json.loads(await asyncio.wait_for(ws_a.recv(), timeout=3))",
        "                if m.get('type')=='presence' and m.get('status')=='online' and m.get('user_id')==USER_B:",
        "                    got_online = True; break",
        "            except asyncio.TimeoutError:",
        "                break",
        "        print('ONLINE:' + ('OK' if got_online else 'FAIL'))",
        "        await ws_b.close()",
        "        got_offline = False",
        "        for _ in range(5):",
        "            try:",
        "                m = json.loads(await asyncio.wait_for(ws_a.recv(), timeout=3))",
        "                if m.get('type')=='presence' and m.get('status')=='offline' and m.get('user_id')==USER_B:",
        "                    got_offline = True; break",
        "            except asyncio.TimeoutError:",
        "                break",
        "        print('OFFLINE:' + ('OK' if got_offline else 'FAIL'))",
        "        await ws_a.close()",
        "    except Exception as e:",
        "        print('ERR:' + str(e))",
        "asyncio.run(test())",
    ])

    r = subprocess.run([_ws_py, "-c", ws_presence_code],
                       capture_output=True, text=True, timeout=25)
    pout = r.stdout.strip()
    perr = r.stderr.strip()[:80]
    check("Presence online received", "ONLINE:OK" in pout, pout[:80] or perr)
    check("Presence offline received", "OFFLINE:OK" in pout, pout[:80] or perr)
else:
    check("Presence online received", False, "could not set up second user")
    check("Presence offline received", False, "could not set up second user")

# ── 14. Reactions ──────────────────────────────────────────────────────────
print("\n[14] Reactions")

# Find a message ID to react to
s, b = req("GET", "/chats/" + str(chat_id) + "/messages?limit=1", token=token)
react_msg_id = None
if s == 200 and isinstance(b, list) and b:
    react_msg_id = b[0].get("id")

if react_msg_id:
    # Toggle add
    s, b = req("POST", "/chats/" + str(chat_id) + "/messages/" + str(react_msg_id) + "/reactions",
               {"emoji": "\U0001f44d"}, token=token)
    check("POST reaction toggle -> added", s == 200 and b.get("action") == "added",
          "action=" + str(b.get("action")))

    # Batch fetch
    s, b = req("GET", "/chats/" + str(chat_id) + "/reactions?message_ids=" + str(react_msg_id),
               token=token)
    msg_reactions = b.get(str(react_msg_id), []) if isinstance(b, dict) else []
    has_thumbs = any(r.get("emoji") == "\U0001f44d" and r.get("me") for r in msg_reactions)
    check("GET reactions -> has thumbs_up with me=true", s == 200 and has_thumbs,
          str(len(msg_reactions)) + " reaction group(s)")

    # Toggle remove
    s, b = req("POST", "/chats/" + str(chat_id) + "/messages/" + str(react_msg_id) + "/reactions",
               {"emoji": "\U0001f44d"}, token=token)
    check("POST reaction toggle -> removed", s == 200 and b.get("action") == "removed",
          "action=" + str(b.get("action")))

    # Verify empty after removal
    s, b = req("GET", "/chats/" + str(chat_id) + "/reactions?message_ids=" + str(react_msg_id),
               token=token)
    msg_reactions = b.get(str(react_msg_id), []) if isinstance(b, dict) else []
    check("GET reactions -> empty after removal", s == 200 and len(msg_reactions) == 0,
          str(len(msg_reactions)) + " remaining")
else:
    check("POST reaction toggle -> added", False, "no message to react to")
    check("GET reactions -> has thumbs_up with me=true", False, "skipped")
    check("POST reaction toggle -> removed", False, "skipped")
    check("GET reactions -> empty after removal", False, "skipped")

# ── 15. Message Pinning ───────────────────────────────────────────────────
print("\n[15] Message Pinning")

# Find a message to pin
s, b = req("GET", "/chats/" + str(chat_id) + "/messages?limit=1", token=token)
pin_msg_id = None
if s == 200 and isinstance(b, list) and b:
    pin_msg_id = b[0].get("id")

if pin_msg_id:
    # Pin message
    s, b = req("POST", "/chats/" + str(chat_id) + "/messages/" + str(pin_msg_id) + "/pin",
               token=token)
    check("POST pin message -> 200", s == 200 and b.get("pinned_message_id") == pin_msg_id,
          "pinned_message_id=" + str(b.get("pinned_message_id")) if isinstance(b, dict) else "")

    # Get pinned message
    s, b = req("GET", "/chats/" + str(chat_id) + "/pinned-message", token=token)
    pinned_id = b.get("message", {}).get("id") if isinstance(b, dict) and b.get("message") else None
    check("GET pinned-message -> matches", s == 200 and pinned_id == pin_msg_id,
          "message.id=" + str(pinned_id))

    # Unpin message
    s, b = req("DELETE", "/chats/" + str(chat_id) + "/messages/" + str(pin_msg_id) + "/pin",
               token=token)
    check("DELETE unpin message -> 204", s == 204, "status=" + str(s))

    # Verify no pinned message
    s, b = req("GET", "/chats/" + str(chat_id) + "/pinned-message", token=token)
    is_null = b is None or (isinstance(b, dict) and b.get("message") is None) or b == "null"
    check("GET pinned-message -> null after unpin", s == 200 and is_null,
          "body=" + str(b)[:80] if not is_null else "")
else:
    check("POST pin message -> 200", False, "no message to pin")
    check("GET pinned-message -> matches", False, "skipped")
    check("DELETE unpin message -> 204", False, "skipped")
    check("GET pinned-message -> null after unpin", False, "skipped")

# ── 16. Forward messages ──────────────────────────────────────────────────
print("\n[16] Forward messages")

# Send a message to direct chat so we have something to forward
FWD_MSG = "Message to forward"
s, b = req("POST", "/chats/" + str(chat_id) + "/messages",
           {"content": FWD_MSG, "type": "text"}, token=token)
fwd_source_msg_id = b.get("id") if s == 201 else None

if fwd_source_msg_id and group_id:
    # Forward from direct chat to group chat
    s, b = req("POST", "/chats/" + str(group_id) + "/messages/forward",
               {"from_chat_id": chat_id, "message_ids": [fwd_source_msg_id]}, token=token)
    check("POST forward messages -> 200", s == 200 and isinstance(b, list),
          str(len(b)) + " forwarded" if isinstance(b, list) else "status=" + str(s))
    if isinstance(b, list) and b:
        fwd_msg = b[0]
        check("Forwarded msg has forwarded_from fields",
              fwd_msg.get("forwarded_from_chat_id") == chat_id
              and fwd_msg.get("forwarded_from_message_id") == fwd_source_msg_id,
              "from_chat=" + str(fwd_msg.get("forwarded_from_chat_id"))
              + " from_msg=" + str(fwd_msg.get("forwarded_from_message_id")))
    else:
        check("Forwarded msg has forwarded_from fields", False, "no messages returned")
else:
    check("POST forward messages -> 200", False, "no source msg or group")
    check("Forwarded msg has forwarded_from fields", False, "skipped")

# ── 17. Privacy settings ─────────────────────────────────────────────────
print("\n[17] Privacy settings")

# PUT settings with last_seen_visibility = approx_only
s, b = req("PUT", "/settings", {"last_seen_visibility": "approx_only"}, token=token)
check("PUT /settings -> 204", s == 204, "status=" + str(s))

# GET settings and verify round-trip
s, b = req("GET", "/settings", token=token)
check("GET /settings -> 200 with approx_only",
      s == 200 and b.get("last_seen_visibility") == "approx_only",
      "last_seen_visibility=" + str(b.get("last_seen_visibility")) if isinstance(b, dict) else "")

# Restore to default
req("PUT", "/settings", {"last_seen_visibility": "everyone"}, token=token)

# ── 18. Privacy enforcement ──────────────────────────────────────────────
print("\n[18] Privacy enforcement")

# Register user A (privacy_test user)
PRIV_USER = "smoketest_privacy"
PRIV_EMAIL = "smoketest_privacy@example.com"
s, b = req("POST", "/auth/register", {"username": PRIV_USER, "email": PRIV_EMAIL, "password": PASS})
# Login as user A
s, b = req("POST", "/auth/login", {"username": PRIV_USER, "password": PASS})
token_priv = b.get("access_token", "")
priv_user_id = b.get("user_id", 0)

if token_priv and priv_user_id:
    # Set user A's visibility to 'nobody'
    s, b = req("PUT", "/settings", {"last_seen_visibility": "nobody"}, token=token_priv)
    check("Set privacy user to 'nobody' -> 204", s == 204, "status=" + str(s))

    # As user B (smoketest_e2e, non-admin), GET /users/{A.id}
    s, b = req("GET", "/users/" + str(priv_user_id), token=token)
    last_act = b.get("last_activity") if isinstance(b, dict) else "ERR"
    is_online = b.get("is_online") if isinstance(b, dict) else "ERR"
    check("GET user with 'nobody' -> no exact last_activity",
          s == 200 and (last_act is None or last_act == ""),
          "last_activity=" + str(last_act) + " is_online=" + str(is_online))
else:
    check("Set privacy user to 'nobody' -> 204", False, "could not create privacy user")
    check("GET user with 'nobody' -> no exact last_activity", False, "skipped")

# ── 19. Message Search ───────────────────────────────────────────────────
print("\n[19] Message Search")

# Search for a term that should match (smoke test messages contain "smoke")
s, b = req("GET", "/chats/" + str(chat_id) + "/messages/search?q=smoke", token=token)
check("GET /messages/search?q=smoke -> 200", s == 200 and isinstance(b, list),
      str(len(b)) + " result(s)" if isinstance(b, list) else "status=" + str(s))
if isinstance(b, list) and b:
    has_enriched = "sender_username" in b[0] and "sender_display_name" in b[0]
    check("Search results have enriched fields", has_enriched,
          "sender=" + str(b[0].get("sender_username")))
else:
    check("Search results have enriched fields", False, "no results")

# Search for nonexistent term
s, b = req("GET", "/chats/" + str(chat_id) + "/messages/search?q=zzz_nonexistent_zzz", token=token)
check("GET /messages/search?q=nonexistent -> 200 empty", s == 200 and isinstance(b, list) and len(b) == 0,
      str(len(b)) + " result(s)" if isinstance(b, list) else "status=" + str(s))

# Missing q parameter
s, b = req("GET", "/chats/" + str(chat_id) + "/messages/search", token=token)
check("GET /messages/search (no q) -> 400", s == 400, "status=" + str(s))

# Cursor pagination: before_id
s, b = req("GET", "/chats/" + str(chat_id) + "/messages/search?q=smoke&before_id=999999999", token=token)
check("GET /messages/search?before_id -> 200", s == 200 and isinstance(b, list),
      str(len(b)) + " result(s)" if isinstance(b, list) else "status=" + str(s))

# ── 20. Chat Avatar ──────────────────────────────────────────────────────
print("\n[20] Chat Avatar")

if group_id and file_id:
    # Set the avatar on the group chat
    s, b = req("POST", "/chats/" + str(group_id) + "/avatar",
               {"file_id": file_id}, token=token)
    check("POST /chats/{id}/avatar -> 200", s == 200 and isinstance(b, dict) and b.get("avatar_url") is not None,
          "avatar_url=" + str(b.get("avatar_url", ""))[:60] if isinstance(b, dict) else "status=" + str(s))

    # Verify avatar_url in GET /chats
    s, b = req("GET", "/chats", token=token)
    group_in_list = None
    for c in (b if isinstance(b, list) else []):
        if c.get("id") == group_id:
            group_in_list = c
            break
    has_avatar_in_list = group_in_list is not None and group_in_list.get("avatar_url") is not None
    check("GET /chats -> group has avatar_url", s == 200 and has_avatar_in_list,
          "avatar_url=" + str(group_in_list.get("avatar_url", ""))[:60] if group_in_list else "group not found")

    # Verify avatar_url in GET /chats/{id}
    s, b = req("GET", "/chats/" + str(group_id), token=token)
    has_avatar_detail = isinstance(b, dict) and b.get("avatar_url") is not None
    check("GET /chats/{id} -> has avatar_url", s == 200 and has_avatar_detail,
          "avatar_url=" + str(b.get("avatar_url", ""))[:60] if isinstance(b, dict) else "status=" + str(s))

    # Verify member_count in GET /chats
    has_member_count = group_in_list is not None and isinstance(group_in_list.get("member_count"), int) and group_in_list["member_count"] > 0
    check("GET /chats -> group has member_count", has_member_count,
          "member_count=" + str(group_in_list.get("member_count")) if group_in_list else "group not found")
else:
    check("POST /chats/{id}/avatar -> 200", False, "no group or file")
    check("GET /chats -> group has avatar_url", False, "skipped")
    check("GET /chats/{id} -> has avatar_url", False, "skipped")
    check("GET /chats -> group has member_count", False, "skipped")

# ── 13. Metrics ────────────────────────────────────────────────────────────
print("\n[13] Metrics")
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
