"""
Simple Messenger v2 – Telegram-like prototype
Flask backend with full API surface.
Run:  python app.py
"""

import os
import uuid
from flask import (
    Flask, request, session, jsonify,
    render_template, redirect, url_for, send_from_directory, abort
)
from werkzeug.security import generate_password_hash, check_password_hash
from werkzeug.utils import secure_filename
from db import get_db, init_db
import storage as _storage

# ---------------------------------------------------------------------------
# App setup
# ---------------------------------------------------------------------------
app = Flask(__name__)
app.secret_key = os.environ.get(
    "SECRET_KEY",
    "change-me-in-production-use-a-long-random-string"
)

UPLOAD_FOLDER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "uploads")
AVATAR_FOLDER = os.path.join(UPLOAD_FOLDER, "avatars")
VOICE_FOLDER  = os.path.join(UPLOAD_FOLDER, "voice")

os.makedirs(AVATAR_FOLDER, exist_ok=True)
os.makedirs(VOICE_FOLDER,  exist_ok=True)

ALLOWED_IMAGE_EXTS = {"png", "jpg", "jpeg", "gif", "webp"}
ALLOWED_AUDIO_EXTS = {"webm", "ogg", "mp3", "wav", "m4a"}
MAX_AVATAR_SIZE = 5 * 1024 * 1024   # 5 MB
MAX_VOICE_SIZE  = 10 * 1024 * 1024  # 10 MB

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
    uid = current_user_id()
    if not uid:
        return None, (jsonify({"error": "Unauthorised"}), 401)
    return uid, None


def allowed_ext(filename, allowed):
    return "." in filename and filename.rsplit(".", 1)[1].lower() in allowed


def resolve_avatar(path):
    """Return a presigned MinIO URL for an avatar object key, or None."""
    return _storage.get_avatar_url(path) if path else None


def get_or_create_conversation(conn, uid, other_id):
    """Return conversation_id for the pair (uid, other_id), creating if needed."""
    a, b = min(uid, other_id), max(uid, other_id)
    row = conn.execute(
        "SELECT id FROM conversations WHERE user_a_id=? AND user_b_id=?", (a, b)
    ).fetchone()
    if row:
        return row["id"]
    cur = conn.execute(
        "INSERT INTO conversations (user_a_id, user_b_id) VALUES (?, ?)", (a, b)
    )
    return cur.lastrowid


# ---------------------------------------------------------------------------
# Health check
# ---------------------------------------------------------------------------
@app.route("/health")
def health():
    return jsonify({"status": "ok", "service": "messenger-legacy"}), 200


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
# Upload file serving (auth-gated)
# ---------------------------------------------------------------------------
@app.route("/uploads/<path:filepath>")
def serve_upload(filepath):
    uid, err = require_auth()
    if err:
        return err
    safe = os.path.normpath(filepath).replace("\\", "/")
    full = os.path.join(UPLOAD_FOLDER, safe)
    if not os.path.abspath(full).startswith(os.path.abspath(UPLOAD_FOLDER)):
        abort(403)
    directory = os.path.dirname(full)
    filename  = os.path.basename(full)
    return send_from_directory(directory, filename)


# ---------------------------------------------------------------------------
# API – Auth
# ---------------------------------------------------------------------------
@app.route("/api/register", methods=["POST"])
def api_register():
    data = request.get_json(silent=True) or {}
    username = (data.get("username") or "").strip()
    password = data.get("password") or ""

    if not (MIN_USERNAME <= len(username) <= MAX_USERNAME):
        return jsonify({"error": f"Username must be {MIN_USERNAME}–{MAX_USERNAME} characters."}), 400
    if not username.replace("_", "").replace("-", "").isalnum():
        return jsonify({"error": "Username may only contain letters, digits, - and _."}), 400
    if not (MIN_PASSWORD <= len(password) <= MAX_PASSWORD):
        return jsonify({"error": f"Password must be {MIN_PASSWORD}–{MAX_PASSWORD} characters."}), 400

    pw_hash = generate_password_hash(password)
    display_name = username

    try:
        with get_db() as conn:
            cur = conn.execute(
                "INSERT INTO users (username, password_hash, display_name) VALUES (?, ?, ?)",
                (username, pw_hash, display_name)
            )
            user_id = cur.lastrowid
            conn.execute(
                "INSERT OR IGNORE INTO user_settings (user_id) VALUES (?)", (user_id,)
            )
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
    with get_db() as conn:
        row = conn.execute(
            "SELECT id, username, display_name, bio, avatar_path FROM users WHERE id=?", (uid,)
        ).fetchone()
    if not row:
        return jsonify({"error": "User not found"}), 404
    return jsonify({
        "id": row["id"],
        "username": row["username"],
        "display_name": row["display_name"] or row["username"],
        "bio": row["bio"] or "",
        "avatar_url": resolve_avatar(row["avatar_path"]),
    })


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
            "SELECT id, username, display_name, avatar_path FROM users WHERE id != ? ORDER BY username COLLATE NOCASE",
            (uid,)
        ).fetchall()

    return jsonify([{
        "id": r["id"],
        "username": r["username"],
        "display_name": r["display_name"] or r["username"],
        "avatar_url": resolve_avatar(r["avatar_path"]),
    } for r in rows])


