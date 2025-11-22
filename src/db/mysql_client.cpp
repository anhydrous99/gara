#include "mysql_client.h"
#include "../utils/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

using json = nlohmann::json;

namespace gara {

MySQLConfig MySQLConfig::fromEnvironment() {
    MySQLConfig config;

    const char* host = std::getenv("MYSQL_HOST");
    const char* port = std::getenv("MYSQL_PORT");
    const char* user = std::getenv("MYSQL_USER");
    const char* password = std::getenv("MYSQL_PASSWORD");
    const char* database = std::getenv("MYSQL_DATABASE");

    if (host) config.host = host;
    if (port) {
        try {
            config.port = std::stoi(port);
        } catch (...) {
            LOG_WARN("Invalid MYSQL_PORT value, using default 3306");
        }
    }
    if (user) config.user = user;
    if (password) config.password = password;
    if (database) config.database = database;

    return config;
}

MySQLClient::MySQLClient(const MySQLConfig& config)
    : conn_(nullptr), config_(config) {

    conn_ = mysql_init(nullptr);
    if (conn_ == nullptr) {
        LOG_ERROR("Failed to initialize MySQL connection");
        throw std::runtime_error("Failed to initialize MySQL connection");
    }

    // Enable auto-reconnect
    bool reconnect = true;
    mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnect);

    // Set connection timeout
    unsigned int timeout = 10;
    mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    // Connect to the database
    if (mysql_real_connect(conn_, config_.host.c_str(), config_.user.c_str(),
                           config_.password.c_str(), config_.database.c_str(),
                           config_.port, nullptr, 0) == nullptr) {
        std::string error = mysql_error(conn_);
        mysql_close(conn_);
        conn_ = nullptr;
        LOG_ERROR("Failed to connect to MySQL database: {}", error);
        throw std::runtime_error("Failed to connect to MySQL: " + error);
    }

    // Set character set to UTF-8
    mysql_set_character_set(conn_, "utf8mb4");

    LOG_INFO("MySQL database connected: {}@{}:{}/{}",
             config_.user, config_.host, config_.port, config_.database);
}

MySQLClient::~MySQLClient() {
    if (conn_) {
        mysql_close(conn_);
        LOG_INFO("MySQL database connection closed");
    }
}

bool MySQLClient::isConnected() const {
    return conn_ != nullptr && mysql_ping(const_cast<MYSQL*>(conn_)) == 0;
}

bool MySQLClient::reconnectIfNeeded() {
    if (conn_ == nullptr) {
        return false;
    }

    if (mysql_ping(conn_) != 0) {
        LOG_WARN("MySQL connection lost, attempting reconnect...");
        // mysql_ping with MYSQL_OPT_RECONNECT should auto-reconnect
        if (mysql_ping(conn_) != 0) {
            LOG_ERROR("Failed to reconnect to MySQL: {}", mysql_error(conn_));
            return false;
        }
        LOG_INFO("MySQL reconnected successfully");
    }
    return true;
}

bool MySQLClient::initialize() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return false;
    }

    // Read schema file
    std::ifstream schema_file("src/db/schema_mysql.sql");
    if (!schema_file.is_open()) {
        LOG_ERROR("Failed to open schema_mysql.sql file");
        return false;
    }

    std::stringstream buffer;
    buffer << schema_file.rdbuf();
    std::string schema_sql = buffer.str();

    // MySQL requires executing statements one at a time
    // Split by semicolon and execute each statement
    std::istringstream stream(schema_sql);
    std::string statement;

    while (std::getline(stream, statement, ';')) {
        // Trim whitespace
        size_t start = statement.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) continue;

        size_t end = statement.find_last_not_of(" \t\n\r");
        statement = statement.substr(start, end - start + 1);

        // Skip empty statements and comments
        if (statement.empty() || statement[0] == '-') continue;

        if (mysql_query(conn_, statement.c_str()) != 0) {
            LOG_ERROR("Failed to execute schema statement: {}", mysql_error(conn_));
            LOG_ERROR("Statement: {}", statement);
            return false;
        }
    }

    LOG_INFO("MySQL database schema initialized successfully");
    return true;
}

