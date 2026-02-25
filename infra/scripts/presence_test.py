#!/usr/bin/env python3
"""End-to-end test for presence active/away behavior."""
import asyncio, json, sys, os, urllib.request, urllib.error, time

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

# ── Setup: two users ─────────────────────────────────────────────────────
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

# Create a fresh group chat for this test run.
# A fresh chat_id guarantees a fresh Redis subscriber on the server,
# avoiding stale subscriber issues from long-running server processes.
ts = int(time.time())
s, b = api("POST", "/chats", {
    "type": "group",
    "name": f"presence_test_{ts}",
    "member_ids": [user_b_id],
}, token=token_a)
chat_id = b.get("id", 0)

# Ensure both users have clean privacy settings (reset from any prior crashed run)
api("PUT", "/settings", {"last_seen_visibility": "everyone"}, token=token_a)
api("PUT", "/settings", {"last_seen_visibility": "everyone"}, token=token_b)

print(f"\n  Users: A={user_a_id}, B={user_b_id}, Chat={chat_id}")


async def drain(ws, timeout=1.5):
    """Drain any pending messages."""
    while True:
        try:
            await asyncio.wait_for(ws.recv(), timeout=timeout)
        except asyncio.TimeoutError:
            break


async def recv_presence(ws, user_id, expected_status, timeout=5):
    """Wait for a presence message for user_id with expected status."""
    deadline = asyncio.get_event_loop().time() + timeout
    while True:
        remaining = deadline - asyncio.get_event_loop().time()
        if remaining <= 0:
            return False, "TIMEOUT"
        try:
            raw = await asyncio.wait_for(ws.recv(), timeout=remaining)
            m = json.loads(raw)
            if m.get("type") == "presence" and m.get("user_id") == user_id:
                if m.get("status") == expected_status:
                    return True, m.get("status")
                continue  # stale presence — skip
        except asyncio.TimeoutError:
            return False, "TIMEOUT"


async def expect_no_presence(ws, user_id, unwanted_status, timeout=3):
    """Read ALL messages for the full timeout and verify none match."""
    deadline = asyncio.get_event_loop().time() + timeout
    while True:
        remaining = deadline - asyncio.get_event_loop().time()
        if remaining <= 0:
            return True, "no_broadcast"
        try:
            raw = await asyncio.wait_for(ws.recv(), timeout=remaining)
            m = json.loads(raw)
            if (m.get("type") == "presence"
                    and m.get("user_id") == user_id
                    and m.get("status") == unwanted_status):
                return False, f"GOT_{unwanted_status.upper()}_UNEXPECTEDLY"
        except asyncio.TimeoutError:
            return True, "no_broadcast"


async def close_and_settle(ws, settle=3):
    """Close a WebSocket and wait for disconnect to propagate."""
    await ws.close()
    await asyncio.sleep(settle)


async def prime_redis_pipeline(ws, cid, attempts=5, per_timeout=3):
    """Send typing indicators and wait for a round-trip through Redis.
    Retries to handle the race between subscribe and Redis SUBSCRIBE ack."""
    for attempt in range(1, attempts + 1):
        await ws.send(json.dumps({"type": "typing", "chat_id": cid}))
        deadline = asyncio.get_event_loop().time() + per_timeout
        while True:
            remaining = deadline - asyncio.get_event_loop().time()
            if remaining <= 0:
                break
            try:
                raw = await asyncio.wait_for(ws.recv(), timeout=remaining)
                m = json.loads(raw)
                if m.get("type") == "typing":
                    return True
            except asyncio.TimeoutError:
                break
        if attempt < attempts:
            await asyncio.sleep(1)
    return False


