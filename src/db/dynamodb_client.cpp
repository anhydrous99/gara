#include "dynamodb_client.h"
#include "../utils/logger.h"
#include <nlohmann/json.hpp>
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/GetItemRequest.h>
#include <aws/dynamodb/model/DeleteItemRequest.h>
#include <aws/dynamodb/model/ScanRequest.h>
#include <aws/dynamodb/model/DescribeTableRequest.h>
#include <aws/dynamodb/model/CreateTableRequest.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/dynamodb/model/KeySchemaElement.h>
#include <algorithm>
#include <cstdlib>

using json = nlohmann::json;
using namespace Aws::DynamoDB::Model;

namespace gara {

// Static members for AWS SDK lifecycle management
std::atomic<int> DynamoDBClient::sdk_init_count_{0};
std::mutex DynamoDBClient::sdk_init_mutex_;

// DynamoDBConfig implementation

DynamoDBConfig DynamoDBConfig::fromEnvironment() {
    DynamoDBConfig config;

    if (const char* region = std::getenv("AWS_REGION")) {
        config.region = region;
    } else if (const char* region = std::getenv("AWS_DEFAULT_REGION")) {
        config.region = region;
    }
    if (const char* endpoint = std::getenv("DYNAMODB_ENDPOINT_URL")) {
        config.endpoint_url = endpoint;
    }
    if (const char* albums_table = std::getenv("DYNAMODB_ALBUMS_TABLE")) {
        config.albums_table = albums_table;
    }
    if (const char* images_table = std::getenv("DYNAMODB_IMAGES_TABLE")) {
        config.images_table = images_table;
    }

    return config;
}

// DynamoDBClient implementation

DynamoDBClient::DynamoDBClient(const DynamoDBConfig& config)
    : config_(config) {

    // Initialize AWS SDK (reference counted)
    {
        std::lock_guard<std::mutex> lock(sdk_init_mutex_);
        if (sdk_init_count_++ == 0) {
            Aws::SDKOptions options;
            Aws::InitAPI(options);
            LOG_INFO("AWS SDK initialized");
        }
    }

    // Configure client
    Aws::Client::ClientConfiguration client_config;
    client_config.region = config_.region;

    if (!config_.endpoint_url.empty()) {
        client_config.endpointOverride = config_.endpoint_url;
        LOG_INFO("Using custom DynamoDB endpoint: {}", config_.endpoint_url);
    }

    client_ = std::make_unique<Aws::DynamoDB::DynamoDBClient>(client_config);

    LOG_INFO("DynamoDB client created for region: {}", config_.region);
}

DynamoDBClient::~DynamoDBClient() {
    client_.reset();

    // Shutdown AWS SDK (reference counted)
    {
        std::lock_guard<std::mutex> lock(sdk_init_mutex_);
        if (--sdk_init_count_ == 0) {
            Aws::SDKOptions options;
            Aws::ShutdownAPI(options);
            LOG_INFO("AWS SDK shutdown");
        }
    }
}

bool DynamoDBClient::isConnected() const {
    return client_ != nullptr && initialized_;
}

bool DynamoDBClient::initialize() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    if (!client_) {
        LOG_ERROR("DynamoDB client not created");
        return false;
    }

    // Create tables if they don't exist
    if (!createTablesIfNotExist()) {
        LOG_ERROR("Failed to create/verify DynamoDB tables");
        return false;
    }

    initialized_ = true;
    LOG_INFO("DynamoDB client initialized successfully");
    return true;
}

bool DynamoDBClient::tableExists(const std::string& table_name) {
    DescribeTableRequest request;
    request.SetTableName(table_name);

    auto outcome = client_->DescribeTable(request);
    return outcome.IsSuccess();
}

