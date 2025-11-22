#ifndef GARA_DB_MYSQL_CLIENT_H
#define GARA_DB_MYSQL_CLIENT_H

#include <mysql/mysql.h>
#include <string>
#include <memory>
#include <mutex>
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

    /**
     * @brief Create config from environment variables
     */
    static MySQLConfig fromEnvironment();
};

/**
 * @brief MySQL implementation of the database client interface
 */
class MySQLClient : public DatabaseClientInterface {
public:
    /**
     * @brief Constructor
     * @param config MySQL connection configuration
     */
    explicit MySQLClient(const MySQLConfig& config);

    /**
     * @brief Destructor - closes database connection
     */
    ~MySQLClient() override;

    // Disable copy
    MySQLClient(const MySQLClient&) = delete;
    MySQLClient& operator=(const MySQLClient&) = delete;

    // DatabaseClientInterface implementation
    bool putAlbum(const Album& album) override;
    std::optional<Album> getAlbum(const std::string& album_id) override;
    std::vector<Album> listAlbums(bool published_only = false) override;
    bool deleteAlbum(const std::string& album_id) override;
    bool albumNameExists(const std::string& name,
                        const std::string& exclude_album_id = "") override;

    // Image metadata operations
    bool putImageMetadata(const ImageMetadata& metadata) override;
    std::optional<ImageMetadata> getImageMetadata(const std::string& image_id) override;
    std::vector<ImageMetadata> listImages(int limit, int offset,
                                          ImageSortOrder sort_order) override;
    int getImageCount() override;
    bool imageExists(const std::string& image_id) override;

    /**
     * @brief Initialize the database schema
     * @return true if successful
     */
    bool initialize();

    /**
     * @brief Check if connected to the database
     * @return true if connected
     */
    bool isConnected() const;

private:
    MYSQL* conn_;
    MySQLConfig config_;
    mutable std::mutex db_mutex_;  // For thread safety

    /**
     * @brief Execute SQL statement
     * @param sql SQL statement to execute
     * @return true if successful
     */
    bool executeSql(const std::string& sql);

    /**
     * @brief Reconnect to the database if connection was lost
     * @return true if reconnected successfully
     */
    bool reconnectIfNeeded();

    /**
     * @brief Helper to convert vector to JSON string for storage
     */
    std::string vectorToJson(const std::vector<std::string>& vec);

    /**
     * @brief Helper to convert JSON string back to vector
     */
    std::vector<std::string> jsonToVector(const std::string& json);

    /**
     * @brief Helper to extract Album from MySQL result row
     */
    Album extractAlbum(MYSQL_ROW row, unsigned long* lengths);

    /**
     * @brief Helper to extract ImageMetadata from MySQL result row
     */
    ImageMetadata extractImageMetadata(MYSQL_ROW row, unsigned long* lengths);

    /**
     * @brief Convert ImageSortOrder to SQL ORDER BY clause
     */
    std::string getSortOrderSql(ImageSortOrder sort_order);

    /**
     * @brief Escape string for MySQL queries
     */
    std::string escapeString(const std::string& str);

    /**
     * @brief Get safe string from result, handling NULL values
     */
    std::string getSafeString(MYSQL_ROW row, int index, unsigned long* lengths);
};

} // namespace gara

#endif // GARA_DB_MYSQL_CLIENT_H
