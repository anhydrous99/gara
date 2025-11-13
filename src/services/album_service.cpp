#include "album_service.h"
#include "s3_service.h"
#include "../constants/album_constants.h"
#include "../exceptions/album_exceptions.h"
#include "../utils/id_generator.h"
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/GetItemRequest.h>
#include <aws/dynamodb/model/UpdateItemRequest.h>
#include <aws/dynamodb/model/DeleteItemRequest.h>
#include <aws/dynamodb/model/ScanRequest.h>
#include <aws/dynamodb/model/QueryRequest.h>
#include <aws/core/Aws.h>
#include <stdexcept>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace gara {

AlbumService::AlbumService(const std::string& table_name,
                           std::shared_ptr<DynamoDBClientInterface> dynamodb_client,
                           std::shared_ptr<S3Service> s3_service)
    : table_name_(table_name), dynamodb_client_(dynamodb_client), s3_service_(s3_service) {
}

bool AlbumService::validateImageExists(const std::string& image_id) {
    if (!s3_service_) {
        // If no S3 service provided (e.g., in tests), skip validation
        return true;
    }

    // Try common image formats
    for (const auto& format : constants::SUPPORTED_IMAGE_FORMATS) {
        std::string key = "raw/" + image_id + "." + format;
        if (s3_service_->objectExists(key)) {
            return true;
        }
    }
    return false;
}

bool AlbumService::albumNameExists(const std::string& name, const std::string& exclude_album_id) {
    Aws::DynamoDB::Model::ScanRequest scan_request;
    scan_request.SetTableName(table_name_.c_str());
    scan_request.SetFilterExpression("#name = :name");
    scan_request.AddExpressionAttributeNames("#name", "Name");
    scan_request.AddExpressionAttributeValues(":name",
        Aws::DynamoDB::Model::AttributeValue(name.c_str()));

    auto outcome = dynamodb_client_->Scan(scan_request);
    if (!outcome.IsSuccess()) {
        return false;
    }

    for (const auto& item : outcome.GetResult().GetItems()) {
        auto album_id_it = item.find("AlbumId");
        if (album_id_it != item.end()) {
            std::string existing_id = album_id_it->second.GetS();
            if (existing_id != exclude_album_id) {
                return true;
            }
        }
    }

    return false;
}

Album AlbumService::itemToAlbum(const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item) {
    Album album;

    auto it = item.find("AlbumId");
    if (it != item.end()) album.album_id = it->second.GetS();

    it = item.find("Name");
    if (it != item.end()) album.name = it->second.GetS();

    it = item.find("Description");
    if (it != item.end()) album.description = it->second.GetS();

    it = item.find("CoverImageId");
    if (it != item.end()) album.cover_image_id = it->second.GetS();

    it = item.find("ImageIds");
    if (it != item.end()) {
        const auto& list_items = it->second.GetL();
        for (const auto& val : list_items) {
            album.image_ids.push_back(val->GetS());
        }
    }

    it = item.find("Tags");
    if (it != item.end()) {
        const auto& list_items = it->second.GetL();
        for (const auto& val : list_items) {
            album.tags.push_back(val->GetS());
        }
    }

    it = item.find("Published");
    if (it != item.end()) {
        album.published = (it->second.GetS() == "true" || it->second.GetBool());
    }

    it = item.find("CreatedAt");
    if (it != item.end()) {
        album.created_at = static_cast<std::time_t>(std::stoll(it->second.GetN()));
    }

    it = item.find("UpdatedAt");
    if (it != item.end()) {
        album.updated_at = static_cast<std::time_t>(std::stoll(it->second.GetN()));
    }

    return album;
}

