"""Admin authentication decorator.

Checks Flask session for a logged-in user, then queries PostgreSQL
to verify the user has is_admin = TRUE.
"""

from functools import wraps
from flask import session, redirect, url_for, abort, request
from pg import pg_cursor


def admin_required(f):
    """Decorator: require the current session user to be a PostgreSQL admin.

    - Not logged in → redirect to /admin/login
    - Logged in but not admin → 403
    - Attaches PG user dict to request.admin_user on success
    """
    @wraps(f)
    def decorated(*args, **kwargs):
        username = session.get("username")
        if not username:
            return redirect(url_for("admin.admin_login", next=request.path))

        with pg_cursor() as cur:
            cur.execute(
                "SELECT id, username, is_admin FROM users "
                "WHERE username = %s AND is_active = TRUE",
                (username,)
            )
            user = cur.fetchone()

        if not user or not user["is_admin"]:
            abort(403)

        request.admin_user = user
        return f(*args, **kwargs)

    return decorated
