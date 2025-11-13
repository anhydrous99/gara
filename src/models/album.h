#ifndef GARA_ALBUM_H
#define GARA_ALBUM_H

#include <string>
#include <vector>
#include <ctime>
#include <nlohmann/json.hpp>

namespace gara {

struct Album {
    std::string album_id;
    std::string name;
    std::string description;
    std::string cover_image_id;
    std::vector<std::string> image_ids;  // Ordered list of SHA256 hashes
    std::vector<std::string> tags;
    bool published;
    std::time_t created_at;
    std::time_t updated_at;

    // Constructor
    Album();
    Album(const std::string& id, const std::string& name);

    // Convert to/from JSON
    nlohmann::json toJson() const;
    static Album fromJson(const nlohmann::json& j);
};

struct CreateAlbumRequest {
    std::string name;
    std::string description;
    std::vector<std::string> tags;
    bool published;

    CreateAlbumRequest();

    static CreateAlbumRequest fromJson(const nlohmann::json& j);
};

struct UpdateAlbumRequest {
    std::string name;
    std::string description;
    std::string cover_image_id;
    std::vector<std::string> tags;
    bool published;

    UpdateAlbumRequest();

    static UpdateAlbumRequest fromJson(const nlohmann::json& j);
};

struct AddImagesRequest {
    std::vector<std::string> image_ids;
    int position;  // -1 for append, otherwise insert at index

    AddImagesRequest();

    static AddImagesRequest fromJson(const nlohmann::json& j);
};

struct ReorderImagesRequest {
    std::vector<std::string> image_ids;  // New ordered list

    ReorderImagesRequest();

    static ReorderImagesRequest fromJson(const nlohmann::json& j);
};

} // namespace gara

#endif // GARA_ALBUM_H