# ---------------------------------------------------------------------------
# API – Chats (conversations)
# ---------------------------------------------------------------------------
@app.route("/api/chats", methods=["GET"])
def api_get_chats():
    uid, err = require_auth()
    if err:
        return err

    with get_db() as conn:
        rows = conn.execute(
            """
            SELECT c.id,
                   CASE WHEN c.user_a_id = ? THEN c.user_b_id ELSE c.user_a_id END AS other_id,
                   c.updated_at,
                   u.username     AS other_username,
                   u.display_name AS other_display_name,
                   u.avatar_path  AS other_avatar_path,
                   (SELECT m.content FROM messages m
                    WHERE m.conversation_id = c.id
                    ORDER BY m.id DESC LIMIT 1) AS last_content,
                   (SELECT m.message_type FROM messages m
                    WHERE m.conversation_id = c.id
                    ORDER BY m.id DESC LIMIT 1) AS last_type,
                   (SELECT m.created_at FROM messages m
                    WHERE m.conversation_id = c.id
                    ORDER BY m.id DESC LIMIT 1) AS last_at
            FROM   conversations c
            JOIN   users u ON u.id = CASE WHEN c.user_a_id = ? THEN c.user_b_id ELSE c.user_a_id END
            WHERE  c.user_a_id = ? OR c.user_b_id = ?
            ORDER  BY COALESCE(last_at, c.updated_at) DESC
            """,
            (uid, uid, uid, uid)
        ).fetchall()

    result = []
    for r in rows:
        last_preview = ""
        if r["last_type"] == "sticker":
            last_preview = "🎭 Sticker"
        elif r["last_type"] == "voice":
            last_preview = "🎤 Voice message"
        elif r["last_content"]:
            last_preview = r["last_content"][:80]

        result.append({
            "id": r["id"],
            "other_id": r["other_id"],
            "other_username": r["other_username"],
            "other_display_name": r["other_display_name"] or r["other_username"],
            "other_avatar_url": resolve_avatar(r["other_avatar_path"]),
            "last_preview": last_preview,
            "last_at": r["last_at"] or r["updated_at"],
        })

    return jsonify(result)


@app.route("/api/chats", methods=["POST"])
def api_create_chat():
    uid, err = require_auth()
    if err:
        return err

    data = request.get_json(silent=True) or {}
    other_id_raw = data.get("with_user_id")
    if not other_id_raw or not str(other_id_raw).isdigit():
        return jsonify({"error": "with_user_id required"}), 400
    other_id = int(other_id_raw)

    if other_id == uid:
        return jsonify({"error": "Cannot chat with yourself"}), 400

    with get_db() as conn:
        other = conn.execute("SELECT id FROM users WHERE id=?", (other_id,)).fetchone()
        if not other:
            return jsonify({"error": "User not found"}), 404
        cid = get_or_create_conversation(conn, uid, other_id)

    return jsonify({"id": cid}), 200


