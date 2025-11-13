#ifndef GARA_INTERFACES_DYNAMODB_CLIENT_INTERFACE_H
#define GARA_INTERFACES_DYNAMODB_CLIENT_INTERFACE_H

#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/PutItemResult.h>
#include <aws/dynamodb/model/GetItemRequest.h>
#include <aws/dynamodb/model/GetItemResult.h>
#include <aws/dynamodb/model/ScanRequest.h>
#include <aws/dynamodb/model/ScanResult.h>
#include <aws/dynamodb/model/DeleteItemRequest.h>
#include <aws/dynamodb/model/DeleteItemResult.h>
#include <aws/core/utils/Outcome.h>

namespace gara {

/**
 * @brief Interface for DynamoDB client operations
 *
 * This interface allows for dependency injection and testing with mock implementations.
 */
class DynamoDBClientInterface {
public:
    virtual ~DynamoDBClientInterface() = default;

    /**
     * @brief Put an item into a DynamoDB table
     */
    virtual Aws::DynamoDB::Model::PutItemOutcome PutItem(
        const Aws::DynamoDB::Model::PutItemRequest& request) const = 0;

    /**
     * @brief Get an item from a DynamoDB table
     */
    virtual Aws::DynamoDB::Model::GetItemOutcome GetItem(
        const Aws::DynamoDB::Model::GetItemRequest& request) const = 0;

    /**
     * @brief Scan a DynamoDB table
     */
    virtual Aws::DynamoDB::Model::ScanOutcome Scan(
        const Aws::DynamoDB::Model::ScanRequest& request) const = 0;

    /**
     * @brief Delete an item from a DynamoDB table
     */
    virtual Aws::DynamoDB::Model::DeleteItemOutcome DeleteItem(
        const Aws::DynamoDB::Model::DeleteItemRequest& request) const = 0;
};

} // namespace gara

#endif // GARA_INTERFACES_DYNAMODB_CLIENT_INTERFACE_H
