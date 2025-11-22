-- Gara Image MySQL Schema
-- MySQL-compatible version of the database schema

-- Albums table
CREATE TABLE IF NOT EXISTS albums (
    album_id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(255) UNIQUE NOT NULL,
    description TEXT,
    cover_image_id VARCHAR(64),
    image_ids JSON,         -- JSON array of image IDs
    tags JSON,              -- JSON array of tags
    published BOOLEAN DEFAULT FALSE,
    created_at BIGINT,      -- Unix timestamp
    updated_at BIGINT,      -- Unix timestamp
    INDEX idx_albums_published (published),
    INDEX idx_albums_name (name),
    INDEX idx_albums_created_at (created_at DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Images table
CREATE TABLE IF NOT EXISTS images (
    image_id VARCHAR(64) PRIMARY KEY,       -- SHA256 hash (64 chars)
    name VARCHAR(255) NOT NULL,             -- Original filename without extension
    original_format VARCHAR(16) NOT NULL,   -- jpeg, png, gif, tiff, webp
    size BIGINT NOT NULL,                   -- File size in bytes
    width INT,                              -- Image width in pixels
    height INT,                             -- Image height in pixels
    uploaded_at BIGINT NOT NULL,            -- Unix timestamp
    INDEX idx_images_uploaded_at (uploaded_at DESC),
    INDEX idx_images_name (name ASC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
