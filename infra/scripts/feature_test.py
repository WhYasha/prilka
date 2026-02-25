#!/usr/bin/env python3
"""Targeted API tests for Self-chat, Delete chat, and Forward with attribution."""
import json, urllib.request, urllib.error, random, string, sys, time

BASE = "https://api.behappy.rest"
PASS = "SmokeTest@2026"

OK   = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"

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
        with urllib.request.urlopen(r, timeout=15) as resp:
            raw = resp.read()
            if not raw or raw.strip() == b"":
                return resp.status, {}
            return resp.status, _parse_json(raw)
    except urllib.error.HTTPError as e:
        raw = e.read()
        if not raw or raw.strip() == b"":
            return e.code, {}
        return e.code, _parse_json(raw)
    except urllib.error.URLError as e:
        return 0, {"_raw": str(e.reason)}

def check(label, cond, detail=""):
    sym = OK if cond else FAIL
    suffix = "  (" + detail + ")" if detail else ""
    print("  " + sym + " " + label + suffix)
    results.append(cond)
    return cond

def rand_suffix():
    return "".join(random.choices(string.ascii_lowercase + string.digits, k=8))

print("\n" + "="*60)
print("  Targeted Feature Tests")
print("="*60)

# -- Setup: Register and login a test user --
suffix = rand_suffix()
TEST_USER = "featuretest_" + suffix
TEST_EMAIL = "featuretest_" + suffix + "@example.com"

print("\n[Setup] Register + Login test user: " + TEST_USER)
s, b = req("POST", "/auth/register", {"username": TEST_USER, "email": TEST_EMAIL, "password": PASS})
if s not in (201, 409):
    print("  FATAL: Could not register user, status=" + str(s) + " body=" + str(b))
    sys.exit(1)
print("  Registered: status=" + str(s))

s, b = req("POST", "/auth/login", {"username": TEST_USER, "password": PASS})
if s != 200:
    print("  FATAL: Could not login, status=" + str(s) + " body=" + str(b))
    sys.exit(1)
token = b.get("access_token", "")
my_user_id = b.get("user_id", 0)
print("  Logged in: user_id=" + str(my_user_id))

# Also register a second user for group chat operations
TEST_USER_B = "featuretest_b_" + suffix
TEST_EMAIL_B = "featuretest_b_" + suffix + "@example.com"
s, b = req("POST", "/auth/register", {"username": TEST_USER_B, "email": TEST_EMAIL_B, "password": PASS})
s, b = req("POST", "/auth/login", {"username": TEST_USER_B, "password": PASS})
token_b = b.get("access_token", "")
user_b_id = b.get("user_id", 0)
print("  Second user: user_id=" + str(user_b_id))


# ============================================================================
# TEST 1: Self-chat / Saved Messages
# ============================================================================
print("\n" + "-"*60)
print("[1] SELF-CHAT / SAVED MESSAGES")
print("-"*60)

# 1a. Create a self-chat by posting own user_id in member_ids
print("\n  1a. Create self-chat")
s, b = req("POST", "/chats", {"type": "direct", "member_ids": [my_user_id]}, token=token)
check("POST /chats (self-chat) -> 200 or 201", s in (200, 201),
      "status=" + str(s) + " chat_id=" + str(b.get("id")) + " type=" + str(b.get("type")))
self_chat_id = b.get("id")

# 1b. Verify creating it again returns the same chat (idempotent)
print("\n  1b. Verify idempotency")
s2, b2 = req("POST", "/chats", {"type": "direct", "member_ids": [my_user_id]}, token=token)
check("POST /chats (self-chat again) -> returns same chat",
      s2 in (200, 201) and b2.get("id") == self_chat_id,
      "status=" + str(s2) + " chat_id=" + str(b2.get("id")) + " expected=" + str(self_chat_id))

# 1c. Send a message to the self-chat
print("\n  1c. Send message to self-chat")
SELF_MSG = "Saved message " + suffix
s, b = req("POST", "/chats/" + str(self_chat_id) + "/messages",
           {"content": SELF_MSG, "type": "text"}, token=token)
check("POST message to self-chat -> 201", s == 201,
      "status=" + str(s) + " msg_id=" + str(b.get("id")))
self_msg_id = b.get("id")

# 1d. Verify the self-chat appears in /chats list
print("\n  1d. Verify self-chat in chat list")
s, b = req("GET", "/chats", token=token)
self_chat_in_list = None
for c in (b if isinstance(b, list) else []):
    if c.get("id") == self_chat_id:
        self_chat_in_list = c
        break
check("GET /chats -> self-chat appears in list", self_chat_in_list is not None,
      "found=" + str(self_chat_in_list is not None))