# ---------------------------------------------------------------------------
# API – Messages
# ---------------------------------------------------------------------------
@app.route("/api/messages", methods=["GET"])
def api_get_messages():
    uid, err = require_auth()
    if err:
        return err

    chat_id_raw = request.args.get("chat_id")
    after_id_raw = request.args.get("after_id", "0")

    if not chat_id_raw or not chat_id_raw.isdigit():
        return jsonify({"error": "?chat_id=<id> required"}), 400
    chat_id = int(chat_id_raw)
    after_id = int(after_id_raw) if after_id_raw.isdigit() else 0

    with get_db() as conn:
        # Verify user is in this conversation
        conv = conn.execute(
            "SELECT id FROM conversations WHERE id=? AND (user_a_id=? OR user_b_id=?)",
            (chat_id, uid, uid)
        ).fetchone()
        if not conv:
            return jsonify({"error": "Conversation not found"}), 404

        rows = conn.execute(
            """
            SELECT m.id, m.conversation_id, m.from_user_id, m.content,
                   m.message_type, m.attachment_path, m.duration_seconds, m.created_at,
                   u.username AS from_username,
                   u.display_name AS from_display_name,
                   u.avatar_path AS from_avatar_path,
                   s.file_path AS sticker_path, s.label AS sticker_label
            FROM   messages m
            JOIN   users u ON u.id = m.from_user_id
            LEFT JOIN stickers s ON s.id = CAST(m.content AS INTEGER)
                                  AND m.message_type = 'sticker'
            WHERE  m.conversation_id = ? AND m.id > ?
            ORDER  BY m.id ASC
            LIMIT  100
            """,
            (chat_id, after_id)
        ).fetchall()

    return jsonify([{
        "id": r["id"],
        "conversation_id": r["conversation_id"],
        "from_user_id": r["from_user_id"],
        "from_username": r["from_username"],
        "from_display_name": r["from_display_name"] or r["from_username"],
        "from_avatar_url": resolve_avatar(r["from_avatar_path"]),
        "content": r["content"],
        "message_type": r["message_type"],
        "attachment_path": r["attachment_path"],
        "duration_seconds": r["duration_seconds"],
        "sticker_url": _storage.get_sticker_url(r["sticker_path"]) if r["sticker_path"] else None,
        "sticker_label": r["sticker_label"],
        "created_at": r["created_at"],
    } for r in rows])


@app.route("/api/messages", methods=["POST"])
def api_send_message():
    uid, err = require_auth()
    if err:
        return err

    data = request.get_json(silent=True) or {}
    chat_id_raw = data.get("chat_id")
    msg_type    = data.get("type", "text")
    content     = (data.get("content") or "").strip()
    sticker_id  = data.get("sticker_id")

    if not chat_id_raw or not str(chat_id_raw).isdigit():
        return jsonify({"error": "chat_id required"}), 400
    chat_id = int(chat_id_raw)

    if msg_type not in ("text", "sticker"):
        return jsonify({"error": "Invalid message type"}), 400

    if msg_type == "text":
        if not content:
            return jsonify({"error": "Content cannot be empty"}), 400
        if len(content) > 4000:
            return jsonify({"error": "Message too long (max 4000 chars)"}), 400
        store_content = content
    else:  # sticker
        if not sticker_id:
            return jsonify({"error": "sticker_id required"}), 400
        store_content = str(sticker_id)

    with get_db() as conn:
        conv = conn.execute(
            "SELECT id FROM conversations WHERE id=? AND (user_a_id=? OR user_b_id=?)",
            (chat_id, uid, uid)
        ).fetchone()
        if not conv:
            return jsonify({"error": "Conversation not found"}), 404

        cur = conn.execute(
            "INSERT INTO messages (conversation_id, from_user_id, content, message_type) VALUES (?, ?, ?, ?)",
            (chat_id, uid, store_content, msg_type)
        )
        msg_id = cur.lastrowid

        conn.execute(
            "UPDATE conversations SET updated_at=CURRENT_TIMESTAMP WHERE id=?", (chat_id,)
        )

        row = conn.execute(
            """
            SELECT m.id, m.conversation_id, m.from_user_id, m.content,
                   m.message_type, m.attachment_path, m.duration_seconds, m.created_at,
                   u.username AS from_username, u.display_name AS from_display_name,
                   u.avatar_path AS from_avatar_path,
                   s.file_path AS sticker_path, s.label AS sticker_label
            FROM   messages m
            JOIN   users u ON u.id = m.from_user_id
            LEFT JOIN stickers s ON s.id = CAST(m.content AS INTEGER)
                                  AND m.message_type = 'sticker'
            WHERE  m.id = ?
            """,
            (msg_id,)
        ).fetchone()

    return jsonify({
        "id": row["id"],
        "conversation_id": row["conversation_id"],
        "from_user_id": row["from_user_id"],
        "from_username": row["from_username"],
        "from_display_name": row["from_display_name"] or row["from_username"],
        "from_avatar_url": resolve_avatar(row["from_avatar_path"]),
        "content": row["content"],
        "message_type": row["message_type"],
        "attachment_path": row["attachment_path"],
        "duration_seconds": row["duration_seconds"],
        "sticker_url": _storage.get_sticker_url(row["sticker_path"]) if row["sticker_path"] else None,
        "sticker_label": row["sticker_label"],
        "created_at": row["created_at"],
    }), 201


