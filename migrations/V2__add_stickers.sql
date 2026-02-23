-- V2: Sticker packs

CREATE TABLE sticker_packs (
    id         BIGSERIAL  PRIMARY KEY,
    name       TEXT       NOT NULL UNIQUE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE stickers (
    id       BIGSERIAL  PRIMARY KEY,
    pack_id  BIGINT     NOT NULL REFERENCES sticker_packs(id) ON DELETE CASCADE,
    label    TEXT       NOT NULL,
    file_id  BIGINT     NOT NULL REFERENCES files(id) ON DELETE CASCADE
);

CREATE INDEX idx_stickers_pack ON stickers (pack_id);
