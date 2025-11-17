#include "local_file_service.h"
#include "../utils/logger.h"
#include <fstream>
#include <stdexcept>

namespace gara {

LocalFileService::LocalFileService(const std::string& storage_path)
    : S3Service(storage_path, "local", true),  // Skip AWS SDK initialization
      storage_path_(storage_path) {

    // Create storage directory if it doesn't exist
    try {
        std::filesystem::create_directories(storage_path);
        Logger::info("Local file storage initialized at: " + storage_path);
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::error("Failed to create storage directory: " + std::string(e.what()));
        throw std::runtime_error("Failed to initialize local file storage");
    }
}

std::filesystem::path LocalFileService::getFilePath(const std::string& key) const {
    return std::filesystem::path(storage_path_) / key;
}

bool LocalFileService::ensureDirectoryExists(const std::filesystem::path& file_path) {
    try {
        auto parent = file_path.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent)) {
            std::filesystem::create_directories(parent);
        }
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::error("Failed to create directory: " + std::string(e.what()));
        return false;
    }
}

bool LocalFileService::uploadFile(const std::string& local_path, const std::string& key,
                                  const std::string& content_type) {
    try {
        auto dest_path = getFilePath(key);

        if (!ensureDirectoryExists(dest_path)) {
            return false;
        }

        std::filesystem::copy_file(local_path, dest_path,
                                  std::filesystem::copy_options::overwrite_existing);

        Logger::debug("File uploaded: " + local_path + " -> " + dest_path.string());
        return true;

    } catch (const std::filesystem::filesystem_error& e) {
        Logger::error("Failed to upload file: " + std::string(e.what()));
        return false;
    }
}

bool LocalFileService::uploadData(const std::vector<char>& data, const std::string& key,
                                  const std::string& content_type) {
    try {
        auto dest_path = getFilePath(key);

        if (!ensureDirectoryExists(dest_path)) {
            return false;
        }

        std::ofstream file(dest_path, std::ios::binary);
        if (!file) {
            Logger::error("Failed to open file for writing: " + dest_path.string());
            return false;
        }

        file.write(data.data(), data.size());
        file.close();

        Logger::debug("Data uploaded: " + dest_path.string() + " (" + std::to_string(data.size()) + " bytes)");
        return true;

    } catch (const std::exception& e) {
        Logger::error("Failed to upload data: " + std::string(e.what()));
        return false;
    }
}

bool LocalFileService::downloadFile(const std::string& key, const std::string& local_path) {
    try {
        auto src_path = getFilePath(key);

        if (!std::filesystem::exists(src_path)) {
            Logger::error("File not found: " + src_path.string());
            return false;
        }

        // Ensure destination directory exists
        auto dest = std::filesystem::path(local_path);
        if (!ensureDirectoryExists(dest)) {
            return false;
        }

        std::filesystem::copy_file(src_path, local_path,
                                  std::filesystem::copy_options::overwrite_existing);

        Logger::debug("File downloaded: " + src_path.string() + " -> " + local_path);
        return true;

    } catch (const std::filesystem::filesystem_error& e) {
        Logger::error("Failed to download file: " + std::string(e.what()));
        return false;
    }
}

std::vector<char> LocalFileService::downloadData(const std::string& key) {
    try {
        auto src_path = getFilePath(key);

        if (!std::filesystem::exists(src_path)) {
            Logger::error("File not found: " + src_path.string());
            return {};
        }

        std::ifstream file(src_path, std::ios::binary | std::ios::ate);
        if (!file) {
            Logger::error("Failed to open file for reading: " + src_path.string());
            return {};
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size)) {
            Logger::error("Failed to read file: " + src_path.string());
            return {};
        }

        Logger::debug("Data downloaded: " + src_path.string() + " (" + std::to_string(size) + " bytes)");
        return buffer;

    } catch (const std::exception& e) {
        Logger::error("Failed to download data: " + std::string(e.what()));
        return {};
    }
}

bool LocalFileService::objectExists(const std::string& key) {
    auto file_path = getFilePath(key);
    return std::filesystem::exists(file_path);
}

bool LocalFileService::deleteObject(const std::string& key) {
    try {
        auto file_path = getFilePath(key);

        if (!std::filesystem::exists(file_path)) {
            Logger::warn("File not found for deletion: " + file_path.string());
            return false;
        }

        std::filesystem::remove(file_path);
        Logger::debug("File deleted: " + file_path.string());
        return true;

    } catch (const std::filesystem::filesystem_error& e) {
        Logger::error("Failed to delete file: " + std::string(e.what()));
        return false;
    }
}

std::string LocalFileService::generatePresignedUrl(const std::string& key, int expiration_seconds) {
    // For local file service, return a file:// URL or a path
    // In a real HTTP server context, this would return a URL like http://localhost:8080/files/{key}
    auto file_path = getFilePath(key);

    if (!std::filesystem::exists(file_path)) {
        Logger::warn("Generating URL for non-existent file: " + file_path.string());
    }

    // Return as file:// URL for now
    // In production, this should be an HTTP URL served by the application
    return "file://" + file_path.string();
}

} // namespace gara
