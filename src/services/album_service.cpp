#include "album_service.h"
#include "../interfaces/file_service_interface.h"
#include "../constants/album_constants.h"
#include "../exceptions/album_exceptions.h"
#include "../utils/id_generator.h"
#include "../utils/logger.h"
#include "../utils/metrics.h"
#include <stdexcept>
#include <algorithm>

namespace gara {

AlbumService::AlbumService(std::shared_ptr<DatabaseClientInterface> db_client,
                           std::shared_ptr<FileServiceInterface> file_service)
    : db_client_(db_client), file_service_(file_service) {
}

bool AlbumService::validateImageExists(const std::string& image_id) {
    if (!file_service_) {
        // If no file service provided (e.g., in tests), skip validation
        return true;
    }

    // Try common image formats
    for (const auto& format : constants::SUPPORTED_IMAGE_FORMATS) {
        std::string key = "raw/" + image_id + "." + format;
        if (file_service_->objectExists(key)) {
            return true;
        }
    }
    return false;
}

Album AlbumService::createAlbum(const CreateAlbumRequest& request) {
    auto timer = gara::Metrics::get()->start_timer("AlbumOperationDuration",
                                                   {{"operation", "create"}});

    // Validate input
    if (request.name.empty()) {
        gara::Logger::log_structured(spdlog::level::warn, "Album creation failed: empty name", {
            {"operation", "createAlbum"}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "create"}, {"status", "validation_error"}});
        throw exceptions::ValidationException("Album name cannot be empty");
    }

    // Check if name already exists
    if (db_client_->albumNameExists(request.name)) {
        gara::Logger::log_structured(spdlog::level::warn, "Album creation failed: duplicate name", {
            {"operation", "createAlbum"},
            {"name", request.name}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "create"}, {"status", "conflict"}});
        throw exceptions::ConflictException("Album with name '" + request.name + "' already exists");
    }

    // Create album
    Album album;
    album.album_id = utils::IdGenerator::generateAlbumId();
    album.name = request.name;
    album.description = request.description;
    album.cover_image_id = "";  // Not provided in CreateAlbumRequest
    album.image_ids = {};  // Not provided in CreateAlbumRequest
    album.tags = request.tags;
    album.published = request.published;
    album.created_at = std::time(nullptr);
    album.updated_at = album.created_at;

    // Store in database
    if (!db_client_->putAlbum(album)) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to create album in database", {
            {"operation", "createAlbum"},
            {"album_id", album.album_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "create"}, {"status", "error"}});
        throw std::runtime_error("Failed to create album");
    }

    gara::Logger::log_structured(spdlog::level::info, "Album created successfully", {
        {"operation", "createAlbum"},
        {"album_id", album.album_id},
        {"name", album.name}
    });
    METRICS_COUNT("AlbumOperations", 1.0, "Count",
                 {{"operation", "create"}, {"status", "success"}});

    return album;
}

Album AlbumService::getAlbum(const std::string& album_id) {
    auto timer = gara::Metrics::get()->start_timer("AlbumOperationDuration",
                                                   {{"operation", "get"}});

    auto album_opt = db_client_->getAlbum(album_id);

    if (!album_opt) {
        gara::Logger::log_structured(spdlog::level::warn, "Album not found", {
            {"operation", "getAlbum"},
            {"album_id", album_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "get"}, {"status", "not_found"}});
        throw exceptions::NotFoundException("Album not found: " + album_id);
    }

    METRICS_COUNT("AlbumOperations", 1.0, "Count",
                 {{"operation", "get"}, {"status", "success"}});
    return *album_opt;
}

std::vector<Album> AlbumService::listAlbums(bool published_only) {
    auto timer = gara::Metrics::get()->start_timer("AlbumOperationDuration",
                                                   {{"operation", "list"}});

    auto albums = db_client_->listAlbums(published_only);

    gara::Logger::log_structured(spdlog::level::debug, "Albums listed", {
        {"operation", "listAlbums"},
        {"count", std::to_string(albums.size())},
        {"published_only", published_only ? "true" : "false"}
    });

    METRICS_COUNT("AlbumOperations", 1.0, "Count",
                 {{"operation", "list"}, {"status", "success"}});

    return albums;
}

