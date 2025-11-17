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

-- Images table
CREATE TABLE IF NOT EXISTS images (
    image_id TEXT PRIMARY KEY,        -- SHA256 hash (64 chars)
    name TEXT NOT NULL,                -- Original filename without extension
    original_format TEXT NOT NULL,     -- jpeg, png, gif, tiff, webp
    size INTEGER NOT NULL,             -- File size in bytes
    width INTEGER,                     -- Image width in pixels
    height INTEGER,                    -- Image height in pixels
    uploaded_at INTEGER NOT NULL       -- Unix timestamp
);

-- Indexes for performance
CREATE INDEX IF NOT EXISTS idx_images_uploaded_at ON images(uploaded_at DESC);
CREATE INDEX IF NOT EXISTS idx_images_name ON images(name ASC);
