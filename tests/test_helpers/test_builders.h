#ifndef GARA_TEST_HELPERS_TEST_BUILDERS_H
#define GARA_TEST_HELPERS_TEST_BUILDERS_H

#include "models/image_metadata.h"
#include "models/album.h"
#include "test_constants.h"
#include <vector>
#include <string>

namespace gara {
namespace test_builders {

/**
 * @brief Builder for TransformRequest objects
 *
 * Provides fluent API for creating TransformRequest test objects with sensible defaults.
 *
 * Example:
 *   auto request = TransformRequestBuilder()
 *       .withImageId("my_image")
 *       .withFormat("png")
 *       .withDimensions(1024, 768)
 *       .build();
 */
class TransformRequestBuilder {
public:
    TransformRequestBuilder()
        : image_id_(test_constants::TEST_IMAGE_ID),
          format_(test_constants::FORMAT_JPEG),
          width_(test_constants::STANDARD_WIDTH_800),
          height_(test_constants::STANDARD_HEIGHT_600) {}

    TransformRequestBuilder& withImageId(const std::string& id) {
        image_id_ = id;
        return *this;
    }

    TransformRequestBuilder& withFormat(const std::string& format) {
        format_ = format;
        return *this;
    }

    TransformRequestBuilder& withDimensions(int width, int height) {
        width_ = width;
        height_ = height;
        return *this;
    }

    TransformRequestBuilder& withWidth(int width) {
        width_ = width;
        return *this;
    }

    TransformRequestBuilder& withHeight(int height) {
        height_ = height;
        return *this;
    }

    TransformRequest build() const {
        return TransformRequest(image_id_, format_, width_, height_);
    }

    // Convenience factory methods
    static TransformRequest defaultJpeg() {
        return TransformRequestBuilder().build();
    }

    static TransformRequest createWithSize(int width, int height) {
        return TransformRequestBuilder()
            .withDimensions(width, height)
            .build();
    }

    static TransformRequest createWithFormat(const std::string& format) {
        return TransformRequestBuilder()
            .withFormat(format)
            .build();
    }

private:
    std::string image_id_;
    std::string format_;
    int width_;
    int height_;
};

/**
 * @brief Builder for CreateAlbumRequest objects
 */
class CreateAlbumRequestBuilder {
public:
    CreateAlbumRequestBuilder()
        : name_("Test Album"),
          description_("Test Description"),
          published_(false) {}

    CreateAlbumRequestBuilder& withName(const std::string& name) {
        name_ = name;
        return *this;
    }

    CreateAlbumRequestBuilder& withDescription(const std::string& desc) {
        description_ = desc;
        return *this;
    }

    CreateAlbumRequestBuilder& withTags(const std::vector<std::string>& tags) {
        tags_ = tags;
        return *this;
    }

    CreateAlbumRequestBuilder& addTag(const std::string& tag) {
        tags_.push_back(tag);
        return *this;
    }

    CreateAlbumRequestBuilder& published(bool pub = true) {
        published_ = pub;
        return *this;
    }

    CreateAlbumRequest build() const {
        CreateAlbumRequest req;
        req.name = name_;
        req.description = description_;
        req.tags = tags_;
        req.published = published_;
        return req;
    }

    // Convenience factory methods
    static CreateAlbumRequest defaultAlbum() {
        return CreateAlbumRequestBuilder().build();
    }

    static CreateAlbumRequest createWithName(const std::string& name) {
        return CreateAlbumRequestBuilder()
            .withName(name)
            .build();
    }

    static CreateAlbumRequest createPublished(const std::string& name) {
        return CreateAlbumRequestBuilder()
            .withName(name)
            .published(true)
            .build();
    }

private:
    std::string name_;
    std::string description_;
    std::vector<std::string> tags_;
    bool published_;
};

/**
 * @brief Builder for UpdateAlbumRequest objects
 */
class UpdateAlbumRequestBuilder {
public:
    UpdateAlbumRequestBuilder()
        : published_(false) {}

    UpdateAlbumRequestBuilder& withName(const std::string& name) {
        name_ = name;
        return *this;
    }

    UpdateAlbumRequestBuilder& withDescription(const std::string& desc) {
        description_ = desc;
        return *this;
    }

    UpdateAlbumRequestBuilder& withCoverImage(const std::string& image_id) {
        cover_image_id_ = image_id;
        return *this;
    }

    UpdateAlbumRequestBuilder& withTags(const std::vector<std::string>& tags) {
        tags_ = tags;
        return *this;
    }

    UpdateAlbumRequestBuilder& published(bool pub = true) {
        published_ = pub;
        return *this;
    }

    UpdateAlbumRequest build() const {
        UpdateAlbumRequest req;
        req.name = name_;
        req.description = description_;
        req.cover_image_id = cover_image_id_;
        req.tags = tags_;
        req.published = published_;
        return req;
    }

private:
    std::string name_;
    std::string description_;
    std::string cover_image_id_;
    std::vector<std::string> tags_;
    bool published_;
};

/**
 * @brief Builder for AddImagesRequest objects
 */
class AddImagesRequestBuilder {
public:
    AddImagesRequestBuilder()
        : position_(-1) {}

    AddImagesRequestBuilder& withImageIds(const std::vector<std::string>& ids) {
        image_ids_ = ids;
        return *this;
    }

    AddImagesRequestBuilder& addImageId(const std::string& id) {
        image_ids_.push_back(id);
        return *this;
    }

    AddImagesRequestBuilder& atPosition(int pos) {
        position_ = pos;
        return *this;
    }

    AddImagesRequestBuilder& appendToEnd() {
        position_ = -1;
        return *this;
    }

    AddImagesRequest build() const {
        AddImagesRequest req;
        req.image_ids = image_ids_;
        req.position = position_;
        return req;
    }

    static AddImagesRequest createWithImages(const std::vector<std::string>& ids) {
        return AddImagesRequestBuilder()
            .withImageIds(ids)
            .build();
    }

private:
    std::vector<std::string> image_ids_;
    int position_;
};

/**
 * @brief Helper for creating test data
 */
class TestDataBuilder {
public:
    /**
     * @brief Create a vector of char data with specified size
     */
    static std::vector<char> createData(size_t size, char fill = 'x') {
        return std::vector<char>(size, fill);
    }

    /**
     * @brief Create binary test data (0-255 repeated)
     */
    static std::vector<char> createBinaryData(size_t size) {
        std::vector<char> data;
        data.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            data.push_back(static_cast<char>(i % 256));
        }
        return data;
    }

    /**
     * @brief Create text content
     */
    static std::vector<char> createTextData(const std::string& text) {
        return std::vector<char>(text.begin(), text.end());
    }

    /**
     * @brief Create fake S3 key
     */
    static std::string createS3Key(const std::string& prefix,
                                   const std::string& suffix) {
        return prefix + "/" + suffix;
    }

    /**
     * @brief Create raw image S3 key
     */
    static std::string createRawImageKey(const std::string& image_id,
                                         const std::string& format = "jpg") {
        return test_constants::S3_RAW_PREFIX + image_id + "." + format;
    }

    /**
     * @brief Create transformed image S3 key
     */
    static std::string createTransformedKey(const std::string& image_id,
                                            const std::string& format,
                                            int width,
                                            int height) {
        return test_constants::S3_TRANSFORMED_PREFIX + image_id + "_" +
               format + "_" + std::to_string(width) + "x" +
               std::to_string(height) + "." + format;
    }
};

} // namespace test_builders
} // namespace gara

#endif // GARA_TEST_HELPERS_TEST_BUILDERS_H