# 1e. Verify it is a direct chat with member_count=1
print("\n  1e. Verify member_count=1")
if self_chat_in_list:
    is_direct = self_chat_in_list.get("type") == "direct"
    mc = self_chat_in_list.get("member_count")
    check("Self-chat is direct with member_count=1",
          is_direct and mc == 1,
          "type=" + str(self_chat_in_list.get("type")) + " member_count=" + str(mc))
else:
    check("Self-chat is direct with member_count=1", False, "self-chat not found in list")

# 1f. Verify messages can be read back
print("\n  1f. Verify message retrieval")
s, b = req("GET", "/chats/" + str(self_chat_id) + "/messages", token=token)
found_msg = any(m.get("id") == self_msg_id for m in (b if isinstance(b, list) else []))
check("GET self-chat messages -> contains our message", s == 200 and found_msg,
      "status=" + str(s) + " found=" + str(found_msg) + " count=" + str(len(b) if isinstance(b, list) else 0))

# 1g. Verify self-chat detail via GET /chats/{id}
print("\n  1g. Get self-chat detail")
s, b = req("GET", "/chats/" + str(self_chat_id), token=token)
check("GET /chats/{id} (self-chat) -> 200", s == 200,
      "status=" + str(s) + " member_count=" + str(b.get("member_count")) + " type=" + str(b.get("type")))


# ============================================================================
# TEST 2: Delete Chat
# ============================================================================
print("\n" + "-"*60)
print("[2] DELETE CHAT")
print("-"*60)

# 2a. Create a group chat to delete
print("\n  2a. Create a group chat")
GROUP_NAME = "Delete Test " + suffix
s, b = req("POST", "/chats", {"type": "group", "name": GROUP_NAME, "member_ids": [user_b_id]}, token=token)
check("POST /chats (group) -> 201", s == 201,
      "status=" + str(s) + " chat_id=" + str(b.get("id")))
delete_chat_id = b.get("id")

# 2b. Send a message to it
print("\n  2b. Send message to group")
s, b = req("POST", "/chats/" + str(delete_chat_id) + "/messages",
           {"content": "Message before delete " + suffix, "type": "text"}, token=token)
check("POST message to group -> 201", s == 201,
      "status=" + str(s) + " msg_id=" + str(b.get("id")))

# 2c. Verify it is in the chat list
print("\n  2c. Verify chat exists in list")
s, b = req("GET", "/chats", token=token)
found_before = any(c.get("id") == delete_chat_id for c in (b if isinstance(b, list) else []))
check("GET /chats -> group in list before delete", s == 200 and found_before,
      "found=" + str(found_before))

# 2d. Delete the chat
print("\n  2d. Delete the chat")
s, b = req("DELETE", "/chats/" + str(delete_chat_id), token=token)
check("DELETE /chats/{id} -> 204 or 200", s in (200, 204),
      "status=" + str(s) + " body=" + str(b)[:80])

# 2e. Verify it is gone from the list
print("\n  2e. Verify chat is gone from list")
s, b = req("GET", "/chats", token=token)
found_after = any(c.get("id") == delete_chat_id for c in (b if isinstance(b, list) else []))
check("GET /chats -> group NOT in list after delete", s == 200 and not found_after,
      "still_found=" + str(found_after))

# 2f. Verify GET /chats/{id} returns 404 or 403
print("\n  2f. Verify direct access returns error")
s, b = req("GET", "/chats/" + str(delete_chat_id), token=token)
check("GET /chats/{id} after delete -> 403 or 404", s in (403, 404),
      "status=" + str(s))


# ============================================================================
# TEST 3: Forward with Attribution
# ============================================================================
print("\n" + "-"*60)
print("[3] FORWARD WITH ATTRIBUTION")
print("-"*60)

# 3a. Create a source chat (direct with user B)
print("\n  3a. Create source DM chat")
s, b = req("POST", "/chats", {"type": "direct", "member_ids": [user_b_id]}, token=token)
check("POST /chats (direct with user B) -> 200/201", s in (200, 201),
      "status=" + str(s) + " chat_id=" + str(b.get("id")))
source_chat_id = b.get("id")

# 3b. Create a target group chat
print("\n  3b. Create target group chat")
TARGET_GROUP = "Forward Test " + suffix
s, b = req("POST", "/chats", {"type": "group", "name": TARGET_GROUP, "member_ids": [user_b_id]}, token=token)
check("POST /chats (target group) -> 201", s == 201,
      "status=" + str(s) + " chat_id=" + str(b.get("id")))
target_chat_id = b.get("id")

# 3c. Send a message in source chat (this is what we will forward)
print("\n  3c. Send message in source chat")
FWD_CONTENT = "Message to forward " + suffix
s, b = req("POST", "/chats/" + str(source_chat_id) + "/messages",
           {"content": FWD_CONTENT, "type": "text"}, token=token)
