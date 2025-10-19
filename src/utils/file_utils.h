#ifndef GARA_FILE_UTILS_H
#define GARA_FILE_UTILS_H

#include <string>
#include <vector>
#include <memory>

namespace gara {
namespace utils {

class FileUtils {
public:
    // Generate SHA256 hash from file
    static std::string calculateSHA256(const std::string& filepath);

    // Generate SHA256 hash from binary data
    static std::string calculateSHA256(const std::vector<char>& data);

    // Get file extension from filename
    static std::string getFileExtension(const std::string& filename);

    // Create temporary file and return path
    static std::string createTempFile(const std::string& prefix = "gara_");

    // Write data to file
    static bool writeToFile(const std::string& filepath, const std::vector<char>& data);

    // Read file into vector
    static std::vector<char> readFile(const std::string& filepath);

    // Delete file
    static bool deleteFile(const std::string& filepath);

    // Get file size
    static size_t getFileSize(const std::string& filepath);

    // Check if file exists
    static bool fileExists(const std::string& filepath);

    // Validate image file format
    static bool isValidImageFormat(const std::string& extension);

    // Get MIME type from extension
    static std::string getMimeType(const std::string& extension);
};

// RAII wrapper for temporary files
class TempFile {
public:
    explicit TempFile(const std::string& prefix = "gara_");
    ~TempFile();

    // Disable copy
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    // Enable move
    TempFile(TempFile&& other) noexcept;
    TempFile& operator=(TempFile&& other) noexcept;

    std::string getPath() const { return filepath_; }
    bool write(const std::vector<char>& data);

private:
    std::string filepath_;
};

} // namespace utils
} // namespace gara

#endif // GARA_FILE_UTILS_H
