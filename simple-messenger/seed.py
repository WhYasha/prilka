"""
Seed script – creates two demo users and a few sample messages.
Run AFTER the DB is initialised (app.py does that automatically).

Usage:  python seed.py
"""

from werkzeug.security import generate_password_hash
from db import get_db, init_db

DEMO_USERS = [
    ("alice", "password123"),
    ("bob",   "password123"),
]

DEMO_MESSAGES = [
    ("alice", "bob",   "Hey Bob! How's it going?"),
    ("bob",   "alice", "Hey Alice! All good here. You?"),
    ("alice", "bob",   "Great, thanks! Want to test this messenger?"),
    ("bob",   "alice", "Sure, looks pretty cool!"),
]


def seed():
    init_db()

    with get_db() as conn:
        user_ids = {}

        for username, password in DEMO_USERS:
            existing = conn.execute(
                "SELECT id FROM users WHERE username = ?", (username,)
            ).fetchone()

            if existing:
                user_ids[username] = existing["id"]
                print(f"  User '{username}' already exists (id={existing['id']}), skipping.")
            else:
                pw_hash = generate_password_hash(password)
                cur = conn.execute(
                    "INSERT INTO users (username, password_hash) VALUES (?, ?)",
                    (username, pw_hash)
                )
                user_ids[username] = cur.lastrowid
                print(f"  Created user '{username}' (id={cur.lastrowid})")

        for from_name, to_name, content in DEMO_MESSAGES:
            conn.execute(
                "INSERT INTO messages (from_user_id, to_user_id, content) VALUES (?, ?, ?)",
                (user_ids[from_name], user_ids[to_name], content)
            )
        print(f"  Inserted {len(DEMO_MESSAGES)} demo messages.")

    print("\nSeed complete.")
    print("  alice / password123")
    print("  bob   / password123")


if __name__ == "__main__":
    seed()