Album AlbumService::updateAlbum(const std::string& album_id, const UpdateAlbumRequest& request) {
    auto timer = gara::Metrics::get()->start_timer("AlbumOperationDuration",
                                                   {{"operation", "update"}});

    // Get existing album
    auto album_opt = db_client_->getAlbum(album_id);
    if (!album_opt) {
        gara::Logger::log_structured(spdlog::level::warn, "Album update failed: not found", {
            {"operation", "updateAlbum"},
            {"album_id", album_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "update"}, {"status", "not_found"}});
        throw exceptions::NotFoundException("Album not found: " + album_id);
    }

    Album album = *album_opt;

    // Update fields if provided (non-empty means provided in UpdateAlbumRequest)
    if (!request.name.empty()) {
        if (request.name != album.name && db_client_->albumNameExists(request.name, album_id)) {
            gara::Logger::log_structured(spdlog::level::warn, "Album update failed: duplicate name", {
                {"operation", "updateAlbum"},
                {"album_id", album_id},
                {"name", request.name}
            });
            METRICS_COUNT("AlbumOperations", 1.0, "Count",
                         {{"operation", "update"}, {"status", "conflict"}});
            throw exceptions::ConflictException("Album with name '" + request.name + "' already exists");
        }
        album.name = request.name;
    }

    if (!request.description.empty()) {
        album.description = request.description;
    }

    if (!request.cover_image_id.empty()) {
        if (!validateImageExists(request.cover_image_id)) {
            gara::Logger::log_structured(spdlog::level::warn, "Album update failed: cover image not found", {
                {"operation", "updateAlbum"},
                {"album_id", album_id},
                {"cover_image_id", request.cover_image_id}
            });
            METRICS_COUNT("AlbumOperations", 1.0, "Count",
                         {{"operation", "update"}, {"status", "validation_error"}});
            throw exceptions::ValidationException("Cover image not found: " + request.cover_image_id);
        }
        album.cover_image_id = request.cover_image_id;
    }

    if (!request.tags.empty()) {
        album.tags = request.tags;
    }

    // Note: published is a bool, so we always update it from the request
    album.published = request.published;

    album.updated_at = std::time(nullptr);

    // Save updated album
    if (!db_client_->putAlbum(album)) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to update album in database", {
            {"operation", "updateAlbum"},
            {"album_id", album_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "update"}, {"status", "error"}});
        throw std::runtime_error("Failed to update album");
    }

    gara::Logger::log_structured(spdlog::level::info, "Album updated successfully", {
        {"operation", "updateAlbum"},
        {"album_id", album_id}
    });
    METRICS_COUNT("AlbumOperations", 1.0, "Count",
                 {{"operation", "update"}, {"status", "success"}});

    return album;
}

bool AlbumService::deleteAlbum(const std::string& album_id) {
    auto timer = gara::Metrics::get()->start_timer("AlbumOperationDuration",
                                                   {{"operation", "delete"}});

    // Check if album exists
    auto album_opt = db_client_->getAlbum(album_id);
    if (!album_opt) {
        gara::Logger::log_structured(spdlog::level::warn, "Album deletion failed: not found", {
            {"operation", "deleteAlbum"},
            {"album_id", album_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "delete"}, {"status", "not_found"}});
        throw exceptions::NotFoundException("Album not found: " + album_id);
    }

    // Delete from database
    bool success = db_client_->deleteAlbum(album_id);

    if (success) {
        gara::Logger::log_structured(spdlog::level::info, "Album deleted successfully", {
            {"operation", "deleteAlbum"},
            {"album_id", album_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "delete"}, {"status", "success"}});
    } else {
        gara::Logger::log_structured(spdlog::level::err, "Failed to delete album from database", {
            {"operation", "deleteAlbum"},
            {"album_id", album_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "delete"}, {"status", "error"}});
    }

    return success;
}

Album AlbumService::addImages(const std::string& album_id, const AddImagesRequest& request) {
    auto timer = gara::Metrics::get()->start_timer("AlbumOperationDuration",
                                                   {{"operation", "add_images"}});

    // Get existing album
    auto album_opt = db_client_->getAlbum(album_id);
    if (!album_opt) {
        gara::Logger::log_structured(spdlog::level::warn, "Add images failed: album not found", {
            {"operation", "addImages"},
            {"album_id", album_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "add_images"}, {"status", "not_found"}});
        throw exceptions::NotFoundException("Album not found: " + album_id);
    }

    Album album = *album_opt;

    // Validate all new image IDs exist
    for (const auto& image_id : request.image_ids) {
        if (!validateImageExists(image_id)) {
            gara::Logger::log_structured(spdlog::level::warn, "Add images failed: image not found", {
                {"operation", "addImages"},
                {"album_id", album_id},
                {"image_id", image_id}
            });
            METRICS_COUNT("AlbumOperations", 1.0, "Count",
                         {{"operation", "add_images"}, {"status", "validation_error"}});
            throw exceptions::ValidationException("Image not found: " + image_id);
        }
    }

    // Add images at specified position or append (position=-1 means append)
    if (request.position >= 0 && request.position <= static_cast<int>(album.image_ids.size())) {
        album.image_ids.insert(
            album.image_ids.begin() + request.position,
            request.image_ids.begin(),
            request.image_ids.end()
        );
    } else {
        album.image_ids.insert(
            album.image_ids.end(),
            request.image_ids.begin(),
            request.image_ids.end()
        );
    }

    album.updated_at = std::time(nullptr);

    // Save updated album
    if (!db_client_->putAlbum(album)) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to add images to album", {
            {"operation", "addImages"},
            {"album_id", album_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "add_images"}, {"status", "error"}});
        throw std::runtime_error("Failed to add images to album");
    }

    gara::Logger::log_structured(spdlog::level::info, "Images added to album successfully", {
        {"operation", "addImages"},
        {"album_id", album_id},
        {"count", std::to_string(request.image_ids.size())}
    });
    METRICS_COUNT("AlbumOperations", 1.0, "Count",
                 {{"operation", "add_images"}, {"status", "success"}});

    return album;
}

