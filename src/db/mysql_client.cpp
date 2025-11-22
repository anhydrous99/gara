#include "mysql_client.h"
#include "../utils/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

using json = nlohmann::json;

namespace gara {

namespace {
    constexpr unsigned int CONNECTION_TIMEOUT_SECONDS = 10;
    constexpr int DEFAULT_MYSQL_PORT = 3306;
}

// MySQLConfig implementation

MySQLConfig MySQLConfig::fromEnvironment() {
    MySQLConfig config;

    if (const char* host = std::getenv("MYSQL_HOST")) {
        config.host = host;
    }
    if (const char* port = std::getenv("MYSQL_PORT")) {
        try {
            config.port = std::stoi(port);
        } catch (...) {
            LOG_WARN("Invalid MYSQL_PORT value, using default {}", DEFAULT_MYSQL_PORT);
        }
    }
    if (const char* user = std::getenv("MYSQL_USER")) {
        config.user = user;
    }
    if (const char* password = std::getenv("MYSQL_PASSWORD")) {
        config.password = password;
    }
    if (const char* database = std::getenv("MYSQL_DATABASE")) {
        config.database = database;
    }

    return config;
}

// MySQLClient implementation

MySQLClient::MySQLClient(const MySQLConfig& config)
    : conn_(nullptr), config_(config) {

    conn_ = mysql_init(nullptr);
    if (conn_ == nullptr) {
        throw std::runtime_error("Failed to initialize MySQL connection");
    }

    bool reconnect = true;
    mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnect);

    unsigned int timeout = CONNECTION_TIMEOUT_SECONDS;
    mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    if (mysql_real_connect(conn_, config_.host.c_str(), config_.user.c_str(),
                           config_.password.c_str(), config_.database.c_str(),
                           config_.port, nullptr, 0) == nullptr) {
        std::string error = mysql_error(conn_);
        mysql_close(conn_);
        conn_ = nullptr;
        throw std::runtime_error("Failed to connect to MySQL: " + error);
    }

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

    std::ifstream schema_file("src/db/schema_mysql.sql");
    if (!schema_file.is_open()) {
        LOG_ERROR("Failed to open schema_mysql.sql file");
        return false;
    }

    std::stringstream buffer;
    buffer << schema_file.rdbuf();
    std::string schema_sql = buffer.str();

    std::istringstream stream(schema_sql);
    std::string statement;

    while (std::getline(stream, statement, ';')) {
        size_t start = statement.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) continue;

        size_t end = statement.find_last_not_of(" \t\n\r");
        statement = statement.substr(start, end - start + 1);

        if (statement.empty() || statement[0] == '-') continue;

        if (mysql_query(conn_, statement.c_str()) != 0) {
            LOG_ERROR("Failed to execute schema statement: {}", mysql_error(conn_));
            return false;
        }
    }

    LOG_INFO("MySQL database schema initialized successfully");
    return true;
}

// Helper methods

std::string MySQLClient::escapeString(const std::string& str) {
    if (conn_ == nullptr) {
        return str;
    }

    std::vector<char> buffer(str.length() * 2 + 1);
    unsigned long len = mysql_real_escape_string(conn_, buffer.data(), str.c_str(), str.length());
    return std::string(buffer.data(), len);
}

std::string MySQLClient::vectorToJson(const std::vector<std::string>& vec) {
    return json(vec).dump();
}

std::vector<std::string> MySQLClient::jsonToVector(const std::string& json_str) {
    if (json_str.empty()) {
        return {};
    }
    try {
        return json::parse(json_str).get<std::vector<std::string>>();
    } catch (const json::exception& e) {
        LOG_ERROR("Failed to parse JSON: {}", e.what());
        return {};
    }
}

std::string MySQLClient::getSafeString(MYSQL_ROW row, int index, unsigned long* lengths) {
    if (row[index] == nullptr) {
        return "";
    }
    return std::string(row[index], lengths[index]);
}

std::string MySQLClient::getSortOrderSql(ImageSortOrder sort_order) {
    switch (sort_order) {
        case ImageSortOrder::NEWEST:   return "uploaded_at DESC";
        case ImageSortOrder::OLDEST:   return "uploaded_at ASC";
        case ImageSortOrder::NAME_ASC: return "name ASC";
        case ImageSortOrder::NAME_DESC: return "name DESC";
        default: return "uploaded_at DESC";
    }
}

// Query execution helpers

bool MySQLClient::executeQuery(const std::string& sql) {
    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("SQL execution failed: {}", mysql_error(conn_));
        return false;
    }
    return true;
}

