#ifndef GARA_SERVICES_DYNAMODB_CLIENT_WRAPPER_H
#define GARA_SERVICES_DYNAMODB_CLIENT_WRAPPER_H

#include "../interfaces/dynamodb_client_interface.h"
#include <aws/dynamodb/DynamoDBClient.h>
#include <memory>

namespace gara {

/**
 * @brief Wrapper around the real AWS DynamoDB client
 *
 * This class implements the DynamoDBClientInterface by delegating
 * to the actual AWS SDK DynamoDBClient.
 */
class DynamoDBClientWrapper : public DynamoDBClientInterface {
public:
    explicit DynamoDBClientWrapper(std::shared_ptr<Aws::DynamoDB::DynamoDBClient> client)
        : client_(client) {}

    Aws::DynamoDB::Model::PutItemOutcome PutItem(
        const Aws::DynamoDB::Model::PutItemRequest& request) const override {
        return client_->PutItem(request);
    }

    Aws::DynamoDB::Model::GetItemOutcome GetItem(
        const Aws::DynamoDB::Model::GetItemRequest& request) const override {
        return client_->GetItem(request);
    }

    Aws::DynamoDB::Model::ScanOutcome Scan(
        const Aws::DynamoDB::Model::ScanRequest& request) const override {
        return client_->Scan(request);
    }

    Aws::DynamoDB::Model::DeleteItemOutcome DeleteItem(
        const Aws::DynamoDB::Model::DeleteItemRequest& request) const override {
        return client_->DeleteItem(request);
    }

private:
    std::shared_ptr<Aws::DynamoDB::DynamoDBClient> client_;
};

} // namespace gara

#endif // GARA_SERVICES_DYNAMODB_CLIENT_WRAPPER_H
