#ifndef GARA_UTILS_ID_GENERATOR_H
#define GARA_UTILS_ID_GENERATOR_H

#include <string>

namespace gara {
namespace utils {

/**
 * @brief Utility class for generating unique identifiers
 */
class IdGenerator {
public:
    /**
     * @brief Generate a unique album ID with timestamp prefix
     *
     * Generates UUID-like ID in format: {timestamp}_{uuid}
     * The timestamp prefix enables chronological sorting.
     *
     * @return Unique album ID string
     */
    static std::string generateAlbumId();
};

} // namespace utils
} // namespace gara

#endif // GARA_UTILS_ID_GENERATOR_H