Aws::DynamoDB::Model::PutItemRequest AlbumService::albumToPutItemRequest(const Album& album) {
    Aws::DynamoDB::Model::PutItemRequest request;
    request.SetTableName(table_name_.c_str());

    request.AddItem("AlbumId", Aws::DynamoDB::Model::AttributeValue(album.album_id.c_str()));
    request.AddItem("Name", Aws::DynamoDB::Model::AttributeValue(album.name.c_str()));
    request.AddItem("Description", Aws::DynamoDB::Model::AttributeValue(album.description.c_str()));
    request.AddItem("CoverImageId", Aws::DynamoDB::Model::AttributeValue(album.cover_image_id.c_str()));
    request.AddItem("Published", Aws::DynamoDB::Model::AttributeValue(album.published ? "true" : "false"));
    request.AddItem("CreatedAt", Aws::DynamoDB::Model::AttributeValue().SetN(std::to_string(album.created_at)));
    request.AddItem("UpdatedAt", Aws::DynamoDB::Model::AttributeValue().SetN(std::to_string(album.updated_at)));

    // Add ImageIds list
    Aws::DynamoDB::Model::AttributeValue image_ids_attr;
    for (const auto& id : album.image_ids) {
        auto item = std::make_shared<Aws::DynamoDB::Model::AttributeValue>(id.c_str());
        image_ids_attr.AddLItem(item);
    }
    request.AddItem("ImageIds", image_ids_attr);

    // Add Tags list
    Aws::DynamoDB::Model::AttributeValue tags_attr;
    for (const auto& tag : album.tags) {
        auto item = std::make_shared<Aws::DynamoDB::Model::AttributeValue>(tag.c_str());
        tags_attr.AddLItem(item);
    }
    request.AddItem("Tags", tags_attr);

    return request;
}

Album AlbumService::createAlbum(const CreateAlbumRequest& request) {
    // Validate name is provided
    if (request.name.empty()) {
        throw exceptions::ValidationException("Album name is required");
    }

    // Check if album with same name exists
    if (albumNameExists(request.name)) {
        throw exceptions::ConflictException("Album with this name already exists");
    }

    // Create new album
    Album album;
    album.album_id = utils::IdGenerator::generateAlbumId();
    album.name = request.name;
    album.description = request.description;
    album.tags = request.tags;
    album.published = request.published;
    album.created_at = std::time(nullptr);
    album.updated_at = album.created_at;

    // Save to DynamoDB
    auto put_request = albumToPutItemRequest(album);
    auto outcome = dynamodb_client_->PutItem(put_request);

    if (!outcome.IsSuccess()) {
        throw std::runtime_error("Failed to create album: " +
            outcome.GetError().GetMessage());
    }

    return album;
}

Album AlbumService::getAlbum(const std::string& album_id) {
    Aws::DynamoDB::Model::GetItemRequest request;
    request.SetTableName(table_name_.c_str());
    request.AddKey("AlbumId", Aws::DynamoDB::Model::AttributeValue(album_id.c_str()));

    auto outcome = dynamodb_client_->GetItem(request);

    if (!outcome.IsSuccess()) {
        throw std::runtime_error("Failed to get album: " +
            outcome.GetError().GetMessage());
    }

    const auto& item = outcome.GetResult().GetItem();
    if (item.empty()) {
        throw exceptions::NotFoundException("Album not found");
    }

    return itemToAlbum(item);
}

std::vector<Album> AlbumService::listAlbums(bool published_only) {
    std::vector<Album> albums;

    if (published_only) {
        // Use Query with GSI if available, otherwise use Scan with filter
        Aws::DynamoDB::Model::ScanRequest scan_request;
        scan_request.SetTableName(table_name_.c_str());
        scan_request.SetFilterExpression("Published = :published");
        scan_request.AddExpressionAttributeValues(":published",
            Aws::DynamoDB::Model::AttributeValue("true"));

        auto outcome = dynamodb_client_->Scan(scan_request);
        if (!outcome.IsSuccess()) {
            throw std::runtime_error("Failed to list albums: " +
                outcome.GetError().GetMessage());
        }

        for (const auto& item : outcome.GetResult().GetItems()) {
            albums.push_back(itemToAlbum(item));
        }
    } else {
        // Scan all albums
        Aws::DynamoDB::Model::ScanRequest scan_request;
        scan_request.SetTableName(table_name_.c_str());

        auto outcome = dynamodb_client_->Scan(scan_request);
        if (!outcome.IsSuccess()) {
            throw std::runtime_error("Failed to list albums: " +
                outcome.GetError().GetMessage());
        }

        for (const auto& item : outcome.GetResult().GetItems()) {
            albums.push_back(itemToAlbum(item));
        }
    }

    // Sort by created_at descending (newest first)
    std::sort(albums.begin(), albums.end(),
        [](const Album& a, const Album& b) {
            return a.created_at > b.created_at;
        });

    return albums;
}