# ---------------------------------------------------------------------------
# API – Upload avatar
# ---------------------------------------------------------------------------
@app.route("/api/upload/avatar", methods=["POST"])
def api_upload_avatar():
    uid, err = require_auth()
    if err:
        return err

    if "file" not in request.files:
        return jsonify({"error": "No file provided"}), 400

    f = request.files["file"]
    if not f.filename or not allowed_ext(f.filename, ALLOWED_IMAGE_EXTS):
        return jsonify({"error": "Invalid file type"}), 400

    f.seek(0, 2)
    size = f.tell()
    f.seek(0)
    if size > MAX_AVATAR_SIZE:
        return jsonify({"error": "File too large (max 5MB)"}), 400

    ext = f.filename.rsplit(".", 1)[1].lower()
    try:
        object_key = _storage.upload_avatar(f.stream, uid, ext)
    except Exception as exc:
        app.logger.error("MinIO avatar upload failed: %s", exc)
        return jsonify({"error": "Storage upload failed"}), 502

    with get_db() as conn:
        conn.execute("UPDATE users SET avatar_path=? WHERE id=?", (object_key, uid))

    return jsonify({"avatar_path": object_key, "url": resolve_avatar(object_key)})


# ---------------------------------------------------------------------------
# API – Upload voice
# ---------------------------------------------------------------------------
@app.route("/api/upload/voice", methods=["POST"])
def api_upload_voice():
    uid, err = require_auth()
    if err:
        return err

    if "file" not in request.files:
        return jsonify({"error": "No file provided"}), 400

    f = request.files["file"]
    if not f.filename or not allowed_ext(f.filename, ALLOWED_AUDIO_EXTS):
        return jsonify({"error": "Invalid file type"}), 400

    f.seek(0, 2)
    size = f.tell()
    f.seek(0)
    if size > MAX_VOICE_SIZE:
        return jsonify({"error": "File too large (max 10MB)"}), 400

    chat_id_raw = request.form.get("chat_id", "")
    duration_raw = request.form.get("duration", "0")
    if not chat_id_raw.isdigit():
        return jsonify({"error": "chat_id required"}), 400
    chat_id = int(chat_id_raw)
    duration = int(duration_raw) if duration_raw.isdigit() else 0

    with get_db() as conn:
        conv = conn.execute(
            "SELECT id FROM conversations WHERE id=? AND (user_a_id=? OR user_b_id=?)",
            (chat_id, uid, uid)
        ).fetchone()
        if not conv:
            return jsonify({"error": "Conversation not found"}), 404

    ext = f.filename.rsplit(".", 1)[1].lower() if "." in f.filename else "webm"
    filename = f"voice_{uid}_{uuid.uuid4().hex[:8]}.{ext}"
    save_path = os.path.join(VOICE_FOLDER, filename)
    f.save(save_path)

    rel_path = f"voice/{filename}"
    with get_db() as conn:
        cur = conn.execute(
            """INSERT INTO messages
               (conversation_id, from_user_id, content, message_type, attachment_path, duration_seconds)
               VALUES (?, ?, '', 'voice', ?, ?)""",
            (chat_id, uid, rel_path, duration)
        )
        msg_id = cur.lastrowid
        conn.execute(
            "UPDATE conversations SET updated_at=CURRENT_TIMESTAMP WHERE id=?", (chat_id,)
        )
        row = conn.execute(
            """
            SELECT m.id, m.conversation_id, m.from_user_id, m.content,
                   m.message_type, m.attachment_path, m.duration_seconds, m.created_at,
                   u.username AS from_username, u.display_name AS from_display_name,
                   u.avatar_path AS from_avatar_path
            FROM   messages m JOIN users u ON u.id = m.from_user_id
            WHERE  m.id=?
            """,
            (msg_id,)
        ).fetchone()

    return jsonify({
        "id": row["id"],
        "conversation_id": row["conversation_id"],
        "from_user_id": row["from_user_id"],
        "from_username": row["from_username"],
        "from_display_name": row["from_display_name"] or row["from_username"],
        "from_avatar_url": resolve_avatar(row["from_avatar_path"]),
        "content": "",
        "message_type": "voice",
        "attachment_path": row["attachment_path"],
        "duration_seconds": row["duration_seconds"],
        "sticker_url": None,
        "sticker_label": None,
        "created_at": row["created_at"],
    }), 201


