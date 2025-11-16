#include "file_utils.h"
#include "logger.h"
#include <openssl/evp.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>

namespace gara {
namespace utils {

std::string FileUtils::calculateSHA256(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return "";
    }

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        return "";
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    const size_t buffer_size = 8192;
    char buffer[buffer_size];

    while (file.read(buffer, buffer_size) || file.gcount() > 0) {
        if (EVP_DigestUpdate(mdctx, buffer, file.gcount()) != 1) {
            EVP_MD_CTX_free(mdctx);
            return "";
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    EVP_MD_CTX_free(mdctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return oss.str();
}

std::string FileUtils::calculateSHA256(const std::vector<char>& data) {
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        return "";
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    if (EVP_DigestUpdate(mdctx, data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    EVP_MD_CTX_free(mdctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return oss.str();
}

std::string FileUtils::getFileExtension(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos == std::string::npos) {
        return "";
    }
    std::string ext = filename.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

std::string FileUtils::createTempFile(const std::string& prefix) {
    std::string temp_dir = std::getenv("TEMP_UPLOAD_DIR") ?
                          std::getenv("TEMP_UPLOAD_DIR") : "/tmp";

    // Validate temp_dir length to prevent buffer overflow
    // Account for: "/" + prefix + "XXXXXX" + null terminator
    if (temp_dir.length() + prefix.length() + 10 > 255) {
        gara::Logger::log_structured(spdlog::level::warn, "Temp directory path too long, using /tmp fallback", {
            {"requested_temp_dir", temp_dir},
            {"prefix", prefix},
            {"fallback", "/tmp"}
        });
        temp_dir = "/tmp";
    }

    char temp_template[256];
    int written = snprintf(temp_template, sizeof(temp_template), "%s/%sXXXXXX",
                           temp_dir.c_str(), prefix.c_str());

    // Check if snprintf truncated the output
    if (written < 0 || written >= static_cast<int>(sizeof(temp_template))) {
        // Path too long, fall back to /tmp
        gara::Logger::log_structured(spdlog::level::warn, "Temp file path construction failed, using /tmp fallback", {
            {"temp_dir", temp_dir},
            {"prefix", prefix},
            {"fallback", "/tmp"}
        });
        snprintf(temp_template, sizeof(temp_template), "/tmp/%sXXXXXX", prefix.c_str());
    }

    int fd = mkstemp(temp_template);
    if (fd == -1) {
        return "";
    }
    close(fd);

    return std::string(temp_template);
}

bool FileUtils::writeToFile(const std::string& filepath, const std::vector<char>& data) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }
    file.write(data.data(), data.size());
    return file.good();
}

std::vector<char> FileUtils::readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        return {};
    }

    return buffer;
}

bool FileUtils::deleteFile(const std::string& filepath) {
    return unlink(filepath.c_str()) == 0;
}

size_t FileUtils::getFileSize(const std::string& filepath) {
    struct stat stat_buf;
    if (stat(filepath.c_str(), &stat_buf) != 0) {
        return 0;
    }
    return stat_buf.st_size;
}

bool FileUtils::fileExists(const std::string& filepath) {
    struct stat buffer;
    return (stat(filepath.c_str(), &buffer) == 0);
}

bool FileUtils::isValidImageFormat(const std::string& extension) {
    // Only accept formats that libvips can actually process
    static const std::vector<std::string> valid_formats = {
        "jpg", "jpeg", "png", "gif", "tiff", "tif", "webp"
    };

    std::string lower_ext = extension;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(), ::tolower);

    return std::find(valid_formats.begin(), valid_formats.end(), lower_ext) != valid_formats.end();
}

std::string FileUtils::getMimeType(const std::string& extension) {
    std::string lower_ext = extension;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(), ::tolower);

    if (lower_ext == "jpg" || lower_ext == "jpeg") return "image/jpeg";
    if (lower_ext == "png") return "image/png";
    if (lower_ext == "gif") return "image/gif";
    if (lower_ext == "bmp") return "image/bmp";
    if (lower_ext == "tiff" || lower_ext == "tif") return "image/tiff";
    if (lower_ext == "webp") return "image/webp";
    if (lower_ext == "svg") return "image/svg+xml";

    return "application/octet-stream";
}

// TempFile implementation
TempFile::TempFile(const std::string& prefix)
    : filepath_(FileUtils::createTempFile(prefix)) {
}

TempFile::~TempFile() {
    if (!filepath_.empty()) {
        FileUtils::deleteFile(filepath_);
    }
}

TempFile::TempFile(TempFile&& other) noexcept
    : filepath_(std::move(other.filepath_)) {
    other.filepath_.clear();
}

TempFile& TempFile::operator=(TempFile&& other) noexcept {
    if (this != &other) {
        if (!filepath_.empty()) {
            FileUtils::deleteFile(filepath_);
        }
        filepath_ = std::move(other.filepath_);
        other.filepath_.clear();
    }
    return *this;
}

bool TempFile::write(const std::vector<char>& data) {
    return FileUtils::writeToFile(filepath_, data);
}

} // namespace utils
} // namespace gara
