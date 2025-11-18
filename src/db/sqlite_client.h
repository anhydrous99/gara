#ifndef GARA_DB_SQLITE_CLIENT_H
#define GARA_DB_SQLITE_CLIENT_H

#include <sqlite3.h>
#include <string>
#include <memory>
#include <mutex>
#include "../interfaces/database_client_interface.h"

namespace gara {

/**
 * @brief SQLite implementation of the database client interface
 */
class SQLiteClient : public DatabaseClientInterface {
public:
    /**
     * @brief Constructor
     * @param db_path Path to the SQLite database file
     */
    explicit SQLiteClient(const std::string& db_path);

    /**
     * @brief Destructor - closes database connection
     */
    ~SQLiteClient() override;

    // Disable copy
    SQLiteClient(const SQLiteClient&) = delete;
    SQLiteClient& operator=(const SQLiteClient&) = delete;

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

private:
    sqlite3* db_;
    std::string db_path_;
    mutable std::mutex db_mutex_;  // For thread safety

    /**
     * @brief Execute SQL statement
     * @param sql SQL statement to execute
     * @return true if successful
     */
    bool executeSql(const std::string& sql);

    /**
     * @brief Helper to convert Album to JSON strings for storage
     */
    std::string vectorToJson(const std::vector<std::string>& vec);

    /**
     * @brief Helper to convert JSON strings back to vectors
     */
    std::vector<std::string> jsonToVector(const std::string& json);

    /**
     * @brief Helper to extract Album from SQLite row
     */
    Album extractAlbum(sqlite3_stmt* stmt);

    /**
     * @brief Helper to extract ImageMetadata from SQLite row
     */
    ImageMetadata extractImageMetadata(sqlite3_stmt* stmt);

    /**
     * @brief Convert ImageSortOrder to SQL ORDER BY clause
     */
    std::string getSortOrderSql(ImageSortOrder sort_order);
};

} // namespace gara

#endif // GARA_DB_SQLITE_CLIENT_H