check("POST message in source chat -> 201", s == 201,
      "status=" + str(s) + " msg_id=" + str(b.get("id")))
source_msg_id = b.get("id")

# 3d. Forward the message to target chat
print("\n  3d. Forward message to target chat")
s, b = req("POST", "/chats/" + str(target_chat_id) + "/messages/forward",
           {"from_chat_id": source_chat_id, "message_ids": [source_msg_id]}, token=token)
check("POST forward -> 200 with array", s == 200 and isinstance(b, list) and len(b) > 0,
      "status=" + str(s) + " count=" + str(len(b) if isinstance(b, list) else 0))

fwd_msg = b[0] if isinstance(b, list) and b else {}

# 3e. Verify forwarded_from_user_id
print("\n  3e. Verify forwarded_from_user_id")
has_fwd_user_id = fwd_msg.get("forwarded_from_user_id") == my_user_id
check("Forwarded msg has forwarded_from_user_id == sender",
      has_fwd_user_id,
      "forwarded_from_user_id=" + str(fwd_msg.get("forwarded_from_user_id")) + " expected=" + str(my_user_id))

# 3f. Verify forwarded_from_display_name
print("\n  3f. Verify forwarded_from_display_name")
fwd_display_name = fwd_msg.get("forwarded_from_display_name")
has_fwd_display_name = fwd_display_name is not None and fwd_display_name != ""
check("Forwarded msg has forwarded_from_display_name",
      has_fwd_display_name,
      "forwarded_from_display_name=" + str(fwd_display_name))

# 3g. Verify other forward fields too
print("\n  3g. Verify forwarded_from_chat_id and forwarded_from_message_id")
has_fwd_chat = fwd_msg.get("forwarded_from_chat_id") == source_chat_id
has_fwd_msg = fwd_msg.get("forwarded_from_message_id") == source_msg_id
check("Forwarded msg has from_chat_id + from_message_id",
      has_fwd_chat and has_fwd_msg,
      "from_chat=" + str(fwd_msg.get("forwarded_from_chat_id")) + " from_msg=" + str(fwd_msg.get("forwarded_from_message_id")))

# 3h. Verify forwarded message appears when listing target chat messages
print("\n  3h. Verify forwarded message in target chat message list")
s, b = req("GET", "/chats/" + str(target_chat_id) + "/messages", token=token)
fwd_in_list = None
for m in (b if isinstance(b, list) else []):
    if m.get("forwarded_from_message_id") == source_msg_id:
        fwd_in_list = m
        break
check("GET target chat messages -> forwarded msg found with attribution",
      fwd_in_list is not None
      and fwd_in_list.get("forwarded_from_user_id") == my_user_id
      and fwd_in_list.get("forwarded_from_display_name") is not None,
      "found=" + str(fwd_in_list is not None)
      + " user_id=" + str(fwd_in_list.get("forwarded_from_user_id") if fwd_in_list else None)
      + " display_name=" + str(fwd_in_list.get("forwarded_from_display_name") if fwd_in_list else None))

# 3i. Forward from user B to verify different user attribution
print("\n  3i. Forward a message from user B (cross-user attribution)")
# User B sends a message in source chat
s, b = req("POST", "/chats/" + str(source_chat_id) + "/messages",
           {"content": "User B message " + suffix, "type": "text"}, token=token_b)
if s == 201:
    user_b_msg_id = b.get("id")
    # User A forwards user B's message to target
    s, b = req("POST", "/chats/" + str(target_chat_id) + "/messages/forward",
               {"from_chat_id": source_chat_id, "message_ids": [user_b_msg_id]}, token=token)
    fwd_b_msg = b[0] if isinstance(b, list) and b else {}
    check("Forward user B msg -> forwarded_from_user_id == user_b",
          s == 200 and fwd_b_msg.get("forwarded_from_user_id") == user_b_id,
          "forwarded_from_user_id=" + str(fwd_b_msg.get("forwarded_from_user_id")) + " expected=" + str(user_b_id))
    check("Forward user B msg -> has display_name of user B",
          fwd_b_msg.get("forwarded_from_display_name") is not None and fwd_b_msg.get("forwarded_from_display_name") != "",
          "display_name=" + str(fwd_b_msg.get("forwarded_from_display_name")))
else:
    check("Forward user B msg -> forwarded_from_user_id == user_b", False,
          "user B could not send msg, status=" + str(s))
    check("Forward user B msg -> has display_name of user B", False, "skipped")


# -- Summary --
passed = sum(results)
total  = len(results)
print("\n" + "="*60)
print("  Result: " + str(passed) + "/" + str(total) + " checks passed")
if passed == total:
    print("  ALL TESTS PASSED")
else:
    print("  " + str(total - passed) + " FAILURE(S)")
print("="*60 + "\n")
sys.exit(0 if passed == total else 1)