async def run_tests():
    import websockets

    # ── Warmup: flush stale connections from prior test runs ──────────
    print("\n[0] Warmup: flushing stale connections")
    try:
        wf_a = await websockets.connect(WS, open_timeout=5)
        await wf_a.send(json.dumps({"type": "auth", "token": token_a}))
        await asyncio.wait_for(wf_a.recv(), timeout=3)
        wf_b = await websockets.connect(WS, open_timeout=5)
        await wf_b.send(json.dumps({"type": "auth", "token": token_b}))
        await asyncio.wait_for(wf_b.recv(), timeout=3)
        await asyncio.sleep(1)
        await wf_b.close()
        await wf_a.close()
    except Exception as e:
        print(f"  Warmup error (non-fatal): {e}")
    await asyncio.sleep(2)

    # ── Setup: connect A, subscribe to the fresh chat, prime Redis ────
    ws_a = await websockets.connect(WS, open_timeout=5)
    await ws_a.send(json.dumps({"type": "auth", "token": token_a}))
    r = json.loads(await asyncio.wait_for(ws_a.recv(), timeout=3))
    assert r.get("type") == "auth_ok", f"Auth A failed: {r}"
    await ws_a.send(json.dumps({"type": "subscribe", "chat_id": chat_id}))
    await asyncio.wait_for(ws_a.recv(), timeout=3)  # subscribed

    # Allow time for Redis SUBSCRIBE to be fully acknowledged
    await asyncio.sleep(1)
    await drain(ws_a)

    # Prime the Redis pub/sub pipeline with retries
    primed = await prime_redis_pipeline(ws_a, chat_id)
    print(f"  Redis pipeline primed: {primed}")
    if not primed:
        print("  [FATAL] Redis pub/sub pipeline not working — server cannot deliver broadcasts")
        print("  This usually means the API cannot connect to Redis.")
        print("  Check: nomad alloc logs <alloc> api_cpp | grep -i redis")
        # Still run the tests so we get a proper result count, but they'll fail
    await drain(ws_a)

    # ── Test 1: Connect with active=true -> online broadcast ─────────
    print("\n[1] Connect with active=true")
    ws_b = await websockets.connect(WS, open_timeout=5)
    await ws_b.send(json.dumps({"type": "auth", "token": token_b, "active": True}))
    await asyncio.wait_for(ws_b.recv(), timeout=3)  # auth_ok

    ok, status = await recv_presence(ws_a, user_b_id, "online", timeout=8)
    check("User B connects active=true -> A sees online", ok, f"status={status}")
    await close_and_settle(ws_b)
    await drain(ws_a, timeout=2)

    # ── Test 2: Connect with active=false -> NO online broadcast ─────
    print("\n[2] Connect with active=false")
    await drain(ws_a, timeout=2)
    ws_b = await websockets.connect(WS, open_timeout=5)
    await ws_b.send(json.dumps({"type": "auth", "token": token_b, "active": False}))
    await asyncio.wait_for(ws_b.recv(), timeout=3)  # auth_ok

    ok, detail = await expect_no_presence(ws_a, user_b_id, "online")
    check("User B connects active=false -> A does NOT see online", ok, detail)

    # ── Test 3: Send presence_update active -> online broadcast ──────
    print("\n[3] presence_update: away -> active")
    await drain(ws_a)
    await asyncio.sleep(0.5)
    await ws_b.send(json.dumps({"type": "presence_update", "status": "active"}))
    ok, status = await recv_presence(ws_a, user_b_id, "online", timeout=8)
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
    ok, detail = await expect_no_presence(ws_a, user_b_id, "online", timeout=2)
    check("Duplicate active -> no extra broadcast", ok, detail)

    # ── Test 7: away then disconnect -> no double offline ────────────
    print("\n[7] Away then disconnect")
    await drain(ws_a)
    await ws_b.send(json.dumps({"type": "presence_update", "status": "away"}))
    ok, status = await recv_presence(ws_a, user_b_id, "offline")
    check("User B sends away -> A sees offline", ok, f"status={status}")

    await drain(ws_a, timeout=2)
    await close_and_settle(ws_b)
    ok, detail = await expect_no_presence(ws_a, user_b_id, "offline", timeout=3)
    check("Disconnect while away -> no extra offline", ok, detail)

    # ── Test 8: Privacy enforcement ──────────────────────────────────
    print("\n[8] Privacy: last_seen_visibility=nobody")
    api("PUT", "/settings", {"last_seen_visibility": "nobody"}, token=token_b)

    await drain(ws_a)
    ws_b = await websockets.connect(WS, open_timeout=5)
    await ws_b.send(json.dumps({"type": "auth", "token": token_b, "active": True}))
    await asyncio.wait_for(ws_b.recv(), timeout=3)

    ok, detail = await expect_no_presence(ws_a, user_b_id, "online")
    check("Privacy=nobody -> A does NOT see presence", ok, detail)

    # Reset privacy
    api("PUT", "/settings", {"last_seen_visibility": "everyone"}, token=token_b)
    await ws_b.close()
    await ws_a.close()

    # ── Cleanup: delete the temporary group chat ─────────────────────
    api("DELETE", f"/chats/{chat_id}", token=token_a)

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