bool MySQLClient::executeSql(const std::string& sql) {
    if (!reconnectIfNeeded()) {
        return false;
    }

    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("SQL execution failed: {}", mysql_error(conn_));
        return false;
    }
    return true;
}

std::string MySQLClient::vectorToJson(const std::vector<std::string>& vec) {
    json j = vec;
    return j.dump();
}

std::vector<std::string> MySQLClient::jsonToVector(const std::string& json_str) {
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

std::string MySQLClient::escapeString(const std::string& str) {
    if (conn_ == nullptr) {
        return str;
    }

    std::vector<char> buffer(str.length() * 2 + 1);
    unsigned long len = mysql_real_escape_string(conn_, buffer.data(), str.c_str(), str.length());
    return std::string(buffer.data(), len);
}

std::string MySQLClient::getSafeString(MYSQL_ROW row, int index, unsigned long* lengths) {
    if (row[index] == nullptr) {
        return "";
    }
    return std::string(row[index], lengths[index]);
}

Album MySQLClient::extractAlbum(MYSQL_ROW row, unsigned long* lengths) {
    Album album;

    album.album_id = getSafeString(row, 0, lengths);
    album.name = getSafeString(row, 1, lengths);
    album.description = getSafeString(row, 2, lengths);
    album.cover_image_id = getSafeString(row, 3, lengths);

    std::string image_ids_json = getSafeString(row, 4, lengths);
    album.image_ids = jsonToVector(image_ids_json.empty() ? "[]" : image_ids_json);

    std::string tags_json = getSafeString(row, 5, lengths);
    album.tags = jsonToVector(tags_json.empty() ? "[]" : tags_json);

    album.published = row[6] != nullptr && std::string(row[6]) == "1";

    if (row[7] != nullptr) {
        album.created_at = static_cast<std::time_t>(std::stoll(row[7]));
    }
    if (row[8] != nullptr) {
        album.updated_at = static_cast<std::time_t>(std::stoll(row[8]));
    }

    return album;
}

ImageMetadata MySQLClient::extractImageMetadata(MYSQL_ROW row, unsigned long* lengths) {
    ImageMetadata metadata;

    metadata.image_id = getSafeString(row, 0, lengths);
    metadata.name = getSafeString(row, 1, lengths);
    metadata.original_format = getSafeString(row, 2, lengths);

    if (row[3] != nullptr) {
        metadata.original_size = static_cast<size_t>(std::stoull(row[3]));
    }
    if (row[4] != nullptr) {
        metadata.width = std::stoi(row[4]);
    }
    if (row[5] != nullptr) {
        metadata.height = std::stoi(row[5]);
    }
    if (row[6] != nullptr) {
        metadata.upload_timestamp = static_cast<std::time_t>(std::stoll(row[6]));
    }

    return metadata;
}

std::string MySQLClient::getSortOrderSql(ImageSortOrder sort_order) {
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

bool MySQLClient::putAlbum(const Album& album) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return false;
    }

    // MySQL uses INSERT ... ON DUPLICATE KEY UPDATE instead of ON CONFLICT
    std::string sql = "INSERT INTO albums (album_id, name, description, cover_image_id, "
                      "image_ids, tags, published, created_at, updated_at) "
                      "VALUES ('" + escapeString(album.album_id) + "', "
                      "'" + escapeString(album.name) + "', "
                      "'" + escapeString(album.description) + "', "
                      "'" + escapeString(album.cover_image_id) + "', "
                      "'" + escapeString(vectorToJson(album.image_ids)) + "', "
                      "'" + escapeString(vectorToJson(album.tags)) + "', "
                      + (album.published ? "1" : "0") + ", "
                      + std::to_string(static_cast<long long>(album.created_at)) + ", "
                      + std::to_string(static_cast<long long>(album.updated_at)) + ") "
                      "ON DUPLICATE KEY UPDATE "
                      "name = VALUES(name), "
                      "description = VALUES(description), "
                      "cover_image_id = VALUES(cover_image_id), "
                      "image_ids = VALUES(image_ids), "
                      "tags = VALUES(tags), "
                      "published = VALUES(published), "
                      "updated_at = VALUES(updated_at)";

    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("Failed to execute putAlbum: {}", mysql_error(conn_));
        return false;
    }

    LOG_DEBUG("Album stored successfully: {}", album.album_id);
    return true;
}

