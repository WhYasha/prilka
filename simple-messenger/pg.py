"""PostgreSQL connection pool for admin panel.

Uses the DATABASE_URL environment variable set in docker-compose.yml.
Connection is lazy-initialized on first use.
"""

import os
import psycopg2
import psycopg2.extras
import psycopg2.pool

_pool = None

DATABASE_URL = os.environ.get("DATABASE_URL", "")


def _get_pool():
    """Lazy-initialize a threaded connection pool (min 1, max 5)."""
    global _pool
    if _pool is None:
        if not DATABASE_URL:
            raise RuntimeError("DATABASE_URL not set; cannot connect to PostgreSQL")
        _pool = psycopg2.pool.ThreadedConnectionPool(
            minconn=1,
            maxconn=5,
            dsn=DATABASE_URL,
        )
    return _pool


def get_pg():
    """Get a PostgreSQL connection from the pool."""
    return _get_pool().getconn()


def put_pg(conn):
    """Return a connection to the pool."""
    _get_pool().putconn(conn)


class pg_cursor:
    """Context manager providing a RealDictCursor with auto commit/rollback.

    Usage:
        with pg_cursor() as cur:
            cur.execute("SELECT id, username FROM users")
            rows = cur.fetchall()
    """

    def __init__(self):
        self.conn = None

    def __enter__(self):
        self.conn = get_pg()
        self.cur = self.conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
        return self.cur

    def __exit__(self, exc_type, exc_val, exc_tb):
        if exc_type is None:
            self.conn.commit()
        else:
            self.conn.rollback()
        self.cur.close()
        put_pg(self.conn)
        return False