Album AlbumService::updateAlbum(const std::string& album_id, const UpdateAlbumRequest& request) {
    // Get existing album
    Album album = getAlbum(album_id);

    // Check if name is being changed and if new name exists
    if (!request.name.empty() && request.name != album.name) {
        if (albumNameExists(request.name, album_id)) {
            throw exceptions::ConflictException("Album with this name already exists");
        }
        album.name = request.name;
    }

    // Update fields if provided
    if (!request.description.empty()) {
        album.description = request.description;
    }

    if (!request.cover_image_id.empty()) {
        // Validate cover image exists
        if (!validateImageExists(request.cover_image_id)) {
            throw exceptions::ValidationException("Cover image " + request.cover_image_id + " not found in S3");
        }
        album.cover_image_id = request.cover_image_id;
    }

    if (!request.tags.empty()) {
        album.tags = request.tags;
    }

    album.published = request.published;
    album.updated_at = std::time(nullptr);

    // Save to DynamoDB
    auto put_request = albumToPutItemRequest(album);
    auto outcome = dynamodb_client_->PutItem(put_request);

    if (!outcome.IsSuccess()) {
        throw std::runtime_error("Failed to update album: " +
            outcome.GetError().GetMessage());
    }

    return album;
}

bool AlbumService::deleteAlbum(const std::string& album_id) {
    Aws::DynamoDB::Model::DeleteItemRequest request;
    request.SetTableName(table_name_.c_str());
    request.AddKey("AlbumId", Aws::DynamoDB::Model::AttributeValue(album_id.c_str()));

    auto outcome = dynamodb_client_->DeleteItem(request);

    if (!outcome.IsSuccess()) {
        throw std::runtime_error("Failed to delete album: " +
            outcome.GetError().GetMessage());
    }

    return true;
}

Album AlbumService::addImages(const std::string& album_id, const AddImagesRequest& request) {
    // Get existing album
    Album album = getAlbum(album_id);

    // Validate all images exist
    for (const auto& image_id : request.image_ids) {
        if (!validateImageExists(image_id)) {
            throw exceptions::ValidationException("Image " + image_id + " not found in S3");
        }
    }

    // Add images at specified position
    if (request.position < 0 || request.position >= static_cast<int>(album.image_ids.size())) {
        // Append to end
        album.image_ids.insert(album.image_ids.end(),
            request.image_ids.begin(), request.image_ids.end());
    } else {
        // Insert at position
        album.image_ids.insert(album.image_ids.begin() + request.position,
            request.image_ids.begin(), request.image_ids.end());
    }

    album.updated_at = std::time(nullptr);

    // Save to DynamoDB
    auto put_request = albumToPutItemRequest(album);
    auto outcome = dynamodb_client_->PutItem(put_request);

    if (!outcome.IsSuccess()) {
        throw std::runtime_error("Failed to add images to album: " +
            outcome.GetError().GetMessage());
    }

    return album;
}

Album AlbumService::removeImage(const std::string& album_id, const std::string& image_id) {
    // Get existing album
    Album album = getAlbum(album_id);

    // Remove image from list
    auto it = std::find(album.image_ids.begin(), album.image_ids.end(), image_id);
    if (it != album.image_ids.end()) {
        album.image_ids.erase(it);
    }

    // If this was the cover image, clear it
    if (album.cover_image_id == image_id) {
        album.cover_image_id = "";
    }

    album.updated_at = std::time(nullptr);

    // Save to DynamoDB
    auto put_request = albumToPutItemRequest(album);
    auto outcome = dynamodb_client_->PutItem(put_request);

    if (!outcome.IsSuccess()) {
        throw std::runtime_error("Failed to remove image from album: " +
            outcome.GetError().GetMessage());
    }

    return album;
}

Album AlbumService::reorderImages(const std::string& album_id, const ReorderImagesRequest& request) {
    // Get existing album
    Album album = getAlbum(album_id);

    // Validate that all image IDs in request match existing ones
    if (request.image_ids.size() != album.image_ids.size()) {
        throw exceptions::ValidationException("Reorder request must include all existing images");
    }

    for (const auto& image_id : request.image_ids) {
        if (std::find(album.image_ids.begin(), album.image_ids.end(), image_id) == album.image_ids.end()) {
            throw exceptions::ValidationException("Image " + image_id + " is not in this album");
        }
    }

    // Update order
    album.image_ids = request.image_ids;
    album.updated_at = std::time(nullptr);

    // Save to DynamoDB
    auto put_request = albumToPutItemRequest(album);
    auto outcome = dynamodb_client_->PutItem(put_request);

    if (!outcome.IsSuccess()) {
        throw std::runtime_error("Failed to reorder images: " +
            outcome.GetError().GetMessage());
    }

    return album;
}

} // namespace gara
