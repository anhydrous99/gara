#ifndef GARA_TESTS_MOCKS_FAKE_DYNAMODB_CLIENT_H
#define GARA_TESTS_MOCKS_FAKE_DYNAMODB_CLIENT_H

#include "../../src/interfaces/dynamodb_client_interface.h"
#include <map>
#include <string>
#include <mutex>

namespace gara {

/**
 * @brief In-memory fake DynamoDB client for testing
 *
 * This implementation stores items in memory using a map structure:
 * table_name -> primary_key_value -> item_attributes
 */
class FakeDynamoDBClient : public DynamoDBClientInterface {
public:
    FakeDynamoDBClient() = default;

    Aws::DynamoDB::Model::PutItemOutcome PutItem(
        const Aws::DynamoDB::Model::PutItemRequest& request) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto& table_name = request.GetTableName();
        const auto& item = request.GetItem();

        // Extract primary key (AlbumId)
        auto album_id_it = item.find("AlbumId");
        if (album_id_it == item.end()) {
            Aws::DynamoDB::DynamoDBError error;
            return Aws::DynamoDB::Model::PutItemOutcome(error);
        }

        std::string album_id = album_id_it->second.GetS();

        // Store the item
        tables_[table_name][album_id] = item;

        Aws::DynamoDB::Model::PutItemResult result;
        return Aws::DynamoDB::Model::PutItemOutcome(result);
    }

    Aws::DynamoDB::Model::GetItemOutcome GetItem(
        const Aws::DynamoDB::Model::GetItemRequest& request) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto& table_name = request.GetTableName();
        const auto& key = request.GetKey();

        // Extract primary key
        auto album_id_it = key.find("AlbumId");
        if (album_id_it == key.end()) {
            Aws::DynamoDB::DynamoDBError error;
            return Aws::DynamoDB::Model::GetItemOutcome(error);
        }

        std::string album_id = album_id_it->second.GetS();

        Aws::DynamoDB::Model::GetItemResult result;

        // Check if table exists
        auto table_it = tables_.find(table_name);
        if (table_it != tables_.end()) {
            // Check if item exists
            auto item_it = table_it->second.find(album_id);
            if (item_it != table_it->second.end()) {
                result.SetItem(item_it->second);
            }
        }

        return Aws::DynamoDB::Model::GetItemOutcome(result);
    }

    Aws::DynamoDB::Model::ScanOutcome Scan(
        const Aws::DynamoDB::Model::ScanRequest& request) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto& table_name = request.GetTableName();
        Aws::DynamoDB::Model::ScanResult result;

        auto table_it = tables_.find(table_name);
        if (table_it != tables_.end()) {
            // Simple scan - return all items (no filter support for now)
            for (const auto& [key, item] : table_it->second) {
                // Apply filter expression if present
                if (request.FilterExpressionHasBeenSet()) {
                    if (matchesFilter(item, request)) {
                        result.AddItems(item);
                    }
                } else {
                    result.AddItems(item);
                }
            }
        }

        result.SetCount(result.GetItems().size());
        return Aws::DynamoDB::Model::ScanOutcome(result);
    }

    Aws::DynamoDB::Model::DeleteItemOutcome DeleteItem(
        const Aws::DynamoDB::Model::DeleteItemRequest& request) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto& table_name = request.GetTableName();
        const auto& key = request.GetKey();

        // Extract primary key
        auto album_id_it = key.find("AlbumId");
        if (album_id_it == key.end()) {
            Aws::DynamoDB::DynamoDBError error;
            return Aws::DynamoDB::Model::DeleteItemOutcome(error);
        }

        std::string album_id = album_id_it->second.GetS();

        // Delete the item
        auto table_it = tables_.find(table_name);
        if (table_it != tables_.end()) {
            table_it->second.erase(album_id);
        }

        Aws::DynamoDB::Model::DeleteItemResult result;
        return Aws::DynamoDB::Model::DeleteItemOutcome(result);
    }

    // Test helper methods
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        tables_.clear();
    }

    size_t getItemCount(const std::string& table_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tables_.find(table_name);
        return it != tables_.end() ? it->second.size() : 0;
    }

private:
    // Simple filter matching (supports basic Published = :published filter)
    bool matchesFilter(const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item,
                      const Aws::DynamoDB::Model::ScanRequest& request) const {
        const auto& filter_expr = request.GetFilterExpression();

        // Simple implementation for "Published = :published" filter
        if (filter_expr.find("Published") != Aws::String::npos) {
            auto published_it = item.find("Published");
            if (published_it != item.end()) {
                auto value_it = request.GetExpressionAttributeValues().find(":published");
                if (value_it != request.GetExpressionAttributeValues().end()) {
                    return published_it->second.GetS() == value_it->second.GetS();
                }
            }
            return false;
        }

        // Simple implementation for "Name = :name" filter
        if (filter_expr.find("#name") != Aws::String::npos || filter_expr.find("Name") != Aws::String::npos) {
            auto name_it = item.find("Name");
            if (name_it != item.end()) {
                auto value_it = request.GetExpressionAttributeValues().find(":name");
                if (value_it != request.GetExpressionAttributeValues().end()) {
                    return name_it->second.GetS() == value_it->second.GetS();
                }
            }
            return false;
        }

        // Default: include item if no recognized filter
        return true;
    }

    // Mutable to allow const methods to modify internal state
    mutable std::map<std::string, std::map<std::string, Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>>> tables_;
    mutable std::mutex mutex_;
};

} // namespace gara

#endif // GARA_TESTS_MOCKS_FAKE_DYNAMODB_CLIENT_H
