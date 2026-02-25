#!/usr/bin/env python3
"""End-to-end test for presence active/away behavior."""
import asyncio, json, sys, os, urllib.request, urllib.error

BASE = os.environ.get("SMOKE_API_URL", "https://api.behappy.rest")
WS   = os.environ.get("SMOKE_WS_URL", "wss://ws.behappy.rest/ws")
PASS = "SmokeTest@2026"

OK   = "\033[32m\u2713\033[0m"
FAIL = "\033[31m\u2717\033[0m"
results = []

def check(label, ok, detail=""):
    results.append(ok)
    mark = OK if ok else FAIL
    print(f"  {mark} {label}  ({detail})")

def api(method, path, body=None, token=None):
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
        try:
            return e.code, json.loads(e.read())
        except Exception:
            return e.code, {}
    except Exception as e:
        return 0, {"error": str(e)}

# ── Setup: two users with a DM ──────────────────────────────────────────
print("=" * 50)
print("  Presence Active/Away Test")
print("=" * 50)

# User A
api("POST", "/auth/register", {"username": "smoketest_e2e", "email": "smoketest@example.com", "password": PASS})
s, b = api("POST", "/auth/login", {"username": "smoketest_e2e", "password": PASS})
token_a = b.get("access_token", "")
user_a_id = b.get("user_id", 0)

# User B
api("POST", "/auth/register", {"username": "smoketest_e2e_b", "email": "smoketest_b@example.com", "password": PASS})
s, b = api("POST", "/auth/login", {"username": "smoketest_e2e_b", "password": PASS})
token_b = b.get("access_token", "")
user_b_id = b.get("user_id", 0)

# Create DM
s, b = api("POST", "/chats", {"type": "direct", "member_ids": [user_b_id]}, token=token_a)
chat_id = b.get("id", 0)

print(f"\n  Users: A={user_a_id}, B={user_b_id}, Chat={chat_id}")


async def drain(ws, timeout=0.3):
    """Drain any pending messages."""
    while True:
        try:
            await asyncio.wait_for(ws.recv(), timeout=timeout)
        except asyncio.TimeoutError:
            break


async def recv_presence(ws, user_id, expected_status, timeout=5):
    """Wait for a presence message for user_id with expected status."""
    for _ in range(10):
        try:
            raw = await asyncio.wait_for(ws.recv(), timeout=timeout)
            m = json.loads(raw)
            if m.get("type") == "presence" and m.get("user_id") == user_id:
                return m.get("status") == expected_status, m.get("status")
        except asyncio.TimeoutError:
            return False, "TIMEOUT"
    return False, "NOT_FOUND"


