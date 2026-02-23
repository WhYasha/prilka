import sqlite3
import os

DB_PATH = os.environ.get("DB_PATH", "messenger.db")

# Ensure the data directory exists (when using a Docker volume at /app/data)
os.makedirs(os.path.dirname(os.path.abspath(DB_PATH)), exist_ok=True)


def get_db():
    """Open a database connection with row_factory set to Row."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


def init_db():
    """Create tables from schema.sql if they don't exist, then seed users."""
    schema_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "schema.sql")
    with get_db() as conn:
        with open(schema_path, "r") as f:
            conn.executescript(f.read())
    print(f"Database initialised at: {DB_PATH}")
    _seed_users()


def _seed_users():
    """Insert seed users if they don't already exist.

    Seed list: alice, bob, carol with password 'testpass'.
    Uses Werkzeug's generate_password_hash so Flask can verify them.
    Only inserts rows that are not already present (idempotent).
    """
    from werkzeug.security import generate_password_hash
    seeds = [
        ("alice", "testpass"),
        ("bob",   "testpass"),
        ("carol", "testpass"),
    ]
    with get_db() as conn:
        for username, password in seeds:
            exists = conn.execute(
                "SELECT 1 FROM users WHERE username = ?", (username,)
            ).fetchone()
            if not exists:
                conn.execute(
                    "INSERT INTO users (username, password_hash, display_name) VALUES (?, ?, ?)",
                    (username, generate_password_hash(password), username.capitalize()),
                )
    print("Seed users ensured.")
