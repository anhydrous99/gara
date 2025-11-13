#include "album.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace gara {

// Album implementation
Album::Album()
    : album_id(""), name(""), description(""), cover_image_id(""),
      published(false), created_at(0), updated_at(0) {}

Album::Album(const std::string& id, const std::string& name)
    : album_id(id), name(name), description(""), cover_image_id(""),
      published(false), created_at(std::time(nullptr)), updated_at(std::time(nullptr)) {}

nlohmann::json Album::toJson() const {
    nlohmann::json j;
    j["album_id"] = album_id;
    j["name"] = name;
    j["description"] = description;
    j["cover_image_id"] = cover_image_id;
    j["image_ids"] = image_ids;
    j["tags"] = tags;
    j["published"] = published;
    j["created_at"] = static_cast<int64_t>(created_at);
    j["updated_at"] = static_cast<int64_t>(updated_at);
    return j;
}

Album Album::fromJson(const nlohmann::json& j) {
    Album album;

    if (j.contains("album_id") && j["album_id"].is_string()) {
        album.album_id = j["album_id"].get<std::string>();
    }

    if (j.contains("name") && j["name"].is_string()) {
        album.name = j["name"].get<std::string>();
    } else {
        throw std::runtime_error("Album name is required");
    }

    if (j.contains("description") && j["description"].is_string()) {
        album.description = j["description"].get<std::string>();
    }

    if (j.contains("cover_image_id") && j["cover_image_id"].is_string()) {
        album.cover_image_id = j["cover_image_id"].get<std::string>();
    }

    if (j.contains("image_ids") && j["image_ids"].is_array()) {
        album.image_ids = j["image_ids"].get<std::vector<std::string>>();
    }

    if (j.contains("tags") && j["tags"].is_array()) {
        album.tags = j["tags"].get<std::vector<std::string>>();
    }

    if (j.contains("published") && j["published"].is_boolean()) {
        album.published = j["published"].get<bool>();
    }

    if (j.contains("created_at")) {
        album.created_at = static_cast<std::time_t>(j["created_at"].get<int64_t>());
    }

    if (j.contains("updated_at")) {
        album.updated_at = static_cast<std::time_t>(j["updated_at"].get<int64_t>());
    }

    return album;
}

// CreateAlbumRequest implementation
CreateAlbumRequest::CreateAlbumRequest()
    : name(""), description(""), published(false) {}

CreateAlbumRequest CreateAlbumRequest::fromJson(const nlohmann::json& j) {
    CreateAlbumRequest request;

    if (j.contains("name") && j["name"].is_string()) {
        request.name = j["name"].get<std::string>();
    } else {
        throw std::runtime_error("Album name is required");
    }

    if (j.contains("description") && j["description"].is_string()) {
        request.description = j["description"].get<std::string>();
    }

    if (j.contains("tags") && j["tags"].is_array()) {
        request.tags = j["tags"].get<std::vector<std::string>>();
    }

    if (j.contains("published") && j["published"].is_boolean()) {
        request.published = j["published"].get<bool>();
    }

    return request;
}

// UpdateAlbumRequest implementation
UpdateAlbumRequest::UpdateAlbumRequest()
    : name(""), description(""), cover_image_id(""), published(false) {}

UpdateAlbumRequest UpdateAlbumRequest::fromJson(const nlohmann::json& j) {
    UpdateAlbumRequest request;

    if (j.contains("name") && j["name"].is_string()) {
        request.name = j["name"].get<std::string>();
    }

    if (j.contains("description") && j["description"].is_string()) {
        request.description = j["description"].get<std::string>();
    }

    if (j.contains("cover_image_id") && j["cover_image_id"].is_string()) {
        request.cover_image_id = j["cover_image_id"].get<std::string>();
    }

    if (j.contains("tags") && j["tags"].is_array()) {
        request.tags = j["tags"].get<std::vector<std::string>>();
    }

    if (j.contains("published") && j["published"].is_boolean()) {
        request.published = j["published"].get<bool>();
    }

    return request;
}

// AddImagesRequest implementation
AddImagesRequest::AddImagesRequest()
    : position(-1) {}

AddImagesRequest AddImagesRequest::fromJson(const nlohmann::json& j) {
    AddImagesRequest request;

    if (j.contains("image_ids") && j["image_ids"].is_array()) {
        request.image_ids = j["image_ids"].get<std::vector<std::string>>();
    } else {
        throw std::runtime_error("image_ids array is required");
    }

    if (j.contains("position") && j["position"].is_number()) {
        request.position = j["position"].get<int>();
    }

    return request;
}

// ReorderImagesRequest implementation
ReorderImagesRequest::ReorderImagesRequest() {}

ReorderImagesRequest ReorderImagesRequest::fromJson(const nlohmann::json& j) {
    ReorderImagesRequest request;

    if (j.contains("image_ids") && j["image_ids"].is_array()) {
        request.image_ids = j["image_ids"].get<std::vector<std::string>>();
    } else {
        throw std::runtime_error("image_ids array is required");
    }

    return request;
}

} // namespace gara
