"""
Simple Messenger – Flask backend
Run:  python app.py
"""

import os
from flask import (
    Flask, request, session, jsonify,
    render_template, redirect, url_for
)
from werkzeug.security import generate_password_hash, check_password_hash
from db import get_db, init_db

# ---------------------------------------------------------------------------
# App setup
# ---------------------------------------------------------------------------
app = Flask(__name__)
app.secret_key = os.environ.get(
    "SECRET_KEY",
    "change-me-in-production-use-a-long-random-string"
)

# Initialise DB on startup (idempotent – uses CREATE TABLE IF NOT EXISTS)
with app.app_context():
    init_db()


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
MIN_USERNAME = 3
MAX_USERNAME = 32
MIN_PASSWORD = 6
MAX_PASSWORD = 128


def current_user_id():
    return session.get("user_id")


def require_auth():
    """Return (None, error_response) if not logged in, else (user_id, None)."""
    uid = current_user_id()
    if not uid:
        return None, (jsonify({"error": "Unauthorised"}), 401)
    return uid, None


# ---------------------------------------------------------------------------
# Page routes
# ---------------------------------------------------------------------------
@app.route("/")
def index():
    if current_user_id():
        return redirect(url_for("messenger_app"))
    return redirect(url_for("login_page"))


@app.route("/login")
def login_page():
    if current_user_id():
        return redirect(url_for("messenger_app"))
    return render_template("login.html")


@app.route("/register")
def register_page():
    if current_user_id():
        return redirect(url_for("messenger_app"))
    return render_template("register.html")


@app.route("/app")
def messenger_app():
    if not current_user_id():
        return redirect(url_for("login_page"))
    return render_template("app.html")


# ---------------------------------------------------------------------------
# API – Auth
# ---------------------------------------------------------------------------
@app.route("/api/register", methods=["POST"])
def api_register():
    data = request.get_json(silent=True) or {}
    username = (data.get("username") or "").strip()
    password = data.get("password") or ""

    # Validate
    if not (MIN_USERNAME <= len(username) <= MAX_USERNAME):
        return jsonify({"error": f"Username must be {MIN_USERNAME}–{MAX_USERNAME} characters."}), 400
    if not username.replace("_", "").replace("-", "").isalnum():
        return jsonify({"error": "Username may only contain letters, digits, - and _."}), 400
    if not (MIN_PASSWORD <= len(password) <= MAX_PASSWORD):
        return jsonify({"error": f"Password must be {MIN_PASSWORD}–{MAX_PASSWORD} characters."}), 400

    pw_hash = generate_password_hash(password)

    try:
        with get_db() as conn:
            cur = conn.execute(
                "INSERT INTO users (username, password_hash) VALUES (?, ?)",
                (username, pw_hash)
            )
            user_id = cur.lastrowid
    except Exception:
        return jsonify({"error": "Username already taken."}), 409

    session.clear()
    session["user_id"] = user_id
    session["username"] = username
    return jsonify({"id": user_id, "username": username}), 201


@app.route("/api/login", methods=["POST"])
def api_login():
    data = request.get_json(silent=True) or {}
    username = (data.get("username") or "").strip()
    password = data.get("password") or ""

    if not username or not password:
        return jsonify({"error": "Username and password required."}), 400

    with get_db() as conn:
        row = conn.execute(
            "SELECT id, username, password_hash FROM users WHERE username = ?",
            (username,)
        ).fetchone()

    if not row or not check_password_hash(row["password_hash"], password):
        return jsonify({"error": "Invalid credentials."}), 401

    session.clear()
    session["user_id"] = row["id"]
    session["username"] = row["username"]
    return jsonify({"id": row["id"], "username": row["username"]})


@app.route("/api/logout", methods=["POST"])
def api_logout():
    session.clear()
    return jsonify({"ok": True})


# ---------------------------------------------------------------------------
# API – Me
# ---------------------------------------------------------------------------
@app.route("/api/me")
def api_me():
    uid, err = require_auth()
    if err:
        return err
    return jsonify({"id": uid, "username": session.get("username")})


# ---------------------------------------------------------------------------
# API – Users
# ---------------------------------------------------------------------------
@app.route("/api/users")
def api_users():
    uid, err = require_auth()
    if err:
        return err

    with get_db() as conn:
        rows = conn.execute(
            "SELECT id, username FROM users WHERE id != ? ORDER BY username COLLATE NOCASE",
            (uid,)
        ).fetchall()

    return jsonify([{"id": r["id"], "username": r["username"]} for r in rows])


# ---------------------------------------------------------------------------
# API – Messages
# ---------------------------------------------------------------------------
@app.route("/api/messages", methods=["GET"])
def api_get_messages():
    uid, err = require_auth()
    if err:
        return err

    with_id_raw = request.args.get("with")
    if not with_id_raw or not with_id_raw.isdigit():
        return jsonify({"error": "?with=<user_id> is required."}), 400
    other_id = int(with_id_raw)

    # Ensure the other user exists
    with get_db() as conn:
        other = conn.execute("SELECT id FROM users WHERE id = ?", (other_id,)).fetchone()
        if not other:
            return jsonify({"error": "User not found."}), 404

        rows = conn.execute(
            """
            SELECT m.id, m.from_user_id, m.to_user_id, m.content, m.created_at,
                   u.username AS from_username
            FROM   messages m
            JOIN   users u ON u.id = m.from_user_id
            WHERE  (m.from_user_id = ? AND m.to_user_id = ?)
               OR  (m.from_user_id = ? AND m.to_user_id = ?)
            ORDER  BY m.created_at ASC, m.id ASC
            """,
            (uid, other_id, other_id, uid)
        ).fetchall()

    return jsonify([
        {
            "id": r["id"],
            "from_user_id": r["from_user_id"],
            "to_user_id": r["to_user_id"],
            "content": r["content"],
            "created_at": r["created_at"],
            "from_username": r["from_username"],
        }
        for r in rows
    ])


@app.route("/api/messages", methods=["POST"])
def api_send_message():
    uid, err = require_auth()
    if err:
        return err

    data = request.get_json(silent=True) or {}
    to_id_raw = data.get("to_user_id")
    content = (data.get("content") or "").strip()

    if not to_id_raw or not str(to_id_raw).isdigit():
        return jsonify({"error": "to_user_id must be a valid integer."}), 400
    to_id = int(to_id_raw)

    if to_id == uid:
        return jsonify({"error": "Cannot message yourself."}), 400

    if not content:
        return jsonify({"error": "Message content cannot be empty."}), 400
    if len(content) > 4000:
        return jsonify({"error": "Message too long (max 4000 chars)."}), 400

    with get_db() as conn:
        other = conn.execute("SELECT id FROM users WHERE id = ?", (to_id,)).fetchone()
        if not other:
            return jsonify({"error": "Recipient not found."}), 404

        cur = conn.execute(
            "INSERT INTO messages (from_user_id, to_user_id, content) VALUES (?, ?, ?)",
            (uid, to_id, content)
        )
        msg_id = cur.lastrowid

        row = conn.execute(
            """
            SELECT m.id, m.from_user_id, m.to_user_id, m.content, m.created_at,
                   u.username AS from_username
            FROM   messages m
            JOIN   users u ON u.id = m.from_user_id
            WHERE  m.id = ?
            """,
            (msg_id,)
        ).fetchone()

    return jsonify({
        "id": row["id"],
        "from_user_id": row["from_user_id"],
        "to_user_id": row["to_user_id"],
        "content": row["content"],
        "created_at": row["created_at"],
        "from_username": row["from_username"],
    }), 201


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    app.run(debug=True, port=5000)
