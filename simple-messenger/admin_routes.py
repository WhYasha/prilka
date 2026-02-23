"""Admin panel Blueprint for BH Messenger.

Provides dashboard stats, user management, message monitoring,
and support messaging — all backed by real PostgreSQL data.
"""

import os
import json
import hashlib
import hmac as _hmac

from flask import (
    Blueprint, request, session, render_template,
    redirect, url_for, abort, jsonify,
)
from admin_auth import admin_required
from pg import pg_cursor

admin_bp = Blueprint(
    "admin",
    __name__,
    template_folder="templates/admin",
    url_prefix="/admin",
)


# ---------------------------------------------------------------------------
# PBKDF2 password verification (matches C++ backend format)
# Format: "pbkdf2$<salt_hex>$<hash_hex>" with 100k iterations, SHA-256
# ---------------------------------------------------------------------------

def _verify_pbkdf2(password, stored):
    """Verify password against C++ backend's PBKDF2-SHA256 stored hash."""
    if not stored or stored.count("$") != 2:
        return False
    prefix, salt_hex, expected_hash = stored.split("$", 2)
    if prefix != "pbkdf2":
        return False
    # C++ passes salt.c_str() (the hex string bytes) to PKCS5_PBKDF2_HMAC
    derived = hashlib.pbkdf2_hmac(
        "sha256", password.encode(), salt_hex.encode(), 100000
    )
    return _hmac.compare_digest(derived.hex(), expected_hash)


# ---------------------------------------------------------------------------
# Admin Login / Logout
# ---------------------------------------------------------------------------

@admin_bp.route("/login", methods=["GET", "POST"])
def admin_login():
    if request.method == "GET":
        return render_template("admin_login.html")

    username = request.form.get("username", "").strip()
    password = request.form.get("password", "")

    if not username or not password:
        return render_template("admin_login.html", error="Username and password required.")

    with pg_cursor() as cur:
        cur.execute(
            "SELECT id, username, password_hash, is_admin, is_active "
            "FROM users WHERE username = %s",
            (username,),
        )
        user = cur.fetchone()

    if not user or not user["is_active"]:
        return render_template("admin_login.html", error="Invalid credentials.")
    if not user["is_admin"]:
        return render_template("admin_login.html", error="Access denied: not an admin.")
    if not _verify_pbkdf2(password, user["password_hash"]):
        return render_template("admin_login.html", error="Invalid credentials.")

    session.clear()
    session["user_id"] = user["id"]
    session["username"] = user["username"]
    session["is_admin"] = True

    next_url = request.args.get("next", url_for("admin.dashboard"))
    return redirect(next_url)


@admin_bp.route("/logout")
def admin_logout():
    session.clear()
    return redirect(url_for("admin.admin_login"))


# ---------------------------------------------------------------------------
# Dashboard
# ---------------------------------------------------------------------------

@admin_bp.route("/")
@admin_required
def dashboard():
    stats = {}
    with pg_cursor() as cur:
        cur.execute("SELECT COUNT(*) AS cnt FROM users WHERE is_active = TRUE")
        stats["total_users"] = cur.fetchone()["cnt"]

        cur.execute(
            "SELECT COUNT(*) AS cnt FROM users "
            "WHERE last_activity >= NOW() - INTERVAL '24 hours' AND is_active = TRUE"
        )
        stats["dau"] = cur.fetchone()["cnt"]

        cur.execute(
            "SELECT COUNT(*) AS cnt FROM users "
            "WHERE last_activity >= NOW() - INTERVAL '7 days' AND is_active = TRUE"
        )
        stats["wau"] = cur.fetchone()["cnt"]

        cur.execute("SELECT COUNT(*) AS cnt FROM chats")
        stats["total_chats"] = cur.fetchone()["cnt"]

        cur.execute(
            "SELECT type::TEXT, COUNT(*) AS cnt FROM chats GROUP BY type ORDER BY type"
        )
        stats["chats_by_type"] = cur.fetchall()

        cur.execute("SELECT COUNT(*) AS cnt FROM messages")
        stats["total_messages"] = cur.fetchone()["cnt"]

        cur.execute(
            "SELECT COUNT(*) AS cnt FROM messages WHERE created_at >= CURRENT_DATE"
        )
        stats["messages_today"] = cur.fetchone()["cnt"]

        cur.execute(
            "SELECT COUNT(*) AS cnt FROM users "
            "WHERE created_at >= CURRENT_DATE AND is_active = TRUE"
        )
        stats["new_users_today"] = cur.fetchone()["cnt"]

        cur.execute("SELECT COUNT(*) AS cnt FROM users WHERE is_blocked = TRUE")
        stats["blocked_users"] = cur.fetchone()["cnt"]

        # Activity timeline: new users per day (last 14 days)
        cur.execute(
            "SELECT DATE(created_at) AS day, COUNT(*) AS cnt "
            "FROM users "
            "WHERE created_at >= CURRENT_DATE - INTERVAL '14 days' AND is_active = TRUE "
            "GROUP BY day ORDER BY day"
        )
        stats["users_per_day"] = cur.fetchall()

        # Activity timeline: messages per day (last 14 days)
        cur.execute(
            "SELECT DATE(created_at) AS day, COUNT(*) AS cnt "
            "FROM messages "
            "WHERE created_at >= CURRENT_DATE - INTERVAL '14 days' "
            "GROUP BY day ORDER BY day"
        )
        stats["messages_per_day"] = cur.fetchall()

    return render_template("dashboard.html", stats=stats)


