#pragma once
#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <errno.h>
#include <sys/stat.h>

#include <ctime>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>

namespace utils {
class FileManager {
 public:
  //   创建文件夹
  static bool createDirectory(const std::string& path) {
    mode_t mode = 0775;
    size_t pos = 0;
    while (pos < path.size()) {
      size_t next = path.find('/', pos);
      if (next == std::string::npos) {
        next = path.size();
      }
      std::string subDir = path.substr(0, next);
      if (!subDir.empty() && mkdir(subDir.c_str(), mode) != 0 && errno != EEXIST) {
        return false;
      }
      pos = next + 1;
    }
    return true;
  }

  // 获取当前日期目录
  static std::string getDateDirectory() {
    std::time_t now = std::time(nullptr);
    std::tm localTime;
    localtime_r(&now, &localTime);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "logs/output/%Y/%m/%d", &localTime);
    return std::string(buffer);
  }

  // 生成会话日志路径（格式：logs/YYYY-MM-DD/sessionId-app.log）
  static std::string getSessionLogPath(const std::string& sessionId) {
    std::string dateDir = getDateDirectory();
    if (!createDirectory(dateDir)) {
      throw std::runtime_error("Failed to create log directory: " + dateDir);
    }
    return dateDir + "/" + sessionId + "-app.log";
  }

  // 检查文件是否存在
  static bool fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
  }

 private:
  FileManager() = delete;
  ~FileManager() = delete;
};
}  // namespace utils

#endif  // FILE_MANAGER_H