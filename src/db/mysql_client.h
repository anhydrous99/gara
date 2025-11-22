#ifndef GARA_DB_MYSQL_CLIENT_H
#define GARA_DB_MYSQL_CLIENT_H

#include <mysql/mysql.h>
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include "../interfaces/database_client_interface.h"

namespace gara {

/**
 * @brief MySQL connection configuration
 */
struct MySQLConfig {
    std::string host = "localhost";
    int port = 3306;
    std::string user = "root";
    std::string password;
    std::string database = "gara";

    static MySQLConfig fromEnvironment();
};

/**
 * @brief RAII wrapper for MySQL result sets
 */
class MySQLResult {
public:
    explicit MySQLResult(MYSQL_RES* result) : result_(result) {}
    ~MySQLResult() { if (result_) mysql_free_result(result_); }

    MySQLResult(const MySQLResult&) = delete;
    MySQLResult& operator=(const MySQLResult&) = delete;

    MYSQL_RES* get() const { return result_; }
    explicit operator bool() const { return result_ != nullptr; }

    MYSQL_ROW fetchRow() { return mysql_fetch_row(result_); }
    unsigned long* fetchLengths() { return mysql_fetch_lengths(result_); }

private:
    MYSQL_RES* result_;
};

/**
 * @brief MySQL implementation of the database client interface
 */
class MySQLClient : public DatabaseClientInterface {
public:
    explicit MySQLClient(const MySQLConfig& config);
    ~MySQLClient() override;

    MySQLClient(const MySQLClient&) = delete;
    MySQLClient& operator=(const MySQLClient&) = delete;

    // DatabaseClientInterface implementation
    bool putAlbum(const Album& album) override;
    std::optional<Album> getAlbum(const std::string& album_id) override;
    std::vector<Album> listAlbums(bool published_only = false) override;
    bool deleteAlbum(const std::string& album_id) override;
    bool albumNameExists(const std::string& name,
                        const std::string& exclude_album_id = "") override;

    bool putImageMetadata(const ImageMetadata& metadata) override;
    std::optional<ImageMetadata> getImageMetadata(const std::string& image_id) override;
    std::vector<ImageMetadata> listImages(int limit, int offset,
                                          ImageSortOrder sort_order) override;
    int getImageCount() override;
    bool imageExists(const std::string& image_id) override;

    bool initialize();
    bool isConnected() const;

private:
    MYSQL* conn_;
    MySQLConfig config_;
    mutable std::mutex db_mutex_;

    // Column indices for albums table
    struct AlbumColumns {
        static constexpr int ID = 0;
        static constexpr int NAME = 1;
        static constexpr int DESCRIPTION = 2;
        static constexpr int COVER_IMAGE_ID = 3;
        static constexpr int IMAGE_IDS = 4;
        static constexpr int TAGS = 5;
        static constexpr int PUBLISHED = 6;
        static constexpr int CREATED_AT = 7;
        static constexpr int UPDATED_AT = 8;
    };

    // Column indices for images table
    struct ImageColumns {
        static constexpr int ID = 0;
        static constexpr int NAME = 1;
        static constexpr int FORMAT = 2;
        static constexpr int SIZE = 3;
        static constexpr int WIDTH = 4;
        static constexpr int HEIGHT = 5;
        static constexpr int UPLOADED_AT = 6;
    };

    bool reconnectIfNeeded();
    std::string escapeString(const std::string& str);

    // JSON helpers
    static std::string vectorToJson(const std::vector<std::string>& vec);
    static std::vector<std::string> jsonToVector(const std::string& json_str);

    // Row extraction helpers
    static std::string getSafeString(MYSQL_ROW row, int index, unsigned long* lengths);
    Album extractAlbum(MYSQL_ROW row, unsigned long* lengths);
    ImageMetadata extractImageMetadata(MYSQL_ROW row, unsigned long* lengths);

    static std::string getSortOrderSql(ImageSortOrder sort_order);

    // Query execution helpers
    bool executeQuery(const std::string& sql);
    MySQLResult executeSelect(const std::string& sql);

    template<typename T>
    std::optional<T> executeSingleRowQuery(
        const std::string& sql,
        std::function<T(MYSQL_ROW, unsigned long*)> extractor);

    template<typename T>
    std::vector<T> executeMultiRowQuery(
        const std::string& sql,
        std::function<T(MYSQL_ROW, unsigned long*)> extractor);
};

} // namespace gara

#endif // GARA_DB_MYSQL_CLIENT_H
