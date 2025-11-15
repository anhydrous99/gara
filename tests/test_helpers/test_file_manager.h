#ifndef GARA_TEST_HELPERS_TEST_FILE_MANAGER_H
#define GARA_TEST_HELPERS_TEST_FILE_MANAGER_H

#include <string>
#include <atomic>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <thread>

namespace gara {
namespace test_helpers {

/**
 * @brief Manages temporary file paths for tests to avoid collisions
 *
 * Generates unique temp file paths that are safe for parallel test execution.
 * Uses atomic counter and timestamp to ensure uniqueness even across threads.
 */
class TestFileManager {
public:
    /**
     * @brief Create a unique temporary file path
     *
     * @param prefix Prefix for the filename (e.g., "test_image_")
     * @param extension Optional file extension (e.g., ".jpg")
     * @return Unique file path in /tmp directory
     *
     * Example:
     *   auto path = TestFileManager::createUniquePath("test_", ".jpg");
     *   // Returns: "/tmp/gara_test_<timestamp>_<counter>.jpg"
     */
    static std::string createUniquePath(const std::string& prefix,
                                       const std::string& extension = "") {
        static std::atomic<int> counter{0};

        std::ostringstream oss;
        oss << "/tmp/gara_"
            << prefix
            << std::time(nullptr) << "_"
            << counter++
            << extension;

        return oss.str();
    }

    /**
     * @brief Create a temporary file path with thread ID for better debugging
     *
     * @param prefix Prefix for the filename
     * @param extension File extension
     * @return Unique file path including thread ID
     */
    static std::string createThreadSafePath(const std::string& prefix,
                                           const std::string& extension = "") {
        static std::atomic<int> counter{0};

        std::ostringstream oss;
        oss << "/tmp/gara_"
            << prefix
            << "_tid_" << std::hex << std::this_thread::get_id()
            << "_" << std::dec << std::time(nullptr)
            << "_" << counter++
            << extension;

        return oss.str();
    }

    /**
     * @brief Create a temporary directory path
     *
     * @param prefix Prefix for the directory name
     * @return Unique directory path
     */
    static std::string createUniqueDirPath(const std::string& prefix) {
        return createUniquePath(prefix + "_dir", "");
    }

    /**
     * @brief Get the standard temp directory
     *
     * @return Path to temp directory ("/tmp" on Unix-like systems)
     */
    static std::string getTempDir() {
        return "/tmp";
    }

    /**
     * @brief Create a predictable path for a given test name
     *
     * Useful when you need to reference the same file multiple times in a test.
     *
     * @param test_name Name of the test
     * @param extension File extension
     * @return Predictable file path based on test name
     */
    static std::string createTestPath(const std::string& test_name,
                                      const std::string& extension = ".txt") {
        return "/tmp/gara_" + test_name + extension;
    }
};

} // namespace test_helpers
} // namespace gara

#endif // GARA_TEST_HELPERS_TEST_FILE_MANAGER_H