async def run_tests():
    import websockets

    # Allow stale WS connections (e.g. from a prior smoke_test run) to close
    await asyncio.sleep(2)

    # ── Test 1: Connect with active=true -> online broadcast ─────────
    print("\n[1] Connect with active=true")
    ws_a = await websockets.connect(WS, open_timeout=5)
    await ws_a.send(json.dumps({"type": "auth", "token": token_a}))
    r = json.loads(await asyncio.wait_for(ws_a.recv(), timeout=3))
    assert r.get("type") == "auth_ok", f"Auth A failed: {r}"
    await ws_a.send(json.dumps({"type": "subscribe", "chat_id": chat_id}))
    await asyncio.wait_for(ws_a.recv(), timeout=3)  # subscribed
    await drain(ws_a)

    # User B connects with active=true (default)
    ws_b = await websockets.connect(WS, open_timeout=5)
    await ws_b.send(json.dumps({"type": "auth", "token": token_b, "active": True}))
    await asyncio.wait_for(ws_b.recv(), timeout=3)  # auth_ok

    ok, status = await recv_presence(ws_a, user_b_id, "online")
    check("User B connects active=true -> A sees online", ok, f"status={status}")
    await ws_b.close()
    await asyncio.sleep(1)          # let server process disconnect
    await drain(ws_a, timeout=1)    # consume the offline + any stragglers

    # ── Test 2: Connect with active=false -> NO online broadcast ─────
    print("\n[2] Connect with active=false")
    await drain(ws_a)
    ws_b = await websockets.connect(WS, open_timeout=5)
    await ws_b.send(json.dumps({"type": "auth", "token": token_b, "active": False}))
    await asyncio.wait_for(ws_b.recv(), timeout=3)  # auth_ok

    # Should NOT get an online presence for B
    got_presence = False
    try:
        raw = await asyncio.wait_for(ws_a.recv(), timeout=3)
        m = json.loads(raw)
        if m.get("type") == "presence" and m.get("user_id") == user_b_id and m.get("status") == "online":
            got_presence = True
    except asyncio.TimeoutError:
        pass
    check("User B connects active=false -> A does NOT see online", not got_presence,
          "no_broadcast" if not got_presence else "GOT_ONLINE_UNEXPECTEDLY")

    # ── Test 3: Send presence_update active -> online broadcast ──────
    print("\n[3] presence_update: away -> active")
    await drain(ws_a)
    await ws_b.send(json.dumps({"type": "presence_update", "status": "active"}))
    ok, status = await recv_presence(ws_a, user_b_id, "online")
    check("User B sends active -> A sees online", ok, f"status={status}")

    # ── Test 4: Send presence_update away -> offline broadcast ───────
    print("\n[4] presence_update: active -> away (simulates tab switch)")
    await drain(ws_a)
    await ws_b.send(json.dumps({"type": "presence_update", "status": "away"}))
    ok, status = await recv_presence(ws_a, user_b_id, "offline")
    check("User B sends away -> A sees offline", ok, f"status={status}")

    # ── Test 5: Send presence_update active again -> online again ────
    print("\n[5] presence_update: away -> active (simulates tab return)")
    await drain(ws_a)
    await ws_b.send(json.dumps({"type": "presence_update", "status": "active"}))
    ok, status = await recv_presence(ws_a, user_b_id, "online")
    check("User B sends active -> A sees online again", ok, f"status={status}")

    # ── Test 6: Duplicate active -> no extra broadcast ───────────────
    print("\n[6] Duplicate presence_update (no-op)")
    await drain(ws_a)
    await ws_b.send(json.dumps({"type": "presence_update", "status": "active"}))
    got_dup = False
    try:
        raw = await asyncio.wait_for(ws_a.recv(), timeout=2)
        m = json.loads(raw)
        if m.get("type") == "presence" and m.get("user_id") == user_b_id:
            got_dup = True
    except asyncio.TimeoutError:
        pass
    check("Duplicate active -> no extra broadcast", not got_dup,
          "no_duplicate" if not got_dup else "GOT_DUPLICATE")

    # ── Test 7: away then disconnect -> no double offline ────────────
    print("\n[7] Away then disconnect")
    await drain(ws_a)
    await ws_b.send(json.dumps({"type": "presence_update", "status": "away"}))
    ok, status = await recv_presence(ws_a, user_b_id, "offline")
    check("User B sends away -> A sees offline", ok, f"status={status}")

    await drain(ws_a)
    await ws_b.close()
    await asyncio.sleep(1)
    got_extra_offline = False
    try:
        raw = await asyncio.wait_for(ws_a.recv(), timeout=2)
        m = json.loads(raw)
        if m.get("type") == "presence" and m.get("user_id") == user_b_id and m.get("status") == "offline":
            got_extra_offline = True
    except asyncio.TimeoutError:
        pass
    check("Disconnect while away -> no extra offline", not got_extra_offline,
          "no_extra" if not got_extra_offline else "GOT_EXTRA_OFFLINE")

    # ── Test 8: Privacy enforcement ──────────────────────────────────
    print("\n[8] Privacy: last_seen_visibility=nobody")
    # Set B privacy to nobody
    api("PUT", "/settings", {"last_seen_visibility": "nobody"}, token=token_b)

    await drain(ws_a)
    ws_b = await websockets.connect(WS, open_timeout=5)
    await ws_b.send(json.dumps({"type": "auth", "token": token_b, "active": True}))
    await asyncio.wait_for(ws_b.recv(), timeout=3)

    got_presence = False
    try:
        raw = await asyncio.wait_for(ws_a.recv(), timeout=3)
        m = json.loads(raw)
        if m.get("type") == "presence" and m.get("user_id") == user_b_id:
            got_presence = True
    except asyncio.TimeoutError:
        pass
    check("Privacy=nobody -> A does NOT see presence", not got_presence,
          "hidden" if not got_presence else "LEAKED")

    # Reset privacy
    api("PUT", "/settings", {"last_seen_visibility": "everyone"}, token=token_b)
    await ws_b.close()
    await ws_a.close()

    # ── Summary ──────────────────────────────────────────────────────
    passed = sum(results)
    total = len(results)
    print(f"\n{'=' * 50}")
    print(f"  Result: {passed}/{total} checks passed")
    print(f"{'=' * 50}")
    return passed == total


# Ensure websockets is available
try:
    import websockets
except ImportError:
    import subprocess, tempfile
    _venv = os.path.join(tempfile.gettempdir(), "wsenv")
    subprocess.run([sys.executable, "-m", "venv", _venv], capture_output=True)
    _pip = os.path.join(_venv, "bin", "pip")
    subprocess.run([_pip, "install", "-q", "websockets"], capture_output=True, timeout=60)
    sys.path.insert(0, os.path.join(_venv, "lib", "python3.11", "site-packages"))
    import websockets

ok = asyncio.run(run_tests())
sys.exit(0 if ok else 1)
