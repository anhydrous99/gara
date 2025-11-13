#ifndef GARA_MOCK_DYNAMODB_CLIENT_H
#define GARA_MOCK_DYNAMODB_CLIENT_H

#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/GetItemRequest.h>
#include <aws/dynamodb/model/ScanRequest.h>
#include <aws/dynamodb/model/DeleteItemRequest.h>
#include <map>
#include <vector>

namespace gara {
namespace testing {

// Fake DynamoDB implementation with in-memory storage
class FakeDynamoDBStorage {
public:
    using Item = Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>;

    void putItem(const std::string& table_name, const Item& item) {
        auto it = item.find("AlbumId");
        if (it != item.end()) {
            std::string album_id = it->second.GetS();
            tables_[table_name][album_id] = item;
        }
    }

    Item getItem(const std::string& table_name, const std::string& album_id) {
        if (tables_.find(table_name) != tables_.end()) {
            auto& table = tables_[table_name];
            if (table.find(album_id) != table.end()) {
                return table[album_id];
            }
        }
        return Item();
    }

    std::vector<Item> scan(const std::string& table_name) {
        std::vector<Item> items;
        if (tables_.find(table_name) != tables_.end()) {
            for (const auto& pair : tables_[table_name]) {
                items.push_back(pair.second);
            }
        }
        return items;
    }

    bool deleteItem(const std::string& table_name, const std::string& album_id) {
        if (tables_.find(table_name) != tables_.end()) {
            auto& table = tables_[table_name];
            return table.erase(album_id) > 0;
        }
        return false;
    }

    void clear() {
        tables_.clear();
    }

private:
    std::map<std::string, std::map<std::string, Item>> tables_;
};

} // namespace testing
} // namespace gara

#endif // GARA_MOCK_DYNAMODB_CLIENT_H
