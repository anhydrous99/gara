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
        Logger::error("Failed to open SQLite database: " + std::string(sqlite3_errmsg(db_)));
        throw std::runtime_error("Failed to open database: " + db_path);
    }

    // Enable WAL mode for better concurrency
    executeSql("PRAGMA journal_mode=WAL");
    executeSql("PRAGMA synchronous=NORMAL");
    executeSql("PRAGMA foreign_keys=ON");

    Logger::info("SQLite database opened: " + db_path);
}

SQLiteClient::~SQLiteClient() {
    if (db_) {
        sqlite3_close(db_);
        Logger::info("SQLite database closed");
    }
}

bool SQLiteClient::initialize() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    // Read schema file
    std::ifstream schema_file("src/db/schema.sql");
    if (!schema_file.is_open()) {
        Logger::error("Failed to open schema.sql file");
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
        Logger::error("Failed to initialize database schema: " + error);
        sqlite3_free(error_msg);
        return false;
    }

    Logger::info("Database schema initialized successfully");
    return true;
}

bool SQLiteClient::executeSql(const std::string& sql) {
    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);

    if (rc != SQLITE_OK) {
        std::string error = error_msg ? error_msg : "Unknown error";
        Logger::error("SQL execution failed: " + error);
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
        Logger::error("Failed to parse JSON: " + std::string(e.what()));
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
        Logger::error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
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
        Logger::error("Failed to execute putAlbum: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    Logger::debug("Album stored successfully: " + album.album_id);
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
        Logger::error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
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
        Logger::error("Failed to execute getAlbum: " + std::string(sqlite3_errmsg(db_)));
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
        Logger::error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return {};
    }

    std::vector<Album> albums;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        albums.push_back(extractAlbum(stmt));
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        Logger::error("Failed to execute listAlbums: " + std::string(sqlite3_errmsg(db_)));
        return {};
    }

    Logger::debug("Listed " + std::to_string(albums.size()) + " albums");
    return albums;
}

bool SQLiteClient::deleteAlbum(const std::string& album_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM albums WHERE album_id = ?";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        Logger::error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    sqlite3_bind_text(stmt, 1, album_id.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        Logger::error("Failed to execute deleteAlbum: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    int changes = sqlite3_changes(db_);
    Logger::debug("Album deleted: " + album_id + " (rows affected: " + std::to_string(changes) + ")");

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
        Logger::error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
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

} // namespace gara