# ---------------------------------------------------------------------------
# API – Profile
# ---------------------------------------------------------------------------
@app.route("/api/profile", methods=["GET"])
def api_get_profile():
    uid, err = require_auth()
    if err:
        return err
    with get_db() as conn:
        row = conn.execute(
            "SELECT id, username, display_name, bio, avatar_path FROM users WHERE id=?", (uid,)
        ).fetchone()
    if not row:
        return jsonify({"error": "Not found"}), 404
    return jsonify({
        "id": row["id"],
        "username": row["username"],
        "display_name": row["display_name"] or row["username"],
        "bio": row["bio"] or "",
        "avatar_url": resolve_avatar(row["avatar_path"]),
    })


@app.route("/api/profile", methods=["PUT"])
def api_update_profile():
    uid, err = require_auth()
    if err:
        return err
    data = request.get_json(silent=True) or {}
    display_name = (data.get("display_name") or "").strip()[:64] or None
    bio = (data.get("bio") or "").strip()[:200] or None

    with get_db() as conn:
        conn.execute(
            "UPDATE users SET display_name=?, bio=? WHERE id=?",
            (display_name, bio, uid)
        )
        row = conn.execute(
            "SELECT id, username, display_name, bio, avatar_path FROM users WHERE id=?", (uid,)
        ).fetchone()
    return jsonify({
        "id": row["id"],
        "username": row["username"],
        "display_name": row["display_name"] or row["username"],
        "bio": row["bio"] or "",
        "avatar_url": resolve_avatar(row["avatar_path"]),
    })


# ---------------------------------------------------------------------------
# API – Settings
# ---------------------------------------------------------------------------
@app.route("/api/settings", methods=["GET"])
def api_get_settings():
    uid, err = require_auth()
    if err:
        return err
    with get_db() as conn:
        row = conn.execute(
            "SELECT theme, notifications_enabled, language FROM user_settings WHERE user_id=?", (uid,)
        ).fetchone()
    if not row:
        return jsonify({"theme": "light", "notifications_enabled": True, "language": "en"})
    return jsonify({
        "theme": row["theme"],
        "notifications_enabled": bool(row["notifications_enabled"]),
        "language": row["language"],
    })


@app.route("/api/settings", methods=["PUT"])
def api_update_settings():
    uid, err = require_auth()
    if err:
        return err
    data = request.get_json(silent=True) or {}
    theme = data.get("theme", "light")
    notif = 1 if data.get("notifications_enabled", True) else 0
    lang  = data.get("language", "en")

    if theme not in ("light", "dark"):
        theme = "light"

    with get_db() as conn:
        conn.execute(
            """INSERT INTO user_settings (user_id, theme, notifications_enabled, language)
               VALUES (?, ?, ?, ?)
               ON CONFLICT(user_id) DO UPDATE SET
                   theme=excluded.theme,
                   notifications_enabled=excluded.notifications_enabled,
                   language=excluded.language""",
            (uid, theme, notif, lang)
        )
    return jsonify({"theme": theme, "notifications_enabled": bool(notif), "language": lang})


# ---------------------------------------------------------------------------
# API – Stickers
# ---------------------------------------------------------------------------
@app.route("/api/stickers")
def api_stickers():
    uid, err = require_auth()
    if err:
        return err
    with get_db() as conn:
        rows = conn.execute("SELECT id, pack_name, label, file_path FROM stickers ORDER BY id").fetchall()
    return jsonify([{
        "id": r["id"],
        "pack_name": r["pack_name"],
        "label": r["label"],
        "url": _storage.get_sticker_url(r["file_path"]),
    } for r in rows])


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    app.run(host="0.0.0.0", debug=False, port=5000)