bool DynamoDBClient::createTablesIfNotExist() {
    // Check and create albums table
    if (!tableExists(config_.albums_table)) {
        LOG_INFO("Creating albums table: {}", config_.albums_table);

        CreateTableRequest request;
        request.SetTableName(config_.albums_table);

        // Key schema
        KeySchemaElement key_schema;
        key_schema.SetAttributeName("album_id");
        key_schema.SetKeyType(KeyType::HASH);
        request.AddKeySchema(key_schema);

        // Attribute definitions
        AttributeDefinition attr_def;
        attr_def.SetAttributeName("album_id");
        attr_def.SetAttributeType(ScalarAttributeType::S);
        request.AddAttributeDefinitions(attr_def);

        // On-demand billing
        request.SetBillingMode(BillingMode::PAY_PER_REQUEST);

        auto outcome = client_->CreateTable(request);
        if (!outcome.IsSuccess()) {
            LOG_ERROR("Failed to create albums table: {}",
                     outcome.GetError().GetMessage());
            return false;
        }

        // Wait for table to become active
        LOG_INFO("Waiting for albums table to become active...");
        int retries = 30;
        while (retries-- > 0) {
            DescribeTableRequest desc_request;
            desc_request.SetTableName(config_.albums_table);
            auto desc_outcome = client_->DescribeTable(desc_request);
            if (desc_outcome.IsSuccess() &&
                desc_outcome.GetResult().GetTable().GetTableStatus() == TableStatus::ACTIVE) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    // Check and create images table
    if (!tableExists(config_.images_table)) {
        LOG_INFO("Creating images table: {}", config_.images_table);

        CreateTableRequest request;
        request.SetTableName(config_.images_table);

        // Key schema
        KeySchemaElement key_schema;
        key_schema.SetAttributeName("image_id");
        key_schema.SetKeyType(KeyType::HASH);
        request.AddKeySchema(key_schema);

        // Attribute definitions
        AttributeDefinition attr_def;
        attr_def.SetAttributeName("image_id");
        attr_def.SetAttributeType(ScalarAttributeType::S);
        request.AddAttributeDefinitions(attr_def);

        // On-demand billing
        request.SetBillingMode(BillingMode::PAY_PER_REQUEST);

        auto outcome = client_->CreateTable(request);
        if (!outcome.IsSuccess()) {
            LOG_ERROR("Failed to create images table: {}",
                     outcome.GetError().GetMessage());
            return false;
        }

        // Wait for table to become active
        LOG_INFO("Waiting for images table to become active...");
        int retries = 30;
        while (retries-- > 0) {
            DescribeTableRequest desc_request;
            desc_request.SetTableName(config_.images_table);
            auto desc_outcome = client_->DescribeTable(desc_request);
            if (desc_outcome.IsSuccess() &&
                desc_outcome.GetResult().GetTable().GetTableStatus() == TableStatus::ACTIVE) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    LOG_INFO("DynamoDB tables verified: {}, {}", config_.albums_table, config_.images_table);
    return true;
}

// Helper methods

std::string DynamoDBClient::vectorToJson(const std::vector<std::string>& vec) {
    return json(vec).dump();
}

std::vector<std::string> DynamoDBClient::jsonToVector(const std::string& json_str) {
    if (json_str.empty()) {
        return {};
    }
    try {
        return json::parse(json_str).get<std::vector<std::string>>();
    } catch (const json::exception& e) {
        LOG_ERROR("Failed to parse JSON: {}", e.what());
        return {};
    }
}

std::string DynamoDBClient::getStringAttribute(
    const Aws::Map<Aws::String, AttributeValue>& item,
    const std::string& key,
    const std::string& default_value) {

    auto it = item.find(key);
    if (it != item.end() && it->second.GetS().length() > 0) {
        return std::string(it->second.GetS().c_str());
    }
    return default_value;
}

long long DynamoDBClient::getNumberAttribute(
    const Aws::Map<Aws::String, AttributeValue>& item,
    const std::string& key,
    long long default_value) {

    auto it = item.find(key);
    if (it != item.end() && it->second.GetN().length() > 0) {
        try {
            return std::stoll(std::string(it->second.GetN().c_str()));
        } catch (...) {
            return default_value;
        }
    }
    return default_value;
}

bool DynamoDBClient::getBoolAttribute(
    const Aws::Map<Aws::String, AttributeValue>& item,
    const std::string& key,
    bool default_value) {

    auto it = item.find(key);
    if (it != item.end()) {
        return it->second.GetBool();
    }
    return default_value;
}

Album DynamoDBClient::extractAlbum(const Aws::Map<Aws::String, AttributeValue>& item) {
    Album album;

    album.album_id = getStringAttribute(item, "album_id");
    album.name = getStringAttribute(item, "name");
    album.description = getStringAttribute(item, "description");
    album.cover_image_id = getStringAttribute(item, "cover_image_id");

    std::string image_ids_json = getStringAttribute(item, "image_ids", "[]");
    album.image_ids = jsonToVector(image_ids_json);

    std::string tags_json = getStringAttribute(item, "tags", "[]");
    album.tags = jsonToVector(tags_json);

    album.published = getBoolAttribute(item, "published", false);
    album.created_at = static_cast<std::time_t>(getNumberAttribute(item, "created_at", 0));
    album.updated_at = static_cast<std::time_t>(getNumberAttribute(item, "updated_at", 0));

    return album;
}

ImageMetadata DynamoDBClient::extractImageMetadata(const Aws::Map<Aws::String, AttributeValue>& item) {
    ImageMetadata metadata;

    metadata.image_id = getStringAttribute(item, "image_id");
    metadata.name = getStringAttribute(item, "name");
    metadata.original_format = getStringAttribute(item, "original_format");
    metadata.original_size = static_cast<size_t>(getNumberAttribute(item, "size", 0));
    metadata.width = static_cast<int>(getNumberAttribute(item, "width", 0));
    metadata.height = static_cast<int>(getNumberAttribute(item, "height", 0));
    metadata.upload_timestamp = static_cast<std::time_t>(getNumberAttribute(item, "uploaded_at", 0));

    return metadata;
}

// Album operations

bool DynamoDBClient::putAlbum(const Album& album) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    PutItemRequest request;
    request.SetTableName(config_.albums_table);

    Aws::Map<Aws::String, AttributeValue> item;

    AttributeValue album_id_val;
    album_id_val.SetS(album.album_id);
    item["album_id"] = album_id_val;

    AttributeValue name_val;
    name_val.SetS(album.name);
    item["name"] = name_val;

    AttributeValue description_val;
    description_val.SetS(album.description);
    item["description"] = description_val;

    AttributeValue cover_image_id_val;
    cover_image_id_val.SetS(album.cover_image_id);
    item["cover_image_id"] = cover_image_id_val;

    AttributeValue image_ids_val;
    image_ids_val.SetS(vectorToJson(album.image_ids));
    item["image_ids"] = image_ids_val;

    AttributeValue tags_val;
    tags_val.SetS(vectorToJson(album.tags));
    item["tags"] = tags_val;

    AttributeValue published_val;
    published_val.SetBool(album.published);
    item["published"] = published_val;

    AttributeValue created_at_val;
    created_at_val.SetN(std::to_string(static_cast<long long>(album.created_at)));
    item["created_at"] = created_at_val;

    AttributeValue updated_at_val;
    updated_at_val.SetN(std::to_string(static_cast<long long>(album.updated_at)));
    item["updated_at"] = updated_at_val;

    request.SetItem(item);

    auto outcome = client_->PutItem(request);
    if (!outcome.IsSuccess()) {
        LOG_ERROR("Failed to put album {}: {}", album.album_id,
                 outcome.GetError().GetMessage());
        return false;
    }

    LOG_DEBUG("Album stored successfully: {}", album.album_id);
    return true;
}

std::optional<Album> DynamoDBClient::getAlbum(const std::string& album_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    GetItemRequest request;
    request.SetTableName(config_.albums_table);

    AttributeValue key_val;
    key_val.SetS(album_id);
    request.AddKey("album_id", key_val);

    auto outcome = client_->GetItem(request);
    if (!outcome.IsSuccess()) {
        LOG_ERROR("Failed to get album {}: {}", album_id,
                 outcome.GetError().GetMessage());
        return std::nullopt;
    }

    const auto& item = outcome.GetResult().GetItem();
    if (item.empty()) {
        return std::nullopt;
    }

    return extractAlbum(item);
}

std::vector<Album> DynamoDBClient::listAlbums(bool published_only) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    std::vector<Album> albums;
    ScanRequest request;
    request.SetTableName(config_.albums_table);

    if (published_only) {
        request.SetFilterExpression("published = :pub");
        AttributeValue pub_val;
        pub_val.SetBool(true);
        request.AddExpressionAttributeValues(":pub", pub_val);
    }

    // Handle pagination
    Aws::Map<Aws::String, AttributeValue> last_key;
    do {
        if (!last_key.empty()) {
            request.SetExclusiveStartKey(last_key);
        }

        auto outcome = client_->Scan(request);
        if (!outcome.IsSuccess()) {
            LOG_ERROR("Failed to scan albums: {}", outcome.GetError().GetMessage());
            return albums;
        }

        for (const auto& item : outcome.GetResult().GetItems()) {
            albums.push_back(extractAlbum(item));
        }

        last_key = outcome.GetResult().GetLastEvaluatedKey();
    } while (!last_key.empty());

    // Sort by created_at descending (DynamoDB Scan doesn't sort)
    std::sort(albums.begin(), albums.end(),
              [](const Album& a, const Album& b) {
                  return a.created_at > b.created_at;
              });

    LOG_DEBUG("Listed {} albums", albums.size());
    return albums;
}

bool DynamoDBClient::deleteAlbum(const std::string& album_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    DeleteItemRequest request;
    request.SetTableName(config_.albums_table);

    AttributeValue key_val;
    key_val.SetS(album_id);
    request.AddKey("album_id", key_val);

    // Return old item to verify it existed
    request.SetReturnValues(ReturnValue::ALL_OLD);

    auto outcome = client_->DeleteItem(request);
    if (!outcome.IsSuccess()) {
        LOG_ERROR("Failed to delete album {}: {}", album_id,
                 outcome.GetError().GetMessage());
        return false;
    }

    bool existed = !outcome.GetResult().GetAttributes().empty();
    LOG_DEBUG("Album deleted: {} (existed: {})", album_id, existed);
    return existed;
}

bool DynamoDBClient::albumNameExists(const std::string& name, const std::string& exclude_album_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    ScanRequest request;
    request.SetTableName(config_.albums_table);
    request.SetProjectionExpression("album_id");

    if (exclude_album_id.empty()) {
        request.SetFilterExpression("#n = :name");
    } else {
        request.SetFilterExpression("#n = :name AND album_id <> :exclude_id");
        AttributeValue exclude_val;
        exclude_val.SetS(exclude_album_id);
        request.AddExpressionAttributeValues(":exclude_id", exclude_val);
    }

    // 'name' is a reserved word in DynamoDB
    request.AddExpressionAttributeNames("#n", "name");

    AttributeValue name_val;
    name_val.SetS(name);
    request.AddExpressionAttributeValues(":name", name_val);

    auto outcome = client_->Scan(request);
    if (!outcome.IsSuccess()) {
        LOG_ERROR("Failed to check album name existence: {}",
                 outcome.GetError().GetMessage());
        return false;
    }

    return outcome.GetResult().GetCount() > 0;
}

// Image metadata operations

bool DynamoDBClient::putImageMetadata(const ImageMetadata& metadata) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    PutItemRequest request;
    request.SetTableName(config_.images_table);

    Aws::Map<Aws::String, AttributeValue> item;

    AttributeValue image_id_val;
    image_id_val.SetS(metadata.image_id);
    item["image_id"] = image_id_val;

    AttributeValue name_val;
    name_val.SetS(metadata.name);
    item["name"] = name_val;

    AttributeValue format_val;
    format_val.SetS(metadata.original_format);
    item["original_format"] = format_val;

    AttributeValue size_val;
    size_val.SetN(std::to_string(static_cast<long long>(metadata.original_size)));
    item["size"] = size_val;

    AttributeValue width_val;
    width_val.SetN(std::to_string(metadata.width));
    item["width"] = width_val;

    AttributeValue height_val;
    height_val.SetN(std::to_string(metadata.height));
    item["height"] = height_val;

    AttributeValue uploaded_at_val;
    uploaded_at_val.SetN(std::to_string(static_cast<long long>(metadata.upload_timestamp)));
    item["uploaded_at"] = uploaded_at_val;

    request.SetItem(item);

    auto outcome = client_->PutItem(request);
    if (!outcome.IsSuccess()) {
        LOG_ERROR("Failed to put image metadata {}: {}", metadata.image_id,
                 outcome.GetError().GetMessage());
        return false;
    }

    LOG_DEBUG("Image metadata stored successfully: {}", metadata.image_id);
    return true;
}

std::optional<ImageMetadata> DynamoDBClient::getImageMetadata(const std::string& image_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    GetItemRequest request;
    request.SetTableName(config_.images_table);

    AttributeValue key_val;
    key_val.SetS(image_id);
    request.AddKey("image_id", key_val);

    auto outcome = client_->GetItem(request);
    if (!outcome.IsSuccess()) {
        LOG_ERROR("Failed to get image metadata {}: {}", image_id,
                 outcome.GetError().GetMessage());
        return std::nullopt;
    }

    const auto& item = outcome.GetResult().GetItem();
    if (item.empty()) {
        return std::nullopt;
    }

    return extractImageMetadata(item);
}

std::vector<ImageMetadata> DynamoDBClient::listImages(int limit, int offset, ImageSortOrder sort_order) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    // DynamoDB Scan doesn't support sorting, so we need to fetch all and sort in memory
    std::vector<ImageMetadata> all_images;
    ScanRequest request;
    request.SetTableName(config_.images_table);

    // Handle pagination
    Aws::Map<Aws::String, AttributeValue> last_key;
    do {
        if (!last_key.empty()) {
            request.SetExclusiveStartKey(last_key);
        }

        auto outcome = client_->Scan(request);
        if (!outcome.IsSuccess()) {
            LOG_ERROR("Failed to scan images: {}", outcome.GetError().GetMessage());
            return {};
        }

        for (const auto& item : outcome.GetResult().GetItems()) {
            all_images.push_back(extractImageMetadata(item));
        }

        last_key = outcome.GetResult().GetLastEvaluatedKey();
    } while (!last_key.empty());

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

    // Apply offset and limit
    std::vector<ImageMetadata> result;
    int start = std::min(offset, static_cast<int>(all_images.size()));
    int end = std::min(start + limit, static_cast<int>(all_images.size()));

    for (int i = start; i < end; ++i) {
        result.push_back(all_images[i]);
    }

    LOG_DEBUG("Listed {} images (offset: {}, limit: {})", result.size(), offset, limit);
    return result;
}

