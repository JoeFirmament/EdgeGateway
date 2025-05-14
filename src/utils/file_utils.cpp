#include "utils/file_utils.h"
#include <fstream>
#include <filesystem>
#include <random>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <unistd.h>
#include <iostream>

namespace fs = std::filesystem;

namespace cam_server {
namespace utils {

bool FileUtils::fileExists(const std::string& file_path) {
    std::cerr << "[UTILS][file_utils.cpp:fileExists] 检查文件: " << file_path << std::endl;

    // 使用更简单的方法检查文件是否存在
    FILE* file = fopen(file_path.c_str(), "r");
    if (file) {
        fclose(file);
        std::cerr << "[UTILS][file_utils.cpp:fileExists] 文件存在并且可以打开" << std::endl;
        return true;
    } else {
        std::cerr << "[UTILS][file_utils.cpp:fileExists] 文件不存在或无法打开，错误: " << strerror(errno) << std::endl;
        return false;
    }
}

bool FileUtils::directoryExists(const std::string& dir_path) {
    try {
        return fs::exists(dir_path) && fs::is_directory(dir_path);
    } catch (const std::exception&) {
        return false;
    }
}

bool FileUtils::createDirectory(const std::string& dir_path, bool recursive) {
    try {
        if (directoryExists(dir_path)) {
            return true;
        }

        if (recursive) {
            return fs::create_directories(dir_path);
        } else {
            return fs::create_directory(dir_path);
        }
    } catch (const std::exception&) {
        return false;
    }
}

bool FileUtils::deleteFile(const std::string& file_path) {
    try {
        if (!fileExists(file_path)) {
            return false;
        }

        return fs::remove(file_path);
    } catch (const std::exception&) {
        return false;
    }
}

bool FileUtils::deleteDirectory(const std::string& dir_path, bool recursive) {
    try {
        if (!directoryExists(dir_path)) {
            return false;
        }

        if (recursive) {
            return fs::remove_all(dir_path) > 0;
        } else {
            return fs::remove(dir_path);
        }
    } catch (const std::exception&) {
        return false;
    }
}

bool FileUtils::rename(const std::string& old_path, const std::string& new_path) {
    try {
        if (!fs::exists(old_path)) {
            return false;
        }

        fs::rename(old_path, new_path);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int64_t FileUtils::getFileSize(const std::string& file_path) {
    try {
        if (!fileExists(file_path)) {
            return -1;
        }

        return fs::file_size(file_path);
    } catch (const std::exception&) {
        return -1;
    }
}

std::chrono::system_clock::time_point FileUtils::getFileModifyTime(const std::string& file_path) {
    try {
        if (!fileExists(file_path)) {
            return std::chrono::system_clock::time_point();
        }

        auto file_time = fs::last_write_time(file_path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file_time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());

        return sctp;
    } catch (const std::exception&) {
        return std::chrono::system_clock::time_point();
    }
}

std::chrono::system_clock::time_point FileUtils::getFileCreateTime(const std::string& file_path) {
    try {
        if (!fileExists(file_path)) {
            return std::chrono::system_clock::time_point();
        }

        struct stat st;
        if (stat(file_path.c_str(), &st) != 0) {
            return std::chrono::system_clock::time_point();
        }

        return std::chrono::system_clock::from_time_t(st.st_ctime);
    } catch (const std::exception&) {
        return std::chrono::system_clock::time_point();
    }
}

std::string FileUtils::getFileExtension(const std::string& file_path) {
    try {
        fs::path path(file_path);
        return path.extension().string();
    } catch (const std::exception&) {
        return "";
    }
}

std::string FileUtils::getFileName(const std::string& file_path) {
    try {
        fs::path path(file_path);
        return path.filename().string();
    } catch (const std::exception&) {
        return "";
    }
}

std::string FileUtils::getFileNameWithoutExtension(const std::string& file_path) {
    try {
        fs::path path(file_path);
        return path.stem().string();
    } catch (const std::exception&) {
        return "";
    }
}

std::string FileUtils::getDirectoryPath(const std::string& file_path) {
    try {
        fs::path path(file_path);
        return path.parent_path().string();
    } catch (const std::exception&) {
        return "";
    }
}

std::string FileUtils::getAbsolutePath(const std::string& path) {
    try {
        return fs::absolute(path).string();
    } catch (const std::exception&) {
        return "";
    }
}

std::string FileUtils::normalizePath(const std::string& path) {
    try {
        return fs::path(path).lexically_normal().string();
    } catch (const std::exception&) {
        return path;
    }
}

std::string FileUtils::joinPath(const std::string& path1, const std::string& path2) {
    try {
        fs::path p1(path1);
        fs::path p2(path2);
        return (p1 / p2).string();
    } catch (const std::exception&) {
        return path1 + "/" + path2;
    }
}

std::vector<std::string> FileUtils::getFileList(const std::string& dir_path, bool recursive) {
    std::vector<std::string> result;

    try {
        if (!directoryExists(dir_path)) {
            return result;
        }

        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
                if (fs::is_regular_file(entry.path())) {
                    result.push_back(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(dir_path)) {
                if (fs::is_regular_file(entry.path())) {
                    result.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception&) {
        // 忽略异常
    }

    return result;
}

std::vector<std::string> FileUtils::getDirectoryList(const std::string& dir_path, bool recursive) {
    std::vector<std::string> result;

    try {
        if (!directoryExists(dir_path)) {
            return result;
        }

        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
                if (fs::is_directory(entry.path())) {
                    result.push_back(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(dir_path)) {
                if (fs::is_directory(entry.path())) {
                    result.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception&) {
        // 忽略异常
    }

    return result;
}

std::string FileUtils::readFile(const std::string& file_path) {
    std::cerr << "FileUtils::readFile 读取文件: " << file_path << std::endl;
    try {
        if (!fileExists(file_path)) {
            std::cerr << "  文件不存在，无法读取" << std::endl;
            return "";
        }

        std::cerr << "  正在打开文件..." << std::endl;
        std::ifstream file(file_path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "  无法打开文件，错误: " << strerror(errno) << std::endl;
            return "";
        }

        std::cerr << "  文件打开成功，正在读取内容..." << std::endl;
        std::string content;
        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        std::cerr << "  文件大小: " << size << " 字节" << std::endl;
        content.resize(size);
        file.seekg(0, std::ios::beg);
        file.read(&content[0], content.size());
        file.close();

        std::cerr << "  文件读取完成，内容长度: " << content.size() << " 字节" << std::endl;
        if (content.size() > 100) {
            std::cerr << "  文件内容前100个字符: " << content.substr(0, 100) << "..." << std::endl;
        } else {
            std::cerr << "  文件内容: " << content << std::endl;
        }

        return content;
    } catch (const std::exception& e) {
        std::cerr << "  读取文件时发生异常: " << e.what() << std::endl;
        return "";
    }
}

std::vector<uint8_t> FileUtils::readBinaryFile(const std::string& file_path) {
    try {
        if (!fileExists(file_path)) {
            return {};
        }

        std::ifstream file(file_path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            return {};
        }

        std::vector<uint8_t> content;
        file.seekg(0, std::ios::end);
        content.resize(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(content.data()), content.size());
        file.close();

        return content;
    } catch (const std::exception&) {
        return {};
    }
}

bool FileUtils::writeFile(const std::string& file_path, const std::string& content, bool append) {
    std::cerr << "[UTILS][file_utils.cpp:writeFile] 写入文件: " << file_path << ", 内容长度: " << content.size() << " 字节, 追加模式: " << (append ? "是" : "否") << std::endl;

    try {
        // 确保目录存在
        std::string dir_path = getDirectoryPath(file_path);
        std::cerr << "[UTILS][file_utils.cpp:writeFile] 目录路径: " << dir_path << std::endl;

        if (!dir_path.empty() && !directoryExists(dir_path)) {
            std::cerr << "[UTILS][file_utils.cpp:writeFile] 目录不存在，尝试创建..." << std::endl;
            if (!createDirectory(dir_path, true)) {
                std::cerr << "[UTILS][file_utils.cpp:writeFile] 创建目录失败" << std::endl;
                return false;
            }
            std::cerr << "[UTILS][file_utils.cpp:writeFile] 目录创建成功" << std::endl;
        } else {
            std::cerr << "[UTILS][file_utils.cpp:writeFile] 目录已存在" << std::endl;
        }

        std::ios_base::openmode mode = std::ios::out | std::ios::binary;
        if (append) {
            mode |= std::ios::app;
        }

        std::cerr << "[UTILS][file_utils.cpp:writeFile] 正在打开文件..." << std::endl;
        std::ofstream file(file_path, mode);
        if (!file.is_open()) {
            std::cerr << "[UTILS][file_utils.cpp:writeFile] 无法打开文件，错误: " << strerror(errno) << std::endl;
            return false;
        }
        std::cerr << "[UTILS][file_utils.cpp:writeFile] 文件打开成功" << std::endl;

        std::cerr << "[UTILS][file_utils.cpp:writeFile] 正在写入内容..." << std::endl;
        file.write(content.data(), content.size());
        file.close();
        std::cerr << "[UTILS][file_utils.cpp:writeFile] 内容写入成功，文件已关闭" << std::endl;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[UTILS][file_utils.cpp:writeFile] 写入文件时发生异常: " << e.what() << std::endl;
        return false;
    }
}

bool FileUtils::writeBinaryFile(const std::string& file_path, const std::vector<uint8_t>& content, bool append) {
    try {
        // 确保目录存在
        std::string dir_path = getDirectoryPath(file_path);
        if (!dir_path.empty() && !directoryExists(dir_path)) {
            if (!createDirectory(dir_path, true)) {
                return false;
            }
        }

        std::ios_base::openmode mode = std::ios::out | std::ios::binary;
        if (append) {
            mode |= std::ios::app;
        }

        std::ofstream file(file_path, mode);
        if (!file.is_open()) {
            return false;
        }

        file.write(reinterpret_cast<const char*>(content.data()), content.size());
        file.close();

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool FileUtils::copyFile(const std::string& src_path, const std::string& dst_path, bool overwrite) {
    try {
        if (!fileExists(src_path)) {
            return false;
        }

        if (fileExists(dst_path) && !overwrite) {
            return false;
        }

        // 确保目标目录存在
        std::string dst_dir = getDirectoryPath(dst_path);
        if (!dst_dir.empty() && !directoryExists(dst_dir)) {
            if (!createDirectory(dst_dir, true)) {
                return false;
            }
        }

        fs::copy_file(src_path, dst_path, overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string FileUtils::getTempFilePath(const std::string& prefix, const std::string& suffix) {
    try {
        std::string temp_dir = getTempDirectoryPath();
        if (temp_dir.empty()) {
            return "";
        }

        // 生成随机文件名
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 35); // 0-9, a-z

        std::string random_str;
        for (int i = 0; i < 10; ++i) {
            int r = dis(gen);
            random_str += (r < 10) ? ('0' + r) : ('a' + r - 10);
        }

        std::string file_name = prefix + random_str + suffix;
        return joinPath(temp_dir, file_name);
    } catch (const std::exception&) {
        return "";
    }
}

std::string FileUtils::getTempDirectoryPath() {
    try {
        return fs::temp_directory_path().string();
    } catch (const std::exception&) {
        return "/tmp";
    }
}

std::string FileUtils::getCurrentWorkingDirectory() {
    try {
        return fs::current_path().string();
    } catch (const std::exception&) {
        return "";
    }
}

bool FileUtils::setCurrentWorkingDirectory(const std::string& dir_path) {
    try {
        if (!directoryExists(dir_path)) {
            return false;
        }

        fs::current_path(dir_path);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int64_t FileUtils::getAvailableDiskSpace(const std::string& path) {
    try {
        struct statvfs stat;
        if (statvfs(path.c_str(), &stat) != 0) {
            return -1;
        }

        return static_cast<int64_t>(stat.f_bavail) * static_cast<int64_t>(stat.f_frsize);
    } catch (const std::exception&) {
        return -1;
    }
}

int64_t FileUtils::getTotalDiskSpace(const std::string& path) {
    try {
        struct statvfs stat;
        if (statvfs(path.c_str(), &stat) != 0) {
            return -1;
        }

        return static_cast<int64_t>(stat.f_blocks) * static_cast<int64_t>(stat.f_frsize);
    } catch (const std::exception&) {
        return -1;
    }
}

void FileUtils::traverseDirectory(const std::string& dir_path,
                                std::function<bool(const std::string&)> callback,
                                bool recursive) {
    try {
        if (!directoryExists(dir_path) || !callback) {
            return;
        }

        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
                if (!callback(entry.path().string())) {
                    break;
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(dir_path)) {
                if (!callback(entry.path().string())) {
                    break;
                }
            }
        }
    } catch (const std::exception&) {
        // 忽略异常
    }
}

} // namespace utils
} // namespace cam_server