Album AlbumService::removeImage(const std::string& album_id, const std::string& image_id) {
    auto timer = gara::Metrics::get()->start_timer("AlbumOperationDuration",
                                                   {{"operation", "remove_image"}});

    // Get existing album
    auto album_opt = db_client_->getAlbum(album_id);
    if (!album_opt) {
        gara::Logger::log_structured(spdlog::level::warn, "Remove image failed: album not found", {
            {"operation", "removeImage"},
            {"album_id", album_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "remove_image"}, {"status", "not_found"}});
        throw exceptions::NotFoundException("Album not found: " + album_id);
    }

    Album album = *album_opt;

    // Find and remove the image
    auto it = std::find(album.image_ids.begin(), album.image_ids.end(), image_id);
    if (it == album.image_ids.end()) {
        gara::Logger::log_structured(spdlog::level::warn, "Remove image failed: image not in album", {
            {"operation", "removeImage"},
            {"album_id", album_id},
            {"image_id", image_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "remove_image"}, {"status", "not_found"}});
        throw exceptions::NotFoundException("Image not found in album: " + image_id);
    }

    album.image_ids.erase(it);
    album.updated_at = std::time(nullptr);

    // Save updated album
    if (!db_client_->putAlbum(album)) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to remove image from album", {
            {"operation", "removeImage"},
            {"album_id", album_id},
            {"image_id", image_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "remove_image"}, {"status", "error"}});
        throw std::runtime_error("Failed to remove image from album");
    }

    gara::Logger::log_structured(spdlog::level::info, "Image removed from album successfully", {
        {"operation", "removeImage"},
        {"album_id", album_id},
        {"image_id", image_id}
    });
    METRICS_COUNT("AlbumOperations", 1.0, "Count",
                 {{"operation", "remove_image"}, {"status", "success"}});

    return album;
}

Album AlbumService::reorderImages(const std::string& album_id, const ReorderImagesRequest& request) {
    auto timer = gara::Metrics::get()->start_timer("AlbumOperationDuration",
                                                   {{"operation", "reorder_images"}});

    // Get existing album
    auto album_opt = db_client_->getAlbum(album_id);
    if (!album_opt) {
        gara::Logger::log_structured(spdlog::level::warn, "Reorder images failed: album not found", {
            {"operation", "reorderImages"},
            {"album_id", album_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "reorder_images"}, {"status", "not_found"}});
        throw exceptions::NotFoundException("Album not found: " + album_id);
    }

    Album album = *album_opt;

    // Validate new order has same images
    if (request.image_ids.size() != album.image_ids.size()) {
        gara::Logger::log_structured(spdlog::level::warn, "Reorder images failed: size mismatch", {
            {"operation", "reorderImages"},
            {"album_id", album_id},
            {"expected", std::to_string(album.image_ids.size())},
            {"provided", std::to_string(request.image_ids.size())}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "reorder_images"}, {"status", "validation_error"}});
        throw exceptions::ValidationException("New order must contain all existing images");
    }

    // Validate all images are present
    for (const auto& image_id : request.image_ids) {
        if (std::find(album.image_ids.begin(), album.image_ids.end(), image_id) == album.image_ids.end()) {
            gara::Logger::log_structured(spdlog::level::warn, "Reorder images failed: unknown image", {
                {"operation", "reorderImages"},
                {"album_id", album_id},
                {"image_id", image_id}
            });
            METRICS_COUNT("AlbumOperations", 1.0, "Count",
                         {{"operation", "reorder_images"}, {"status", "validation_error"}});
            throw exceptions::ValidationException("Image not in album: " + image_id);
        }
    }

    album.image_ids = request.image_ids;
    album.updated_at = std::time(nullptr);

    // Save updated album
    if (!db_client_->putAlbum(album)) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to reorder images in album", {
            {"operation", "reorderImages"},
            {"album_id", album_id}
        });
        METRICS_COUNT("AlbumOperations", 1.0, "Count",
                     {{"operation", "reorder_images"}, {"status", "error"}});
        throw std::runtime_error("Failed to reorder images in album");
    }

    gara::Logger::log_structured(spdlog::level::info, "Images reordered successfully", {
        {"operation", "reorderImages"},
        {"album_id", album_id}
    });
    METRICS_COUNT("AlbumOperations", 1.0, "Count",
                 {{"operation", "reorder_images"}, {"status", "success"}});

    return album;
}

} // namespace gara
