#pragma once
#ifndef UTILS_HEX_H
#define UTILS_HEX_H

#include <cstddef>
#include <string>
#include <vector>

#include "../configs/ConfigHex.h"

namespace utils {
// 十六进制转换工具类
// 提供字节与十六进制字符串之间的相互转换功能
class UtilsHex {
 public:
  // 将字节数组转换为十六进制字符串
  // @param data 字节数据指针
  // @param len 字节长度
  // @param uppercase 是否使用大写字母（默认小写）
  // @param with_space 是否在字节间添加空格（默认添加）
  // @return 转换后的十六进制字符串
  // 重载1:物参数版本，使用配置中的默认值
  static std::string bytesToHexString(const char* data, size_t len) {
    // 从配置获取大小写，默认带空格
    return bytesToHexString(data, len, ConfigHex::getInstance().isHexUppercase(), ConfigHex::getInstance().isWithSpace());
  }
  // 重载2:完整参数版本，支持显示制定
  static std::string bytesToHexString(const char* data, size_t len, bool uppercase, bool with_space = true);

  // 将十六进制字符串转换为字节数组
  // @param hex_str 十六进制字符串（可带空格分割）
  // @param out_data 输出字节数组
  // @return 转换成功的字节数，失败返回0
  static size_t hexStringToBytes(const std::string& hex_str, char* out_data);

  // 将十六进制字符串转换为uint8_t类型的向量（字节数组）
  // 功能说明：
  // - 移除输入字符串中的空格（支持带空格格式如”A1 B2 C3“ 和不带空格格式如 ”A1B2C3“）
  // - 校验十六进制字符串有效性（仅允许 0-9、a-f、A_F 字符）
  // - 内部复用 hexStringToBytes 逻辑，确保转换规则一致
  // @param hex_str 输入的十六进制字符串（可包含空格，长度不限但需符合十六进制格式）
  // @return std::vector<uint8_t> 转换后的字节向量
  // - 成功：包含转换后的自己序列，（长度为处理后字符串长度的1/2）
  // - 失败：返回空向量（如输入包含无效字符、处理后长度为奇数等）
  static std::vector<uint8_t> hexStringToVector(const std::string& hex_str);

  // Base64 编码函数
  static std::string base64_encode(const unsigned char* data, size_t len);

 private:
  // 私有构造函数，禁止实例化（工具类不需要实例）
  UtilsHex() = default;
  ~UtilsHex() = default;
};
}  // namespace utils

#endif  // UTILS_HEX_H