MySQLResult MySQLClient::executeSelect(const std::string& sql) {
    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("SELECT execution failed: {}", mysql_error(conn_));
        return MySQLResult(nullptr);
    }

    MYSQL_RES* result = mysql_store_result(conn_);
    if (result == nullptr) {
        LOG_ERROR("Failed to store result: {}", mysql_error(conn_));
    }
    return MySQLResult(result);
}

template<typename T>
std::optional<T> MySQLClient::executeSingleRowQuery(
    const std::string& sql,
    std::function<T(MYSQL_ROW, unsigned long*)> extractor) {

    MySQLResult result = executeSelect(sql);
    if (!result) {
        return std::nullopt;
    }

    MYSQL_ROW row = result.fetchRow();
    if (row == nullptr) {
        return std::nullopt;
    }

    return extractor(row, result.fetchLengths());
}

template<typename T>
std::vector<T> MySQLClient::executeMultiRowQuery(
    const std::string& sql,
    std::function<T(MYSQL_ROW, unsigned long*)> extractor) {

    MySQLResult result = executeSelect(sql);
    if (!result) {
        return {};
    }

    std::vector<T> items;
    MYSQL_ROW row;
    while ((row = result.fetchRow()) != nullptr) {
        items.push_back(extractor(row, result.fetchLengths()));
    }

    return items;
}

// Row extraction

Album MySQLClient::extractAlbum(MYSQL_ROW row, unsigned long* lengths) {
    Album album;

    album.album_id = getSafeString(row, AlbumColumns::ID, lengths);
    album.name = getSafeString(row, AlbumColumns::NAME, lengths);
    album.description = getSafeString(row, AlbumColumns::DESCRIPTION, lengths);
    album.cover_image_id = getSafeString(row, AlbumColumns::COVER_IMAGE_ID, lengths);

    std::string image_ids_json = getSafeString(row, AlbumColumns::IMAGE_IDS, lengths);
    album.image_ids = jsonToVector(image_ids_json.empty() ? "[]" : image_ids_json);

    std::string tags_json = getSafeString(row, AlbumColumns::TAGS, lengths);
    album.tags = jsonToVector(tags_json.empty() ? "[]" : tags_json);

    album.published = row[AlbumColumns::PUBLISHED] != nullptr &&
                      std::string(row[AlbumColumns::PUBLISHED]) == "1";

    if (row[AlbumColumns::CREATED_AT] != nullptr) {
        album.created_at = static_cast<std::time_t>(std::stoll(row[AlbumColumns::CREATED_AT]));
    }
    if (row[AlbumColumns::UPDATED_AT] != nullptr) {
        album.updated_at = static_cast<std::time_t>(std::stoll(row[AlbumColumns::UPDATED_AT]));
    }

    return album;
}

ImageMetadata MySQLClient::extractImageMetadata(MYSQL_ROW row, unsigned long* lengths) {
    ImageMetadata metadata;

    metadata.image_id = getSafeString(row, ImageColumns::ID, lengths);
    metadata.name = getSafeString(row, ImageColumns::NAME, lengths);
    metadata.original_format = getSafeString(row, ImageColumns::FORMAT, lengths);

    if (row[ImageColumns::SIZE] != nullptr) {
        metadata.original_size = static_cast<size_t>(std::stoull(row[ImageColumns::SIZE]));
    }
    if (row[ImageColumns::WIDTH] != nullptr) {
        metadata.width = std::stoi(row[ImageColumns::WIDTH]);
    }
    if (row[ImageColumns::HEIGHT] != nullptr) {
        metadata.height = std::stoi(row[ImageColumns::HEIGHT]);
    }
    if (row[ImageColumns::UPLOADED_AT] != nullptr) {
        metadata.upload_timestamp = static_cast<std::time_t>(std::stoll(row[ImageColumns::UPLOADED_AT]));
    }

    return metadata;
}

// Album operations

