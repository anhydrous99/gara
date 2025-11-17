#ifndef GARA_ALBUM_SERVICE_H
#define GARA_ALBUM_SERVICE_H

#include <memory>
#include <string>
#include <vector>
#include "../models/album.h"
#include "../interfaces/database_client_interface.h"

namespace gara {

// Forward declaration
class FileServiceInterface;

class AlbumService {
public:
    /**
     * @brief Constructor with database client injection
     * @param db_client Database client interface (for dependency injection)
     * @param file_service Optional file service for image validation
     */
    AlbumService(std::shared_ptr<DatabaseClientInterface> db_client,
                 std::shared_ptr<FileServiceInterface> file_service = nullptr);

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
    std::shared_ptr<DatabaseClientInterface> db_client_;
    std::shared_ptr<FileServiceInterface> file_service_;

    // Helper: Validate image exists in storage
    bool validateImageExists(const std::string& image_id);
};

} // namespace gara

#endif // GARA_ALBUM_SERVICE_H