std::optional<Album> MySQLClient::getAlbum(const std::string& album_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return std::nullopt;
    }

    std::string sql = "SELECT album_id, name, description, cover_image_id, image_ids, "
                      "tags, published, created_at, updated_at "
                      "FROM albums WHERE album_id = '" + escapeString(album_id) + "'";

    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("Failed to execute getAlbum: {}", mysql_error(conn_));
        return std::nullopt;
    }

    MYSQL_RES* result = mysql_store_result(conn_);
    if (result == nullptr) {
        LOG_ERROR("Failed to store result: {}", mysql_error(conn_));
        return std::nullopt;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == nullptr) {
        mysql_free_result(result);
        return std::nullopt;
    }

    unsigned long* lengths = mysql_fetch_lengths(result);
    Album album = extractAlbum(row, lengths);

    mysql_free_result(result);
    return album;
}

std::vector<Album> MySQLClient::listAlbums(bool published_only) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return {};
    }

    std::string sql = "SELECT album_id, name, description, cover_image_id, image_ids, "
                      "tags, published, created_at, updated_at FROM albums";

    if (published_only) {
        sql += " WHERE published = 1";
    }

    sql += " ORDER BY created_at DESC";

    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("Failed to execute listAlbums: {}", mysql_error(conn_));
        return {};
    }

    MYSQL_RES* result = mysql_store_result(conn_);
    if (result == nullptr) {
        LOG_ERROR("Failed to store result: {}", mysql_error(conn_));
        return {};
    }

    std::vector<Album> albums;
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result)) != nullptr) {
        unsigned long* lengths = mysql_fetch_lengths(result);
        albums.push_back(extractAlbum(row, lengths));
    }

    mysql_free_result(result);

    LOG_DEBUG("Listed {} albums", albums.size());
    return albums;
}

bool MySQLClient::deleteAlbum(const std::string& album_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return false;
    }

    std::string sql = "DELETE FROM albums WHERE album_id = '" + escapeString(album_id) + "'";

    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("Failed to execute deleteAlbum: {}", mysql_error(conn_));
        return false;
    }

    my_ulonglong affected = mysql_affected_rows(conn_);
    LOG_DEBUG("Album deleted: {} (rows affected: {})", album_id, affected);

    return affected > 0;
}

bool MySQLClient::albumNameExists(const std::string& name,
                                   const std::string& exclude_album_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return false;
    }

    std::string sql = "SELECT 1 FROM albums WHERE name = '" + escapeString(name) + "'";

    if (!exclude_album_id.empty()) {
        sql += " AND album_id != '" + escapeString(exclude_album_id) + "'";
    }

    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("Failed to execute albumNameExists: {}", mysql_error(conn_));
        return false;
    }

    MYSQL_RES* result = mysql_store_result(conn_);
    if (result == nullptr) {
        LOG_ERROR("Failed to store result: {}", mysql_error(conn_));
        return false;
    }

    bool exists = (mysql_fetch_row(result) != nullptr);
    mysql_free_result(result);

    return exists;
}

// Image metadata operations

