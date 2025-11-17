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

    // Test helper methods
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        albums_.clear();
    }

    size_t getAlbumCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return albums_.size();
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, Album> albums_;
};

} // namespace testing
} // namespace gara

#endif // GARA_TESTS_MOCKS_FAKE_DATABASE_CLIENT_H