bool MySQLClient::putAlbum(const Album& album) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return false;
    }

    std::ostringstream sql;
    sql << "INSERT INTO albums (album_id, name, description, cover_image_id, "
        << "image_ids, tags, published, created_at, updated_at) VALUES ("
        << "'" << escapeString(album.album_id) << "', "
        << "'" << escapeString(album.name) << "', "
        << "'" << escapeString(album.description) << "', "
        << "'" << escapeString(album.cover_image_id) << "', "
        << "'" << escapeString(vectorToJson(album.image_ids)) << "', "
        << "'" << escapeString(vectorToJson(album.tags)) << "', "
        << (album.published ? 1 : 0) << ", "
        << static_cast<long long>(album.created_at) << ", "
        << static_cast<long long>(album.updated_at) << ") "
        << "ON DUPLICATE KEY UPDATE "
        << "name = VALUES(name), "
        << "description = VALUES(description), "
        << "cover_image_id = VALUES(cover_image_id), "
        << "image_ids = VALUES(image_ids), "
        << "tags = VALUES(tags), "
        << "published = VALUES(published), "
        << "updated_at = VALUES(updated_at)";

    if (!executeQuery(sql.str())) {
        LOG_ERROR("Failed to execute putAlbum for: {}", album.album_id);
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

    return executeSingleRowQuery<Album>(sql,
        [this](MYSQL_ROW row, unsigned long* lengths) {
            return extractAlbum(row, lengths);
        });
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

    auto albums = executeMultiRowQuery<Album>(sql,
        [this](MYSQL_ROW row, unsigned long* lengths) {
            return extractAlbum(row, lengths);
        });

    LOG_DEBUG("Listed {} albums", albums.size());
    return albums;
}

bool MySQLClient::deleteAlbum(const std::string& album_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return false;
    }

    std::string sql = "DELETE FROM albums WHERE album_id = '" + escapeString(album_id) + "'";

    if (!executeQuery(sql)) {
        return false;
    }

    my_ulonglong affected = mysql_affected_rows(conn_);
    LOG_DEBUG("Album deleted: {} (rows affected: {})", album_id, affected);

    return affected > 0;
}

bool MySQLClient::albumNameExists(const std::string& name, const std::string& exclude_album_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return false;
    }

    std::string sql = "SELECT 1 FROM albums WHERE name = '" + escapeString(name) + "'";
    if (!exclude_album_id.empty()) {
        sql += " AND album_id != '" + escapeString(exclude_album_id) + "'";
    }

    MySQLResult result = executeSelect(sql);
    return result && result.fetchRow() != nullptr;
}

// Image metadata operations

bool MySQLClient::putImageMetadata(const ImageMetadata& metadata) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return false;
    }

    std::ostringstream sql;
    sql << "INSERT INTO images (image_id, name, original_format, size, width, height, uploaded_at) VALUES ("
        << "'" << escapeString(metadata.image_id) << "', "
        << "'" << escapeString(metadata.name) << "', "
        << "'" << escapeString(metadata.original_format) << "', "
        << static_cast<long long>(metadata.original_size) << ", "
        << metadata.width << ", "
        << metadata.height << ", "
        << static_cast<long long>(metadata.upload_timestamp) << ") "
        << "ON DUPLICATE KEY UPDATE "
        << "name = VALUES(name), "
        << "original_format = VALUES(original_format), "
        << "size = VALUES(size), "
        << "width = VALUES(width), "
        << "height = VALUES(height), "
        << "uploaded_at = VALUES(uploaded_at)";

    if (!executeQuery(sql.str())) {
        LOG_ERROR("Failed to execute putImageMetadata for: {}", metadata.image_id);
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

    return executeSingleRowQuery<ImageMetadata>(sql,
        [this](MYSQL_ROW row, unsigned long* lengths) {
            return extractImageMetadata(row, lengths);
        });
}

std::vector<ImageMetadata> MySQLClient::listImages(int limit, int offset, ImageSortOrder sort_order) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return {};
    }

    std::ostringstream sql;
    sql << "SELECT image_id, name, original_format, size, width, height, uploaded_at "
        << "FROM images ORDER BY " << getSortOrderSql(sort_order)
        << " LIMIT " << limit << " OFFSET " << offset;

    auto images = executeMultiRowQuery<ImageMetadata>(sql.str(),
        [this](MYSQL_ROW row, unsigned long* lengths) {
            return extractImageMetadata(row, lengths);
        });

    LOG_DEBUG("Listed {} images", images.size());
    return images;
}

int MySQLClient::getImageCount() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return 0;
    }

    MySQLResult result = executeSelect("SELECT COUNT(*) FROM images");
    if (!result) {
        return 0;
    }

    MYSQL_ROW row = result.fetchRow();
    if (row == nullptr || row[0] == nullptr) {
        return 0;
    }

    return std::stoi(row[0]);
}

bool MySQLClient::imageExists(const std::string& image_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!reconnectIfNeeded()) {
        return false;
    }

    std::string sql = "SELECT 1 FROM images WHERE image_id = '" + escapeString(image_id) + "'";

    MySQLResult result = executeSelect(sql);
    return result && result.fetchRow() != nullptr;
}

} // namespace gara
