"""
Seed script – creates 3 demo users, sticker pack, conversations and sample messages.
Run AFTER migration:  python migrate.py && python seed.py
Or on fresh install:  python seed.py  (init_db() is called automatically)
"""

import os
from werkzeug.security import generate_password_hash
from db import get_db, init_db

DEMO_USERS = [
    ("alice", "password123", "Alice Wonder"),
    ("bob",   "password123", "Bob Builder"),
    ("carol", "password123", "Carol Stream"),
]

STICKER_PACK = [
    ("emoji", "wave",     "stickers/wave.svg"),
    ("emoji", "heart",    "stickers/heart.svg"),
    ("emoji", "thumbsup", "stickers/thumbsup.svg"),
    ("emoji", "laugh",    "stickers/laugh.svg"),
    ("emoji", "cool",     "stickers/cool.svg"),
    ("emoji", "fire",     "stickers/fire.svg"),
    ("emoji", "party",    "stickers/party.svg"),
    ("emoji", "sad",      "stickers/sad.svg"),
]

DEMO_MESSAGES = [
    # (from_username, to_username, type, content)
    ("alice", "bob",   "text",    "Hey Bob! How's it going?"),
    ("bob",   "alice", "text",    "Hey Alice! All good here. You?"),
    ("alice", "bob",   "text",    "Great, thanks! Want to test this messenger?"),
    ("bob",   "alice", "text",    "Sure, looks pretty cool!"),
    ("alice", "bob",   "sticker", "wave"),    # label -> sticker_id resolved below
    ("bob",   "alice", "sticker", "heart"),
    ("carol", "alice", "text",    "Hi Alice! Carol here."),
    ("alice", "carol", "text",    "Hey Carol, welcome!"),
]


def get_or_create_conv(conn, uid_a, uid_b):
    a, b = min(uid_a, uid_b), max(uid_a, uid_b)
    row = conn.execute(
        "SELECT id FROM conversations WHERE user_a_id=? AND user_b_id=?", (a, b)
    ).fetchone()
    if row:
        return row["id"]
    cur = conn.execute(
        "INSERT INTO conversations (user_a_id, user_b_id) VALUES (?, ?)", (a, b)
    )
    return cur.lastrowid


def seed():
    init_db()

    with get_db() as conn:
        # --- Users ---
        user_ids = {}
        for username, password, display_name in DEMO_USERS:
            existing = conn.execute(
                "SELECT id FROM users WHERE username=?", (username,)
            ).fetchone()
            if existing:
                user_ids[username] = existing["id"]
                print(f"  User '{username}' already exists (id={existing['id']}), updating display_name.")
                conn.execute(
                    "UPDATE users SET display_name=? WHERE id=?",
                    (display_name, existing["id"])
                )
            else:
                pw_hash = generate_password_hash(password)
                cur = conn.execute(
                    "INSERT INTO users (username, password_hash, display_name) VALUES (?, ?, ?)",
                    (username, pw_hash, display_name)
                )
                user_ids[username] = cur.lastrowid
                print(f"  Created user '{username}' (id={cur.lastrowid})")

            # Ensure settings row
            conn.execute(
                "INSERT OR IGNORE INTO user_settings (user_id) VALUES (?)",
                (user_ids[username],)
            )

        # --- Stickers ---
        sticker_ids = {}
        for pack_name, label, file_path in STICKER_PACK:
            existing = conn.execute(
                "SELECT id FROM stickers WHERE label=? AND pack_name=?", (label, pack_name)
            ).fetchone()
            if existing:
                sticker_ids[label] = existing["id"]
                print(f"  Sticker '{label}' already exists")
            else:
                cur = conn.execute(
                    "INSERT INTO stickers (pack_name, label, file_path) VALUES (?, ?, ?)",
                    (pack_name, label, file_path)
                )
                sticker_ids[label] = cur.lastrowid
                print(f"  Created sticker '{label}' (id={cur.lastrowid})")

        # --- Messages ---
        msg_count = 0
        for from_name, to_name, msg_type, content in DEMO_MESSAGES:
            from_id = user_ids[from_name]
            to_id   = user_ids[to_name]
            cid = get_or_create_conv(conn, from_id, to_id)

            if msg_type == "sticker":
                store_content = str(sticker_ids.get(content, 1))
            else:
                store_content = content

            conn.execute(
                "INSERT INTO messages (conversation_id, from_user_id, content, message_type) VALUES (?, ?, ?, ?)",
                (cid, from_id, store_content, msg_type)
            )
            conn.execute(
                "UPDATE conversations SET updated_at=CURRENT_TIMESTAMP WHERE id=?", (cid,)
            )
            msg_count += 1

        print(f"  Inserted {msg_count} demo messages.")

    print("\nSeed complete. Login credentials:")
    for username, password, _ in DEMO_USERS:
        print(f"  {username} / {password}")


if __name__ == "__main__":
    seed()
