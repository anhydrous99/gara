#ifndef GARA_INTERFACES_DATABASE_CLIENT_INTERFACE_H
#define GARA_INTERFACES_DATABASE_CLIENT_INTERFACE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include "../models/album.h"

namespace gara {

/**
 * @brief Database-agnostic interface for album storage operations
 *
 * This interface abstracts away the underlying database implementation,
 * allowing for easy switching between DynamoDB, SQLite, etc.
 */
class DatabaseClientInterface {
public:
    virtual ~DatabaseClientInterface() = default;

    /**
     * @brief Store or update an album
     * @param album The album to store
     * @return true if successful, false otherwise
     */
    virtual bool putAlbum(const Album& album) = 0;

    /**
     * @brief Retrieve an album by ID
     * @param album_id The album ID to retrieve
     * @return Optional album if found, nullopt otherwise
     */
    virtual std::optional<Album> getAlbum(const std::string& album_id) = 0;

    /**
     * @brief List all albums or filter by published status
     * @param published_only If true, only return published albums
     * @return Vector of albums
     */
    virtual std::vector<Album> listAlbums(bool published_only = false) = 0;

    /**
     * @brief Delete an album by ID
     * @param album_id The album ID to delete
     * @return true if deleted, false if not found
     */
    virtual bool deleteAlbum(const std::string& album_id) = 0;

    /**
     * @brief Check if an album name exists (for uniqueness validation)
     * @param name The album name to check
     * @param exclude_album_id Optional album ID to exclude from the check
     * @return true if name exists, false otherwise
     */
    virtual bool albumNameExists(const std::string& name,
                                 const std::string& exclude_album_id = "") = 0;
};

} // namespace gara

#endif // GARA_INTERFACES_DATABASE_CLIENT_INTERFACE_H
