#include "id_generator.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace gara {
namespace utils {

std::string IdGenerator::generateAlbumId() {
    // Generate UUID-like ID with timestamp prefix for sorting
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    std::time_t now = std::time(nullptr);
    std::ostringstream oss;
    oss << now << "_";

    // Generate simple UUID (8-4-4-4-12 hex format)
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << (dis(gen) & 0xFFFFFFFF) << "-";
    oss << std::setw(4) << (dis(gen) & 0xFFFF) << "-";
    oss << std::setw(4) << ((dis(gen) & 0x0FFF) | 0x4000) << "-";
    oss << std::setw(4) << ((dis(gen) & 0x3FFF) | 0x8000) << "-";
    oss << std::setw(12) << (dis(gen) & 0xFFFFFFFFFFFF);

    return oss.str();
}

} // namespace utils
} // namespace gara
