-- Gara Image SQLite Schema
-- Migrated from DynamoDB to SQLite

-- Albums table
CREATE TABLE IF NOT EXISTS albums (
    album_id TEXT PRIMARY KEY,
    name TEXT UNIQUE NOT NULL,
    description TEXT,
    cover_image_id TEXT,
    image_ids TEXT,  -- JSON array of image IDs
    tags TEXT,       -- JSON array of tags
    published INTEGER DEFAULT 0,  -- 0 = false, 1 = true
    created_at INTEGER,  -- Unix timestamp
    updated_at INTEGER   -- Unix timestamp
);

-- Indexes for performance
CREATE INDEX IF NOT EXISTS idx_albums_published ON albums(published);
CREATE INDEX IF NOT EXISTS idx_albums_name ON albums(name);
CREATE INDEX IF NOT EXISTS idx_albums_created_at ON albums(created_at DESC);
