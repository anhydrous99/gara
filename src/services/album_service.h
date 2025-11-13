#ifndef GARA_ALBUM_SERVICE_H
#define GARA_ALBUM_SERVICE_H

#include <aws/dynamodb/DynamoDBClient.h>
#include <memory>
#include <string>
#include <vector>
#include "../models/album.h"
#include "../interfaces/dynamodb_client_interface.h"

namespace gara {

// Forward declaration
class S3Service;

class AlbumService {
public:
    /**
     * @brief Constructor with DynamoDB client injection
     * @param table_name DynamoDB table name for albums
     * @param dynamodb_client DynamoDB client interface (for dependency injection)
     * @param s3_service Optional S3 service for image validation
     */
    AlbumService(const std::string& table_name,
                 std::shared_ptr<DynamoDBClientInterface> dynamodb_client,
                 std::shared_ptr<S3Service> s3_service = nullptr);

    // CRUD operations
    Album createAlbum(const CreateAlbumRequest& request);
    Album getAlbum(const std::string& album_id);
    std::vector<Album> listAlbums(bool published_only = false);
    Album updateAlbum(const std::string& album_id, const UpdateAlbumRequest& request);
    bool deleteAlbum(const std::string& album_id);

    // Image management
    Album addImages(const std::string& album_id, const AddImagesRequest& request);
    Album removeImage(const std::string& album_id, const std::string& image_id);
    Album reorderImages(const std::string& album_id, const ReorderImagesRequest& request);

private:
    std::shared_ptr<DynamoDBClientInterface> dynamodb_client_;
    std::shared_ptr<S3Service> s3_service_;
    std::string table_name_;

    // Helper: Convert Album to DynamoDB item
    Aws::DynamoDB::Model::PutItemRequest albumToPutItemRequest(const Album& album);

    // Helper: Convert DynamoDB item to Album
    Album itemToAlbum(const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item);

    // Helper: Validate image exists in S3
    bool validateImageExists(const std::string& image_id);

    // Helper: Check if album name exists
    bool albumNameExists(const std::string& name, const std::string& exclude_album_id = "");
};

} // namespace gara

#endif // GARA_ALBUM_SERVICE_H
