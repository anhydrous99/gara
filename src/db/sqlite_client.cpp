#include "sqlite_client.h"
#include "../utils/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

namespace gara {

SQLiteClient::SQLiteClient(const std::string& db_path)
    : db_(nullptr), db_path_(db_path) {

    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to open SQLite database: {}", sqlite3_errmsg(db_));
        throw std::runtime_error("Failed to open database: " + db_path);
    }

    // Enable WAL mode for better concurrency
    executeSql("PRAGMA journal_mode=WAL");
    executeSql("PRAGMA synchronous=NORMAL");
    executeSql("PRAGMA foreign_keys=ON");

    LOG_INFO("SQLite database opened: {}", db_path);
}

SQLiteClient::~SQLiteClient() {
    if (db_) {
        sqlite3_close(db_);
        LOG_INFO("SQLite database closed");
    }
}

bool SQLiteClient::initialize() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    // Read schema file
    std::ifstream schema_file("src/db/schema.sql");
    if (!schema_file.is_open()) {
        LOG_ERROR("Failed to open schema.sql file");
        return false;
    }

    std::stringstream buffer;
    buffer << schema_file.rdbuf();
    std::string schema_sql = buffer.str();

    // Execute schema
    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, schema_sql.c_str(), nullptr, nullptr, &error_msg);

    if (rc != SQLITE_OK) {
        std::string error = error_msg ? error_msg : "Unknown error";
        LOG_ERROR("Failed to initialize database schema: {}", error);
        sqlite3_free(error_msg);
        return false;
    }

    LOG_INFO("Database schema initialized successfully");
    return true;
}

bool SQLiteClient::executeSql(const std::string& sql) {
    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);

    if (rc != SQLITE_OK) {
        std::string error = error_msg ? error_msg : "Unknown error";
        LOG_ERROR("SQL execution failed: {}", error);
        sqlite3_free(error_msg);
        return false;
    }
    return true;
}

std::string SQLiteClient::vectorToJson(const std::vector<std::string>& vec) {
    json j = vec;
    return j.dump();
}

std::vector<std::string> SQLiteClient::jsonToVector(const std::string& json_str) {
    if (json_str.empty()) {
        return {};
    }
    try {
        json j = json::parse(json_str);
        return j.get<std::vector<std::string>>();
    } catch (const json::exception& e) {
        LOG_ERROR("Failed to parse JSON: {}", e.what());
        return {};
    }
}

Album SQLiteClient::extractAlbum(sqlite3_stmt* stmt) {
    Album album;

    album.album_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    album.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

    const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    album.description = desc ? desc : "";

    const char* cover = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    album.cover_image_id = cover ? cover : "";

    const char* image_ids_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    album.image_ids = jsonToVector(image_ids_json ? image_ids_json : "[]");

    const char* tags_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    album.tags = jsonToVector(tags_json ? tags_json : "[]");

    album.published = sqlite3_column_int(stmt, 6) != 0;
    album.created_at = static_cast<std::time_t>(sqlite3_column_int64(stmt, 7));
    album.updated_at = static_cast<std::time_t>(sqlite3_column_int64(stmt, 8));

    return album;
}

bool SQLiteClient::putAlbum(const Album& album) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        INSERT INTO albums (album_id, name, description, cover_image_id,
                          image_ids, tags, published, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(album_id) DO UPDATE SET
            name = excluded.name,
            description = excluded.description,
            cover_image_id = excluded.cover_image_id,
            image_ids = excluded.image_ids,
            tags = excluded.tags,
            published = excluded.published,
            updated_at = excluded.updated_at
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    // Bind parameters
    sqlite3_bind_text(stmt, 1, album.album_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, album.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, album.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, album.cover_image_id.c_str(), -1, SQLITE_TRANSIENT);

    std::string image_ids_json = vectorToJson(album.image_ids);
    sqlite3_bind_text(stmt, 5, image_ids_json.c_str(), -1, SQLITE_TRANSIENT);

    std::string tags_json = vectorToJson(album.tags);
    sqlite3_bind_text(stmt, 6, tags_json.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_int(stmt, 7, album.published ? 1 : 0);
    sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(album.created_at));
    sqlite3_bind_int64(stmt, 9, static_cast<sqlite3_int64>(album.updated_at));

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute putAlbum: {}", sqlite3_errmsg(db_));
        return false;
    }

    LOG_DEBUG("Album stored successfully: {}", album.album_id);
    return true;
}

std::optional<Album> SQLiteClient::getAlbum(const std::string& album_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        SELECT album_id, name, description, cover_image_id, image_ids,
               tags, published, created_at, updated_at
        FROM albums
        WHERE album_id = ?
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, album_id.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        Album album = extractAlbum(stmt);
        sqlite3_finalize(stmt);
        return album;
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute getAlbum: {}", sqlite3_errmsg(db_));
    }

    return std::nullopt;
}