bool MySQLClient::putImageMetadata(const ImageMetadata& metadata) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return false;
    }

    std::string sql = "INSERT INTO images (image_id, name, original_format, size, width, height, uploaded_at) "
                      "VALUES ('" + escapeString(metadata.image_id) + "', "
                      "'" + escapeString(metadata.name) + "', "
                      "'" + escapeString(metadata.original_format) + "', "
                      + std::to_string(static_cast<long long>(metadata.original_size)) + ", "
                      + std::to_string(metadata.width) + ", "
                      + std::to_string(metadata.height) + ", "
                      + std::to_string(static_cast<long long>(metadata.upload_timestamp)) + ") "
                      "ON DUPLICATE KEY UPDATE "
                      "name = VALUES(name), "
                      "original_format = VALUES(original_format), "
                      "size = VALUES(size), "
                      "width = VALUES(width), "
                      "height = VALUES(height), "
                      "uploaded_at = VALUES(uploaded_at)";

    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("Failed to execute putImageMetadata: {}", mysql_error(conn_));
        return false;
    }

    LOG_DEBUG("Image metadata stored successfully: {}", metadata.image_id);
    return true;
}

std::optional<ImageMetadata> MySQLClient::getImageMetadata(const std::string& image_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return std::nullopt;
    }

    std::string sql = "SELECT image_id, name, original_format, size, width, height, uploaded_at "
                      "FROM images WHERE image_id = '" + escapeString(image_id) + "'";

    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("Failed to execute getImageMetadata: {}", mysql_error(conn_));
        return std::nullopt;
    }

    MYSQL_RES* result = mysql_store_result(conn_);
    if (result == nullptr) {
        LOG_ERROR("Failed to store result: {}", mysql_error(conn_));
        return std::nullopt;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == nullptr) {
        mysql_free_result(result);
        return std::nullopt;
    }

    unsigned long* lengths = mysql_fetch_lengths(result);
    ImageMetadata metadata = extractImageMetadata(row, lengths);

    mysql_free_result(result);
    return metadata;
}

std::vector<ImageMetadata> MySQLClient::listImages(int limit, int offset,
                                                    ImageSortOrder sort_order) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return {};
    }

    std::string sql = "SELECT image_id, name, original_format, size, width, height, uploaded_at "
                      "FROM images ORDER BY " + getSortOrderSql(sort_order) +
                      " LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset);

    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("Failed to execute listImages: {}", mysql_error(conn_));
        return {};
    }

    MYSQL_RES* result = mysql_store_result(conn_);
    if (result == nullptr) {
        LOG_ERROR("Failed to store result: {}", mysql_error(conn_));
        return {};
    }

    std::vector<ImageMetadata> images;
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result)) != nullptr) {
        unsigned long* lengths = mysql_fetch_lengths(result);
        images.push_back(extractImageMetadata(row, lengths));
    }

    mysql_free_result(result);

    LOG_DEBUG("Listed {} images", images.size());
    return images;
}

int MySQLClient::getImageCount() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return 0;
    }

    const char* sql = "SELECT COUNT(*) FROM images";

    if (mysql_query(conn_, sql) != 0) {
        LOG_ERROR("Failed to execute getImageCount: {}", mysql_error(conn_));
        return 0;
    }

    MYSQL_RES* result = mysql_store_result(conn_);
    if (result == nullptr) {
        LOG_ERROR("Failed to store result: {}", mysql_error(conn_));
        return 0;
    }

    int count = 0;
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row != nullptr && row[0] != nullptr) {
        count = std::stoi(row[0]);
    }

    mysql_free_result(result);
    return count;
}

bool MySQLClient::imageExists(const std::string& image_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return false;
    }

    std::string sql = "SELECT 1 FROM images WHERE image_id = '" + escapeString(image_id) + "'";

    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("Failed to execute imageExists: {}", mysql_error(conn_));
        return false;
    }

    MYSQL_RES* result = mysql_store_result(conn_);
    if (result == nullptr) {
        LOG_ERROR("Failed to store result: {}", mysql_error(conn_));
        return false;
    }

    bool exists = (mysql_fetch_row(result) != nullptr);
    mysql_free_result(result);

    return exists;
}

} // namespace gara
