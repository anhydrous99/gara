#ifndef GARA_TESTS_MOCKS_FAKE_DATABASE_CLIENT_H
#define GARA_TESTS_MOCKS_FAKE_DATABASE_CLIENT_H

#include "../../src/interfaces/database_client_interface.h"
#include "../../src/models/album.h"
#include <map>
#include <string>
#include <mutex>
#include <optional>
#include <algorithm>

namespace gara {
namespace testing {

/**
 * @brief In-memory fake database client for testing
 *
 * Simulates database storage without actual SQLite or cloud database
 */
class FakeDatabaseClient : public DatabaseClientInterface {
public:
    FakeDatabaseClient() = default;

    bool putAlbum(const Album& album) override {
        std::lock_guard<std::mutex> lock(mutex_);
        albums_[album.album_id] = album;
        return true;
    }

    std::optional<Album> getAlbum(const std::string& album_id) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = albums_.find(album_id);
        if (it != albums_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<Album> listAlbums(bool published_only = false) override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<Album> result;
        for (const auto& [id, album] : albums_) {
            if (!published_only || album.published) {
                result.push_back(album);
            }
        }

        // Sort by created_at descending (most recent first)
        std::sort(result.begin(), result.end(),
                 [](const Album& a, const Album& b) {
                     return a.created_at > b.created_at;
                 });

        return result;
    }

    bool deleteAlbum(const std::string& album_id) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = albums_.find(album_id);
        if (it != albums_.end()) {
            albums_.erase(it);
            return true;
        }
        return false;
    }

    bool albumNameExists(const std::string& name, const std::string& exclude_album_id = "") override {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& [id, album] : albums_) {
            if (album.name == name && id != exclude_album_id) {
                return true;
            }
        }
        return false;
    }

    // Image metadata operations
    bool putImageMetadata(const ImageMetadata& metadata) override {
        std::lock_guard<std::mutex> lock(mutex_);
        images_[metadata.image_id] = metadata;
        return true;
    }

    std::optional<ImageMetadata> getImageMetadata(const std::string& image_id) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = images_.find(image_id);
        if (it != images_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<ImageMetadata> listImages(int limit, int offset, ImageSortOrder sort_order) override {
        std::lock_guard<std::mutex> lock(mutex_);

        // Convert map to vector
        std::vector<ImageMetadata> all_images;
        for (const auto& [id, img] : images_) {
            all_images.push_back(img);
        }

        // Sort based on sort_order
        switch (sort_order) {
            case ImageSortOrder::NEWEST:
                std::sort(all_images.begin(), all_images.end(),
                         [](const ImageMetadata& a, const ImageMetadata& b) {
                             return a.upload_timestamp > b.upload_timestamp;
                         });
                break;
            case ImageSortOrder::OLDEST:
                std::sort(all_images.begin(), all_images.end(),
                         [](const ImageMetadata& a, const ImageMetadata& b) {
                             return a.upload_timestamp < b.upload_timestamp;
                         });
                break;
            case ImageSortOrder::NAME_ASC:
                std::sort(all_images.begin(), all_images.end(),
                         [](const ImageMetadata& a, const ImageMetadata& b) {
                             return a.name < b.name;
                         });
                break;
            case ImageSortOrder::NAME_DESC:
                std::sort(all_images.begin(), all_images.end(),
                         [](const ImageMetadata& a, const ImageMetadata& b) {
                             return a.name > b.name;
                         });
                break;
        }

        // Apply pagination
        std::vector<ImageMetadata> result;
        size_t start = std::min(static_cast<size_t>(offset), all_images.size());
        size_t end = std::min(start + static_cast<size_t>(limit), all_images.size());

        for (size_t i = start; i < end; ++i) {
            result.push_back(all_images[i]);
        }

        return result;
    }

    int getImageCount() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(images_.size());
    }

    bool imageExists(const std::string& image_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return images_.find(image_id) != images_.end();
    }

    // Test helper methods
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        albums_.clear();
        images_.clear();
    }

    size_t getAlbumCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return albums_.size();
    }

    size_t getImageMetadataCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return images_.size();
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, Album> albums_;
    std::map<std::string, ImageMetadata> images_;
};

} // namespace testing
} // namespace gara

#endif // GARA_TESTS_MOCKS_FAKE_DATABASE_CLIENT_H
