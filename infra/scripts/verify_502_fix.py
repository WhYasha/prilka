#!/usr/bin/env python3
"""
Verification script for 502 fix on /api/chats/{id}/invites endpoint.

Run this AFTER deploying the backend changes to verify the 502 is resolved.

Investigation findings (subtask-1-3):
- ALL 5 InvitesController endpoints return 502, not just listInvites
- Public endpoints (previewInvite, no auth required) also return 502
- All other controllers (Chats, Auth, Messages, Files, etc.) work fine
- This means the issue is NOT in individual handler error handling
- Root cause is likely: InvitesController fails to register routes, OR
  the deployed binary was built before InvitesController.cpp was added,
  OR the invites DB table is missing causing a crash during init

Recommended debugging steps on the server:
  1. docker logs api_cpp 2>&1 | grep -i "invite\|error\|crash\|exception"
  2. docker exec api_cpp psql -U $DB_USER -d $DB_NAME -c "\\dt invites"
  3. Rebuild and redeploy: docker compose build api_cpp && docker compose up -d api_cpp

Usage:
    python infra/scripts/verify_502_fix.py
"""
import json
import sys
import urllib.request
import urllib.error

BASE = "https://behappy.rest/api"
PASS = "SmokeTest@2026"
USER = "smoketest_e2e"

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
            raw = resp.read()
            return resp.status, json.loads(raw) if raw else {}
    except urllib.error.HTTPError as e:
        raw = e.read()
        try:
            return e.code, json.loads(raw) if raw else {}
        except (json.JSONDecodeError, ValueError):
            return e.code, {"_raw": raw.decode("utf-8", errors="replace")[:200]}
    except urllib.error.URLError as e:
        return 0, {"_err": str(e.reason)}


def check(label, cond, detail=""):
    sym = OK if cond else FAIL
    suffix = "  (" + detail + ")" if detail else ""
    print("  " + sym + " " + label + suffix)
    results.append(cond)
    return cond


print("\n==========================================")
print("  502 Fix Verification — All Invites Endpoints")
print("==========================================")

# Baseline: verify non-invites endpoints work
print("\n[0] Baseline — non-invites endpoints")
s, _ = req("GET", "/health")
check("GET /health -> 200", s == 200, "status=" + str(s))

s, b = req("POST", "/auth/login", {"username": USER, "password": PASS})
check("POST /auth/login -> 200", s == 200, "status=" + str(s))
token = b.get("access_token", "")
if not token:
    print("  FATAL: Could not obtain auth token. Aborting.")
    sys.exit(1)

s, _ = req("GET", "/chats/39", token=token)
check("GET /chats/39 -> non-502", s != 502, "status=" + str(s))

# Test 1: GET /chats/{id}/invites (listInvites, AuthFilter)
print("\n[1] GET /chats/39/invites (valid auth)")
s, b = req("GET", "/chats/39/invites", token=token)
check("No 502", s != 502, "status=" + str(s))
check("Valid status (200/403/404)", s in (200, 403, 404), "status=" + str(s))

# Test 2: GET /chats/{id}/invites without auth
print("\n[2] GET /chats/39/invites (no auth)")
s, _ = req("GET", "/chats/39/invites")
check("Returns 401", s == 401, "status=" + str(s))

# Test 3: GET /chats/{id}/invites with non-existent chat
print("\n[3] GET /chats/999999/invites (non-existent chat)")
s, _ = req("GET", "/chats/999999/invites", token=token)
check("No 502", s != 502, "status=" + str(s))
check("Returns 403/404", s in (403, 404), "status=" + str(s))

# Test 4: POST /chats/{id}/invites (createInvite, AuthFilter)
print("\n[4] POST /chats/999999/invites (create, non-member)")
s, _ = req("POST", "/chats/999999/invites", token=token)
check("No 502", s != 502, "status=" + str(s))

# Test 5: GET /invites/{token}/preview (public, no AuthFilter)
print("\n[5] GET /invites/nonexistent-token/preview (public)")
s, _ = req("GET", "/invites/nonexistent-token/preview")
check("No 502", s != 502, "status=" + str(s))
check("Returns 404", s == 404, "status=" + str(s))

# Test 6: POST /invites/{token}/join (AuthFilter)
print("\n[6] POST /invites/nonexistent-token/join (auth)")
s, _ = req("POST", "/invites/nonexistent-token/join", token=token)
check("No 502", s != 502, "status=" + str(s))
check("Returns 404", s == 404, "status=" + str(s))

# Test 7: DELETE /invites/{token} (revokeInvite, AuthFilter)
print("\n[7] DELETE /invites/nonexistent-token (auth)")
s, _ = req("DELETE", "/invites/nonexistent-token", token=token)
check("No 502", s != 502, "status=" + str(s))
check("Returns 404", s == 404, "status=" + str(s))

# Summary
passed = sum(results)
total = len(results)
no_502 = all(r for r in results)
print("\n==========================================")
print("  Result: " + str(passed) + "/" + str(total) + " checks passed")
if no_502:
    print("  \033[32mAll invites endpoints responding correctly — no 502!\033[0m")
else:
    print("  \033[31m502 still present on some endpoints\033[0m")
    print()
    print("  DIAGNOSIS: If ALL invites endpoints return 502 but other")
    print("  controllers work, the InvitesController is likely not loaded.")
    print("  Check:")
    print("    1. docker logs api_cpp — look for crash/init errors")
    print("    2. Verify 'invites' table exists in production DB")
    print("    3. Rebuild binary: docker compose build api_cpp")
print("==========================================\n")
sys.exit(0 if no_502 else 1)
