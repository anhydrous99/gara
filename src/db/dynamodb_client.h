#ifndef GARA_DB_DYNAMODB_CLIENT_H
#define GARA_DB_DYNAMODB_CLIENT_H

#include <aws/core/Aws.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <string>
#include <memory>
#include <mutex>
#include "../interfaces/database_client_interface.h"

namespace gara {

/**
 * @brief DynamoDB connection configuration
 */
struct DynamoDBConfig {
    std::string region = "us-east-1";
    std::string endpoint_url;  // Optional: for local DynamoDB (e.g., http://localhost:8000)
    std::string albums_table = "gara_albums";
    std::string images_table = "gara_images";

    static DynamoDBConfig fromEnvironment();
};

/**
 * @brief DynamoDB implementation of the database client interface
 */
class DynamoDBClient : public DatabaseClientInterface {
public:
    explicit DynamoDBClient(const DynamoDBConfig& config);
    ~DynamoDBClient() override;

    DynamoDBClient(const DynamoDBClient&) = delete;
    DynamoDBClient& operator=(const DynamoDBClient&) = delete;

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

    /**
     * @brief Initialize the DynamoDB client and verify table access
     * @return true if initialization successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Check if the client is connected and tables are accessible
     * @return true if connected, false otherwise
     */
    bool isConnected() const;

private:
    DynamoDBConfig config_;
    std::unique_ptr<Aws::DynamoDB::DynamoDBClient> client_;
    mutable std::mutex db_mutex_;
    bool initialized_ = false;

    // AWS SDK lifecycle management
    static std::atomic<int> sdk_init_count_;
    static std::mutex sdk_init_mutex_;

    // Helper methods for data conversion
    static std::string vectorToJson(const std::vector<std::string>& vec);
    static std::vector<std::string> jsonToVector(const std::string& json_str);

    // Row extraction helpers
    Album extractAlbum(const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item);
    ImageMetadata extractImageMetadata(const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item);

    // Helper to get string attribute safely
    static std::string getStringAttribute(
        const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item,
        const std::string& key,
        const std::string& default_value = "");

    // Helper to get numeric attribute safely
    static long long getNumberAttribute(
        const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item,
        const std::string& key,
        long long default_value = 0);

    // Helper to get boolean attribute safely
    static bool getBoolAttribute(
        const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item,
        const std::string& key,
        bool default_value = false);

    // Check if a table exists
    bool tableExists(const std::string& table_name);

    // Create tables if they don't exist
    bool createTablesIfNotExist();
};

} // namespace gara

#endif // GARA_DB_DYNAMODB_CLIENT_H
