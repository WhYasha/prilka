import sqlite3
import os

DB_PATH = os.environ.get("DB_PATH", "messenger.db")


def get_db():
    """Open a database connection with row_factory set to Row."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


def init_db():
    """Create tables from schema.sql if they don't exist."""
    schema_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "schema.sql")
    with get_db() as conn:
        with open(schema_path, "r") as f:
            conn.executescript(f.read())
    print(f"Database initialised at: {DB_PATH}")
