# Simple Messenger

A minimal 1:1 messenger built with Flask + SQLite + vanilla JS.

## Quick start

```bash
# 1. Enter the project directory
cd simple-messenger

# 2. Create and activate a virtual environment (recommended)
python -m venv .venv
source .venv/bin/activate          # Windows: .venv\Scripts\activate

# 3. Install dependencies
pip install -r requirements.txt

# 4. (Optional) Load demo users + sample messages
python seed.py

# 5. Run the development server
python app.py
```

Open **http://localhost:5000** in your browser.

### Demo credentials (after running seed.py)
| Username | Password    |
|----------|-------------|
| alice    | password123 |
| bob      | password123 |

---

## Project layout

```
simple-messenger/
├── app.py            # Flask app + all API routes
├── db.py             # DB connection helper + init_db()
├── schema.sql        # CREATE TABLE statements
├── seed.py           # Creates demo users & messages
├── requirements.txt
├── templates/
│   ├── base.html
│   ├── login.html
│   ├── register.html
│   └── app.html      # Main messenger SPA shell
└── static/
    ├── style.css
    └── app.js        # Vanilla JS client (polling every 2.5 s)
```

## API reference

| Method | Path | Auth | Body / Query | Description |
|--------|------|------|--------------|-------------|
| POST | `/api/register` | — | `{username, password}` | Create account + sign in |
| POST | `/api/login`    | — | `{username, password}` | Sign in |
| POST | `/api/logout`   | ✓ | — | Clear session |
| GET  | `/api/me`       | ✓ | — | Current user info |
| GET  | `/api/users`    | ✓ | — | All users except self |
| GET  | `/api/messages` | ✓ | `?with=<user_id>` | Conversation with a user |
| POST | `/api/messages` | ✓ | `{to_user_id, content}` | Send a message |

## Security notes

- Passwords are hashed with **PBKDF2-SHA256** via Werkzeug.
- All SQL uses **parameterised queries** (no string interpolation).
- Messages endpoint only returns rows where the logged-in user is a **participant**.
- Set a strong `SECRET_KEY` env var before deploying:
  ```bash
  export SECRET_KEY="$(python -c 'import secrets; print(secrets.token_hex(32))')"
  ```
- This is a **development** setup. For production add HTTPS, a WSGI server (gunicorn), and proper secret management.
