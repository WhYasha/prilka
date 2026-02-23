-- V5: Seed sticker pack and sticker file metadata for MinIO-backed stickers.
-- Sticker SVGs are stored in the bh-stickers bucket at stickers/s0N.svg.
-- minio_init uploads the actual SVG files; this migration creates the DB records.

DO $$
DECLARE
    owner_id BIGINT;
    pack_id  BIGINT;
    f1 BIGINT; f2 BIGINT; f3 BIGINT; f4 BIGINT;
    f5 BIGINT; f6 BIGINT; f7 BIGINT; f8 BIGINT;
BEGIN
    -- Use the first seed user as system owner for sticker files
    SELECT id INTO owner_id FROM users WHERE username = 'alice' LIMIT 1;
    IF owner_id IS NULL THEN
        RAISE NOTICE 'No seed user found; skipping sticker seed';
        RETURN;
    END IF;

    -- File records (idempotent via ON CONFLICT on object_key UNIQUE)
    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (owner_id, 'bh-stickers', 'stickers/s01.svg', 'grin.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO f1;
    IF f1 IS NULL THEN SELECT id INTO f1 FROM files WHERE object_key = 'stickers/s01.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (owner_id, 'bh-stickers', 'stickers/s02.svg', 'lol.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO f2;
    IF f2 IS NULL THEN SELECT id INTO f2 FROM files WHERE object_key = 'stickers/s02.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (owner_id, 'bh-stickers', 'stickers/s03.svg', 'heart.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO f3;
    IF f3 IS NULL THEN SELECT id INTO f3 FROM files WHERE object_key = 'stickers/s03.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (owner_id, 'bh-stickers', 'stickers/s04.svg', 'like.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO f4;
    IF f4 IS NULL THEN SELECT id INTO f4 FROM files WHERE object_key = 'stickers/s04.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (owner_id, 'bh-stickers', 'stickers/s05.svg', 'party.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO f5;
    IF f5 IS NULL THEN SELECT id INTO f5 FROM files WHERE object_key = 'stickers/s05.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (owner_id, 'bh-stickers', 'stickers/s06.svg', 'cry.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO f6;
    IF f6 IS NULL THEN SELECT id INTO f6 FROM files WHERE object_key = 'stickers/s06.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (owner_id, 'bh-stickers', 'stickers/s07.svg', 'cool.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO f7;
    IF f7 IS NULL THEN SELECT id INTO f7 FROM files WHERE object_key = 'stickers/s07.svg'; END IF;

    INSERT INTO files (uploader_id, bucket, object_key, filename, mime_type, size_bytes)
    VALUES (owner_id, 'bh-stickers', 'stickers/s08.svg', 'fire.svg', 'image/svg+xml', 0)
    ON CONFLICT (object_key) DO NOTHING RETURNING id INTO f8;
    IF f8 IS NULL THEN SELECT id INTO f8 FROM files WHERE object_key = 'stickers/s08.svg'; END IF;

    -- Sticker pack (idempotent via UNIQUE name)
    INSERT INTO sticker_packs (name) VALUES ('Emojis')
    ON CONFLICT (name) DO NOTHING RETURNING id INTO pack_id;
    IF pack_id IS NULL THEN SELECT id INTO pack_id FROM sticker_packs WHERE name = 'Emojis'; END IF;

    -- Stickers: only insert if this pack has no stickers yet
    IF (SELECT COUNT(*) FROM stickers WHERE pack_id = pack_id) = 0 THEN
        INSERT INTO stickers (pack_id, label, file_id) VALUES
            (pack_id, 'grin',  f1),
            (pack_id, 'lol',   f2),
            (pack_id, 'heart', f3),
            (pack_id, 'like',  f4),
            (pack_id, 'party', f5),
            (pack_id, 'cry',   f6),
            (pack_id, 'cool',  f7),
            (pack_id, 'fire',  f8);
    END IF;
END $$;
