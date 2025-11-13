#ifndef GARA_MAPPERS_ALBUM_MAPPER_H
#define GARA_MAPPERS_ALBUM_MAPPER_H

#include "../models/album.h"
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/AttributeValue.h>
#include <aws/core/utils/memory/stl/AWSMap.h>
#include <aws/core/utils/memory/stl/AWSString.h>

namespace gara {
namespace mappers {

/**
 * @brief Mapper class for converting between Album model and DynamoDB items
 */
class AlbumMapper {
public:
    /**
     * @brief Convert DynamoDB item to Album model
     *
     * @param item DynamoDB item attributes
     * @return Album model populated from DynamoDB item
     */
    static Album itemToAlbum(const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item);

    /**
     * @brief Convert Album model to DynamoDB PutItemRequest
     *
     * @param album Album model to convert
     * @param table_name DynamoDB table name
     * @return PutItemRequest ready for DynamoDB operation
     */
    static Aws::DynamoDB::Model::PutItemRequest albumToPutItemRequest(
        const Album& album,
        const std::string& table_name);
};

} // namespace mappers
} // namespace gara

#endif // GARA_MAPPERS_ALBUM_MAPPER_H