# ---------------------------------------------------------------------------
# User Management
# ---------------------------------------------------------------------------

@admin_bp.route("/users")
@admin_required
def user_list():
    page = max(1, int(request.args.get("page", 1)))
    per_page = 50
    offset = (page - 1) * per_page
    search = request.args.get("q", "").strip()
    filter_status = request.args.get("status", "")

    where_clauses = ["TRUE"]
    params = []

    if search:
        where_clauses.append("(u.username ILIKE %s OR u.display_name ILIKE %s)")
        params.extend([f"%{search}%", f"%{search}%"])

    if filter_status == "blocked":
        where_clauses.append("u.is_blocked = TRUE")
    elif filter_status == "admin":
        where_clauses.append("u.is_admin = TRUE")
    elif filter_status == "active":
        where_clauses.append("u.is_active = TRUE AND u.is_blocked = FALSE")
    elif filter_status == "inactive":
        where_clauses.append("u.is_active = FALSE")

    where_sql = " AND ".join(where_clauses)

    with pg_cursor() as cur:
        cur.execute(
            f"SELECT COUNT(*) AS cnt FROM users u WHERE {where_sql}", params
        )
        total = cur.fetchone()["cnt"]

        cur.execute(
            f"""SELECT u.id, u.username, u.display_name, u.email, u.is_active,
                       u.is_admin, u.is_blocked, u.created_at, u.last_activity
                FROM users u
                WHERE {where_sql}
                ORDER BY u.created_at DESC
                LIMIT %s OFFSET %s""",
            params + [per_page, offset],
        )
        users = cur.fetchall()

    total_pages = max(1, (total + per_page - 1) // per_page)
    return render_template(
        "users.html",
        users=users, page=page, total_pages=total_pages,
        total=total, search=search, filter_status=filter_status,
    )


@admin_bp.route("/users/<int:user_id>")
@admin_required
def user_detail(user_id):
    with pg_cursor() as cur:
        cur.execute(
            "SELECT id, username, display_name, email, bio, is_active, "
            "is_admin, is_blocked, created_at, updated_at, last_activity "
            "FROM users WHERE id = %s",
            (user_id,),
        )
        user = cur.fetchone()
        if not user:
            abort(404)

        cur.execute(
            "SELECT COUNT(*) AS cnt FROM messages WHERE sender_id = %s",
            (user_id,),
        )
        message_count = cur.fetchone()["cnt"]

        cur.execute(
            """SELECT c.id, c.type::TEXT AS type, c.name, c.title, cm.role, cm.joined_at,
                      (SELECT COUNT(*) FROM chat_members WHERE chat_id = c.id) AS member_count
               FROM chat_members cm
               JOIN chats c ON c.id = cm.chat_id
               WHERE cm.user_id = %s
               ORDER BY cm.joined_at DESC""",
            (user_id,),
        )
        chats = cur.fetchall()

        cur.execute(
            """SELECT m.id, m.chat_id, m.content, m.message_type, m.created_at,
                      c.name AS chat_name, c.type::TEXT AS chat_type
               FROM messages m
               JOIN chats c ON c.id = m.chat_id
               WHERE m.sender_id = %s
               ORDER BY m.created_at DESC
               LIMIT 50""",
            (user_id,),
        )
        messages = cur.fetchall()

    return render_template(
        "user_detail.html",
        user=user, chats=chats, messages=messages,
        message_count=message_count,
    )


@admin_bp.route("/users/<int:user_id>/block", methods=["POST"])
@admin_required
def block_user(user_id):
    if user_id == request.admin_user["id"]:
        return redirect(url_for("admin.user_detail", user_id=user_id))
    with pg_cursor() as cur:
        cur.execute(
            "UPDATE users SET is_blocked = TRUE WHERE id = %s AND is_blocked = FALSE",
            (user_id,),
        )
    return redirect(url_for("admin.user_detail", user_id=user_id))


@admin_bp.route("/users/<int:user_id>/unblock", methods=["POST"])
@admin_required
def unblock_user(user_id):
    with pg_cursor() as cur:
        cur.execute(
            "UPDATE users SET is_blocked = FALSE WHERE id = %s AND is_blocked = TRUE",
            (user_id,),
        )
    return redirect(url_for("admin.user_detail", user_id=user_id))


@admin_bp.route("/users/<int:user_id>/soft-delete", methods=["POST"])
@admin_required
def soft_delete_user(user_id):
    if user_id == request.admin_user["id"]:
        return redirect(url_for("admin.user_detail", user_id=user_id))
    with pg_cursor() as cur:
        cur.execute(
            "UPDATE users SET is_active = FALSE, is_blocked = TRUE WHERE id = %s",
            (user_id,),
        )
    return redirect(url_for("admin.user_detail", user_id=user_id))


@admin_bp.route("/users/<int:user_id>/toggle-admin", methods=["POST"])
@admin_required
def toggle_admin(user_id):
    if user_id == request.admin_user["id"]:
        return redirect(url_for("admin.user_detail", user_id=user_id))
    with pg_cursor() as cur:
        cur.execute(
            "UPDATE users SET is_admin = NOT is_admin WHERE id = %s",
            (user_id,),
        )
    return redirect(url_for("admin.user_detail", user_id=user_id))


# ---------------------------------------------------------------------------
# Message Monitoring
# ---------------------------------------------------------------------------

@admin_bp.route("/messages")
@admin_required
def message_list():
    page = max(1, int(request.args.get("page", 1)))
    per_page = 100
    offset = (page - 1) * per_page
    filter_user = request.args.get("user_id", "").strip()
    filter_chat = request.args.get("chat_id", "").strip()
    filter_text = request.args.get("q", "").strip()

    where_clauses = ["TRUE"]
    params = []

    if filter_user and filter_user.isdigit():
        where_clauses.append("m.sender_id = %s")
        params.append(int(filter_user))

    if filter_chat and filter_chat.isdigit():
        where_clauses.append("m.chat_id = %s")
        params.append(int(filter_chat))

    if filter_text:
        where_clauses.append("m.content ILIKE %s")
        params.append(f"%{filter_text}%")

    where_sql = " AND ".join(where_clauses)

    with pg_cursor() as cur:
        cur.execute(
            f"SELECT COUNT(*) AS cnt FROM messages m WHERE {where_sql}", params
        )
        total = cur.fetchone()["cnt"]

        cur.execute(
            f"""SELECT m.id, m.chat_id, m.sender_id, m.content, m.message_type,
                       m.created_at,
                       u.username AS sender_username,
                       u.display_name AS sender_display_name,
                       c.name AS chat_name, c.type::TEXT AS chat_type
                FROM messages m
                JOIN users u ON u.id = m.sender_id
                JOIN chats c ON c.id = m.chat_id
                WHERE {where_sql}
                ORDER BY m.created_at DESC
                LIMIT %s OFFSET %s""",
            params + [per_page, offset],
        )
        messages = cur.fetchall()

    total_pages = max(1, (total + per_page - 1) // per_page)
    return render_template(
        "messages.html",
        messages=messages, page=page, total_pages=total_pages,
        total=total, filter_user=filter_user,
        filter_chat=filter_chat, filter_text=filter_text,
    )


# ---------------------------------------------------------------------------
# Support Messaging
# ---------------------------------------------------------------------------

def _get_redis():
    """Get a Redis client for publishing support messages."""
    import redis
    host = os.environ.get("REDIS_HOST", "redis")
    port = int(os.environ.get("REDIS_PORT", "6379"))
    password = os.environ.get("REDIS_PASSWORD", "")
    return redis.Redis(host=host, port=port, password=password, decode_responses=True)


def _publish_to_redis(chat_id, msg_id, sender_id, content, created_at):
    """Publish support message to Redis for real-time WebSocket delivery."""
    payload = json.dumps({
        "type": "message",
        "id": msg_id,
        "chat_id": chat_id,
        "sender_id": sender_id,
        "content": content,
        "message_type": "text",
        "created_at": str(created_at),
    })
    try:
        r = _get_redis()
        r.publish(f"chat:{chat_id}", payload)
    except Exception:
        pass  # Non-critical: message is persisted in DB, user sees on refresh


@admin_bp.route("/support")
@admin_required
def support_page():
    with pg_cursor() as cur:
        # Chats the support user is in
        cur.execute(
            """SELECT c.id, c.type::TEXT AS type, c.name, c.title,
                      (SELECT COUNT(*) FROM chat_members WHERE chat_id = c.id) AS member_count
               FROM chats c
               JOIN chat_members cm ON cm.chat_id = c.id
               JOIN users u ON u.id = cm.user_id AND u.username = 'bh_support'
               ORDER BY c.updated_at DESC"""
        )
        chats = cur.fetchall()

        # Recent support messages
        cur.execute(
            """SELECT m.id, m.chat_id, m.content, m.created_at,
                      c.name AS chat_name, c.type::TEXT AS chat_type
               FROM messages m
               JOIN users u ON u.id = m.sender_id AND u.username = 'bh_support'
               JOIN chats c ON c.id = m.chat_id
               ORDER BY m.created_at DESC
               LIMIT 50"""
        )
        recent_messages = cur.fetchall()

        # All users for target selection
        cur.execute(
            "SELECT id, username, display_name FROM users "
            "WHERE is_active = TRUE AND username != 'bh_support' "
            "ORDER BY username"
        )
        all_users = cur.fetchall()

    return render_template(
        "support.html",
        chats=chats, recent_messages=recent_messages, all_users=all_users,
    )


@admin_bp.route("/support/send", methods=["POST"])
@admin_required
def support_send():
    chat_id = request.form.get("chat_id", "").strip()
    target_user_id = request.form.get("target_user_id", "").strip()
    content = request.form.get("content", "").strip()

    if not content:
        return redirect(url_for("admin.support_page"))

    with pg_cursor() as cur:
        # Get bh_support user ID
        cur.execute("SELECT id FROM users WHERE username = 'bh_support'")
        support_user = cur.fetchone()
        if not support_user:
            abort(500)
        support_id = support_user["id"]

        actual_chat_id = None

        if target_user_id and target_user_id.isdigit():
            target_id = int(target_user_id)
            # Find existing direct chat between support and target
            cur.execute(
                """SELECT c.id FROM chats c
                   JOIN chat_members cm1 ON cm1.chat_id = c.id AND cm1.user_id = %s
                   JOIN chat_members cm2 ON cm2.chat_id = c.id AND cm2.user_id = %s
                   WHERE c.type = 'direct'""",
                (support_id, target_id),
            )
            row = cur.fetchone()
            if row:
                actual_chat_id = row["id"]
            else:
                # Create direct chat
                cur.execute(
                    "INSERT INTO chats (type, name, owner_id) "
                    "VALUES ('direct', NULL, %s) RETURNING id",
                    (support_id,),
                )
                actual_chat_id = cur.fetchone()["id"]
                cur.execute(
                    "INSERT INTO chat_members (chat_id, user_id, role) "
                    "VALUES (%s, %s, 'owner'), (%s, %s, 'member')",
                    (actual_chat_id, support_id, actual_chat_id, target_id),
                )

        elif chat_id and chat_id.isdigit():
            actual_chat_id = int(chat_id)
            # Ensure bh_support is a member
            cur.execute(
                "INSERT INTO chat_members (chat_id, user_id, role) "
                "VALUES (%s, %s, 'admin') ON CONFLICT DO NOTHING",
                (actual_chat_id, support_id),
            )
        else:
            return redirect(url_for("admin.support_page"))

        # Insert message with [SUPPORT] prefix for visibility
        marked_content = f"[SUPPORT] {content}"
        cur.execute(
            "INSERT INTO messages (chat_id, sender_id, content, message_type) "
            "VALUES (%s, %s, %s, 'text') RETURNING id, created_at",
            (actual_chat_id, support_id, marked_content),
        )
        msg_row = cur.fetchone()

        cur.execute(
            "UPDATE chats SET updated_at = NOW() WHERE id = %s",
            (actual_chat_id,),
        )

    # Real-time delivery via Redis pub/sub
    _publish_to_redis(
        actual_chat_id, msg_row["id"], support_id,
        marked_content, msg_row["created_at"],
    )

    return redirect(url_for("admin.support_page"))