std::vector<Album> SQLiteClient::listAlbums(bool published_only) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    std::string sql = R"(
        SELECT album_id, name, description, cover_image_id, image_ids,
               tags, published, created_at, updated_at
        FROM albums
    )";

    if (published_only) {
        sql += " WHERE published = 1";
    }

    sql += " ORDER BY created_at DESC";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return {};
    }

    std::vector<Album> albums;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        albums.push_back(extractAlbum(stmt));
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute listAlbums: {}", sqlite3_errmsg(db_));
        return {};
    }

    LOG_DEBUG("Listed {} albums", albums.size());
    return albums;
}

bool SQLiteClient::deleteAlbum(const std::string& album_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM albums WHERE album_id = ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, album_id.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute deleteAlbum: {}", sqlite3_errmsg(db_));
        return false;
    }

    int changes = sqlite3_changes(db_);
    LOG_DEBUG("Album deleted: {} (rows affected: {})", album_id, changes);

    return changes > 0;
}

bool SQLiteClient::albumNameExists(const std::string& name,
                                   const std::string& exclude_album_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    std::string sql = "SELECT 1 FROM albums WHERE name = ?";

    if (!exclude_album_id.empty()) {
        sql += " AND album_id != ?";
    }

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    if (!exclude_album_id.empty()) {
        sqlite3_bind_text(stmt, 2, exclude_album_id.c_str(), -1, SQLITE_STATIC);
    }

    rc = sqlite3_step(stmt);
    bool exists = (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);
    return exists;
}

// Image metadata operations

ImageMetadata SQLiteClient::extractImageMetadata(sqlite3_stmt* stmt) {
    ImageMetadata metadata;

    metadata.image_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    metadata.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    metadata.original_format = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    metadata.original_size = static_cast<size_t>(sqlite3_column_int64(stmt, 3));
    metadata.width = sqlite3_column_int(stmt, 4);
    metadata.height = sqlite3_column_int(stmt, 5);
    metadata.upload_timestamp = static_cast<std::time_t>(sqlite3_column_int64(stmt, 6));

    return metadata;
}

std::string SQLiteClient::getSortOrderSql(ImageSortOrder sort_order) {
    switch (sort_order) {
        case ImageSortOrder::NEWEST:
            return "uploaded_at DESC";
        case ImageSortOrder::OLDEST:
            return "uploaded_at ASC";
        case ImageSortOrder::NAME_ASC:
            return "name ASC";
        case ImageSortOrder::NAME_DESC:
            return "name DESC";
        default:
            return "uploaded_at DESC";
    }
}

bool SQLiteClient::putImageMetadata(const ImageMetadata& metadata) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        INSERT INTO images (image_id, name, original_format, size, width, height, uploaded_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(image_id) DO UPDATE SET
            name = excluded.name,
            original_format = excluded.original_format,
            size = excluded.size,
            width = excluded.width,
            height = excluded.height,
            uploaded_at = excluded.uploaded_at
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    // Bind parameters
    sqlite3_bind_text(stmt, 1, metadata.image_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, metadata.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, metadata.original_format.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(metadata.original_size));
    sqlite3_bind_int(stmt, 5, metadata.width);
    sqlite3_bind_int(stmt, 6, metadata.height);
    sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(metadata.upload_timestamp));

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute putImageMetadata: {}", sqlite3_errmsg(db_));
        return false;
    }

    LOG_DEBUG("Image metadata stored successfully: {}", metadata.image_id);
    return true;
}

std::optional<ImageMetadata> SQLiteClient::getImageMetadata(const std::string& image_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        SELECT image_id, name, original_format, size, width, height, uploaded_at
        FROM images
        WHERE image_id = ?
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, image_id.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        ImageMetadata metadata = extractImageMetadata(stmt);
        sqlite3_finalize(stmt);
        return metadata;
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute getImageMetadata: {}", sqlite3_errmsg(db_));
    }

    return std::nullopt;
}

std::vector<ImageMetadata> SQLiteClient::listImages(int limit, int offset,
                                                    ImageSortOrder sort_order) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    std::string sql = R"(
        SELECT image_id, name, original_format, size, width, height, uploaded_at
        FROM images
        ORDER BY )" + getSortOrderSql(sort_order) + R"(
        LIMIT ? OFFSET ?
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return {};
    }

    sqlite3_bind_int(stmt, 1, limit);
    sqlite3_bind_int(stmt, 2, offset);

    std::vector<ImageMetadata> images;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        images.push_back(extractImageMetadata(stmt));
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute listImages: {}", sqlite3_errmsg(db_));
        return {};
    }

    LOG_DEBUG("Listed {} images", images.size());
    return images;
}

int SQLiteClient::getImageCount() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "SELECT COUNT(*) FROM images";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return 0;
    }

    rc = sqlite3_step(stmt);

    int count = 0;
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        LOG_ERROR("Failed to execute getImageCount: {}", sqlite3_errmsg(db_));
        return 0;
    }

    return count;
}

bool SQLiteClient::imageExists(const std::string& image_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "SELECT 1 FROM images WHERE image_id = ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, image_id.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    bool exists = (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);
    return exists;
}

} // namespace gara
