"""
migrate.py – Upgrades an existing messenger.db (v1 schema) to v2 in-place.

Run once:  python migrate.py

Safe to re-run on an already-migrated DB (all steps are idempotent).
"""

import sqlite3
import os

DB_PATH = os.environ.get("DB_PATH", "messenger.db")


def col_exists(conn, table, column):
    rows = conn.execute(f"PRAGMA table_info({table})").fetchall()
    return any(r[1] == column for r in rows)


def table_exists(conn, table):
    row = conn.execute(
        "SELECT name FROM sqlite_master WHERE type='table' AND name=?", (table,)
    ).fetchone()
    return row is not None


def migrate():
    if not os.path.exists(DB_PATH):
        print(f"No DB found at {DB_PATH} – nothing to migrate (fresh install will use schema.sql).")
        return

    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = OFF")  # off during migration

    print(f"Migrating {DB_PATH} …")

    # ------------------------------------------------------------------
    # Step 1: Add new columns to users table
    # ------------------------------------------------------------------
    for col, definition in [
        ("display_name", "TEXT"),
        ("bio",          "TEXT"),
        ("avatar_path",  "TEXT"),
    ]:
        if not col_exists(conn, "users", col):
            conn.execute(f"ALTER TABLE users ADD COLUMN {col} {definition}")
            print(f"  + users.{col}")
        else:
            print(f"  = users.{col} already exists")

    # ------------------------------------------------------------------
    # Step 2: Create conversations table
    # ------------------------------------------------------------------
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS conversations (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_a_id  INTEGER NOT NULL REFERENCES users(id),
            user_b_id  INTEGER NOT NULL REFERENCES users(id),
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(user_a_id, user_b_id)
        );
        CREATE INDEX IF NOT EXISTS idx_conversations_users
            ON conversations(user_a_id, user_b_id);
    """)
    print("  + conversations table ready")

    # ------------------------------------------------------------------
    # Step 3: Migrate messages if old schema (has from_user_id / to_user_id)
    # ------------------------------------------------------------------
    if col_exists(conn, "messages", "from_user_id") and not col_exists(conn, "messages", "conversation_id"):
        print("  Migrating messages to conversation-based schema …")

        # Collect all unique user pairs from old messages
        old_msgs = conn.execute(
            "SELECT id, from_user_id, to_user_id, content, created_at FROM messages ORDER BY id"
        ).fetchall()

        # Build conversation map: (min_id, max_id) -> conversation_id
        conv_map = {}
        for msg in old_msgs:
            a = min(msg["from_user_id"], msg["to_user_id"])
            b = max(msg["from_user_id"], msg["to_user_id"])
            if (a, b) not in conv_map:
                cur = conn.execute(
                    "INSERT OR IGNORE INTO conversations (user_a_id, user_b_id) VALUES (?, ?)",
                    (a, b)
                )
                if cur.lastrowid:
                    conv_map[(a, b)] = cur.lastrowid
                else:
                    # already existed
                    row = conn.execute(
                        "SELECT id FROM conversations WHERE user_a_id=? AND user_b_id=?", (a, b)
                    ).fetchone()
                    conv_map[(a, b)] = row["id"]

        print(f"    Created {len(conv_map)} conversations from old messages")

        # Rename old messages table
        conn.execute("ALTER TABLE messages RENAME TO messages_old")

        # Create new messages table
        conn.executescript("""
            CREATE TABLE messages (
                id               INTEGER PRIMARY KEY AUTOINCREMENT,
                conversation_id  INTEGER NOT NULL REFERENCES conversations(id),
                from_user_id     INTEGER NOT NULL REFERENCES users(id),
                content          TEXT,
                message_type     TEXT NOT NULL DEFAULT 'text',
                attachment_path  TEXT,
                duration_seconds INTEGER,
                created_at       DATETIME DEFAULT CURRENT_TIMESTAMP
            );
            CREATE INDEX IF NOT EXISTS idx_messages_conv
                ON messages(conversation_id, id);
        """)

        # Copy data
        for msg in old_msgs:
            a = min(msg["from_user_id"], msg["to_user_id"])
            b = max(msg["from_user_id"], msg["to_user_id"])
            cid = conv_map[(a, b)]
            conn.execute(
                """INSERT INTO messages (id, conversation_id, from_user_id, content, message_type, created_at)
                   VALUES (?, ?, ?, ?, 'text', ?)""",
                (msg["id"], cid, msg["from_user_id"], msg["content"], msg["created_at"])
            )

        conn.execute("DROP TABLE messages_old")
        print(f"    Migrated {len(old_msgs)} messages")
    else:
        print("  = messages already on new schema")

    # ------------------------------------------------------------------
    # Step 4: user_settings table
    # ------------------------------------------------------------------
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS user_settings (
            user_id               INTEGER PRIMARY KEY REFERENCES users(id),
            theme                 TEXT    NOT NULL DEFAULT 'light',
            notifications_enabled INTEGER NOT NULL DEFAULT 1,
            language              TEXT    NOT NULL DEFAULT 'en'
        );
    """)
    # Backfill existing users
    conn.execute("""
        INSERT OR IGNORE INTO user_settings (user_id)
        SELECT id FROM users
    """)
    print("  + user_settings table ready")

    # ------------------------------------------------------------------
    # Step 5: stickers table
    # ------------------------------------------------------------------
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS stickers (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            pack_name TEXT NOT NULL,
            label     TEXT NOT NULL,
            file_path TEXT NOT NULL
        );
    """)
    print("  + stickers table ready")

    conn.commit()
    conn.close()
    print("\nMigration complete.")


if __name__ == "__main__":
    migrate()
