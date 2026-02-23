"""MinIO object storage client for Flask messenger.

Buckets used:
  bh-avatars  – user profile pictures  (MINIO_AVATARS_BUCKET)
  bh-stickers – sticker SVG files       (MINIO_STICKERS_BUCKET)

All buckets are private; files are served via short-lived presigned GET URLs
(default TTL = MINIO_PRESIGN_TTL seconds, default 900).

Public URL rewriting
--------------------
MinIO runs on an internal Docker network (minio:9000).  In production the S3
API is proxied through nginx at /minio/, which passes Host: minio:9000 so
presigned-URL signatures remain valid.  Set MINIO_PUBLIC_URL to the public
base URL so the presigned URLs handed to the browser point to the right host:

  dev:  MINIO_PUBLIC_URL=http://localhost:9000
  prod: MINIO_PUBLIC_URL=https://behappy.rest/minio
"""

import datetime
import os
import uuid

_ENDPOINT    = os.environ.get("MINIO_ENDPOINT",       "localhost:9000").replace("http://", "").replace("https://", "")
_ACCESS_KEY  = os.environ.get("MINIO_ACCESS_KEY",     "minioadmin")
_SECRET_KEY  = os.environ.get("MINIO_SECRET_KEY",     "changeme_minio")
_PUBLIC_URL  = os.environ.get("MINIO_PUBLIC_URL",     "").rstrip("/")

AVATARS_BUCKET  = os.environ.get("MINIO_AVATARS_BUCKET",  "bh-avatars")
STICKERS_BUCKET = os.environ.get("MINIO_STICKERS_BUCKET", "bh-stickers")
PRESIGN_TTL     = int(os.environ.get("MINIO_PRESIGN_TTL", "900"))

_client = None


def _get_client():
    global _client
    if _client is None:
        from minio import Minio
        _client = Minio(_ENDPOINT, access_key=_ACCESS_KEY, secret_key=_SECRET_KEY, secure=False)
    return _client


def _rewrite_url(url: str) -> str:
    """Replace internal MinIO base with the public URL (if configured)."""
    if not _PUBLIC_URL:
        return url
    internal_base = "http://" + _ENDPOINT
    if url.startswith(internal_base):
        return _PUBLIC_URL + url[len(internal_base):]
    return url


# ---------------------------------------------------------------------------
# Avatar storage
# ---------------------------------------------------------------------------

def upload_avatar(file_stream, user_id: int, ext: str) -> str:
    """Upload avatar to bh-avatars bucket.  Returns the object key."""
    key = f"avatars/{user_id}/{uuid.uuid4().hex[:12]}.{ext}"
    file_stream.seek(0, 2)
    length = file_stream.tell()
    file_stream.seek(0)
    content_type = f"image/{ext}" if ext not in ("jpg",) else "image/jpeg"
    if ext == "jpg":
        content_type = "image/jpeg"
    _get_client().put_object(AVATARS_BUCKET, key, file_stream, length, content_type=content_type)
    return key


def get_avatar_url(object_key: str):
    """Return a presigned GET URL for the given avatar object key, or None."""
    if not object_key or not object_key.startswith("avatars/"):
        return None
    try:
        url = _get_client().presigned_get_object(
            AVATARS_BUCKET, object_key,
            expires=datetime.timedelta(seconds=PRESIGN_TTL),
        )
        return _rewrite_url(url)
    except Exception:
        return None


# ---------------------------------------------------------------------------
# Sticker storage
# ---------------------------------------------------------------------------

def get_sticker_url(object_key: str):
    """Return a presigned GET URL for the given sticker object key, or None."""
    if not object_key:
        return None
    try:
        url = _get_client().presigned_get_object(
            STICKERS_BUCKET, object_key,
            expires=datetime.timedelta(seconds=PRESIGN_TTL),
        )
        return _rewrite_url(url)
    except Exception:
        return None
