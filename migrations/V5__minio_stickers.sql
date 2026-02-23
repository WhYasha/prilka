-- V5: Seed sticker pack and sticker file metadata for MinIO-backed stickers.
-- Sticker SVGs are stored in the bh-stickers bucket at stickers/s0N.svg.
-- minio_init uploads the actual SVG files; this migration creates the DB records.
-- Variables prefixed v_ to avoid ambiguity with same-named table columns.

DO $$
DECLARE
    v_owner  BIGINT;
    v_pack   BIGINT;
    v_f1 BIGINT; v_f2 BIGINT; v_f3 BIGINT; v_f4 BIGINT;
    v_f5 BIGINT; v_f6 BIGINT; v_f7 BIGINT; v_f8 BIGINT;
BEGIN
    -- Use the first seed user as system owner for sticker files
    SELECT id INTO v_owner FROM users WHERE username = 'alice' LIMIT 1;
    IF v_owner IS NULL THEN
        RAISE NOTICE 'No seed user found; skipping sticker seed';
        RETURN;
    END IF;

    -- File records (idempotent via ON CONFLICT on object_key UNIQUE)
    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (v_owner, 'bh-stickers', 'stickers/s01.svg', 'grin.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO v_f1;
    IF v_f1 IS NULL THEN SELECT id INTO v_f1 FROM files WHERE object_key = 'stickers/s01.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (v_owner, 'bh-stickers', 'stickers/s02.svg', 'lol.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO v_f2;
    IF v_f2 IS NULL THEN SELECT id INTO v_f2 FROM files WHERE object_key = 'stickers/s02.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (v_owner, 'bh-stickers', 'stickers/s03.svg', 'heart.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO v_f3;
    IF v_f3 IS NULL THEN SELECT id INTO v_f3 FROM files WHERE object_key = 'stickers/s03.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (v_owner, 'bh-stickers', 'stickers/s04.svg', 'like.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO v_f4;
    IF v_f4 IS NULL THEN SELECT id INTO v_f4 FROM files WHERE object_key = 'stickers/s04.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (v_owner, 'bh-stickers', 'stickers/s05.svg', 'party.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO v_f5;
    IF v_f5 IS NULL THEN SELECT id INTO v_f5 FROM files WHERE object_key = 'stickers/s05.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (v_owner, 'bh-stickers', 'stickers/s06.svg', 'cry.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO v_f6;
    IF v_f6 IS NULL THEN SELECT id INTO v_f6 FROM files WHERE object_key = 'stickers/s06.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (v_owner, 'bh-stickers', 'stickers/s07.svg', 'cool.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO v_f7;
    IF v_f7 IS NULL THEN SELECT id INTO v_f7 FROM files WHERE object_key = 'stickers/s07.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (v_owner, 'bh-stickers', 'stickers/s08.svg', 'fire.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO v_f8;
    IF v_f8 IS NULL THEN SELECT id INTO v_f8 FROM files WHERE object_key = 'stickers/s08.svg'; END IF;

    -- Sticker pack (idempotent via UNIQUE name)
    INSERT INTO sticker_packs (name) VALUES ('Emojis')
    ON CONFLICT (name) DO NOTHING RETURNING id INTO v_pack;
    IF v_pack IS NULL THEN SELECT id INTO v_pack FROM sticker_packs WHERE name = 'Emojis'; END IF;

    -- Stickers: only insert if this pack has no stickers yet
    IF (SELECT COUNT(*) FROM stickers WHERE pack_id = v_pack) = 0 THEN
        INSERT INTO stickers (pack_id, label, file_id) VALUES
            (v_pack, 'grin',  v_f1),
            (v_pack, 'lol',   v_f2),
            (v_pack, 'heart', v_f3),
            (v_pack, 'like',  v_f4),
            (v_pack, 'party', v_f5),
            (v_pack, 'cry',   v_f6),
            (v_pack, 'cool',  v_f7),
            (v_pack, 'fire',  v_f8);
    END IF;
END $$;