int DynamoDBClient::getImageCount() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    ScanRequest request;
    request.SetTableName(config_.images_table);
    request.SetSelect(Select::COUNT);

    int count = 0;
    Aws::Map<Aws::String, AttributeValue> last_key;

    do {
        if (!last_key.empty()) {
            request.SetExclusiveStartKey(last_key);
        }

        auto outcome = client_->Scan(request);
        if (!outcome.IsSuccess()) {
            LOG_ERROR("Failed to count images: {}", outcome.GetError().GetMessage());
            return 0;
        }

        count += outcome.GetResult().GetCount();
        last_key = outcome.GetResult().GetLastEvaluatedKey();
    } while (!last_key.empty());

    return count;
}

bool DynamoDBClient::imageExists(const std::string& image_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    GetItemRequest request;
    request.SetTableName(config_.images_table);
    request.SetProjectionExpression("image_id");

    AttributeValue key_val;
    key_val.SetS(image_id);
    request.AddKey("image_id", key_val);

    auto outcome = client_->GetItem(request);
    if (!outcome.IsSuccess()) {
        LOG_ERROR("Failed to check image existence {}: {}", image_id,
                 outcome.GetError().GetMessage());
        return false;
    }

    return !outcome.GetResult().GetItem().empty();
}

} // namespace gara
