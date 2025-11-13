#ifndef GARA_EXCEPTIONS_ALBUM_EXCEPTIONS_H
#define GARA_EXCEPTIONS_ALBUM_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace gara {
namespace exceptions {

/**
 * @brief Exception thrown when a requested resource is not found
 */
class NotFoundException : public std::runtime_error {
public:
    explicit NotFoundException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown when input validation fails
 */
class ValidationException : public std::runtime_error {
public:
    explicit ValidationException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown when a resource conflict occurs (e.g., duplicate name)
 */
class ConflictException : public std::runtime_error {
public:
    explicit ConflictException(const std::string& message)
        : std::runtime_error(message) {}
};

} // namespace exceptions
} // namespace gara

#endif // GARA_EXCEPTIONS_ALBUM_EXCEPTIONS_H
