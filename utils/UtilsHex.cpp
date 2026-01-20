#include "UtilsHex.h"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace utils {
// 将字节数组转换为十六进制字符串
std::string UtilsHex::bytesToHexString(const char *data, size_t len, bool uppercase, bool with_space) {
  if (data == nullptr || len == 0) return "";

  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  ss << (uppercase ? std::uppercase : std::nouppercase);

  for (size_t i = 0; i < len; ++i) {
    ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(data[i]));
    if (with_space && i != len - 1) ss << " ";
  }
  return ss.str();
}

// 将十六进制字符串转换为字节数组
size_t UtilsHex::hexStringToBytes(const std::string &hex_str, char *out_data) {
  if (out_data == nullptr || hex_str.empty()) return 0;

  std::string processed_str;
  //   移除所有空格
  for (char c : hex_str) {
    if (c != ' ') processed_str += c;
  }

  //   十六进制字符串长度必须为偶数
  if (processed_str.length() % 2 != 0) return 0;

  size_t byte_count = processed_str.length() / 2;
  for (size_t i = 0; i < byte_count; ++i) {
    // 提取两个字符作为一个字节
    std::string byte_str = processed_str.substr(i * 2, 2);

    // 转换为十六进制数值
    char *end_ptr = nullptr;
    unsigned long val = std::strtoul(byte_str.c_str(), &end_ptr, 16);

    // 检查转换是否成功
    if (end_ptr != byte_str.c_str() + 2) return 0;

    out_data[i] = static_cast<char>(val & 0xFF);
  }
  return byte_count;
}

// 将十六进制字符串转换为 uint8_t 向量的具体实现
std::vector<uint8_t> UtilsHex::hexStringToVector(const std::string &hex_str) {
  // 1·预处理：移除所有空格
  std::string processed_str;
  for (char c : hex_str) {
    if (c != ' ') processed_str += c;
  }
  // 2·计算预期字节数（处理后字符串长度的 1/2）
  size_t byte_count = processed_str.size() / 2;
  std::vector<uint8_t> bytes(byte_count);
  // 3·空输入直接返回空向量
  if (byte_count == 0) return bytes;
  // 4·复用 hexStringToBytes 进行转换，校验结果
  size_t converted = hexStringToBytes(hex_str, reinterpret_cast<char *>(bytes.data()));
  if (converted != byte_count) {
    return {};  // 转换失败（如含无效字符，返回空向量）
  }
  return bytes;
}

// 补充Base64编码表
const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string utils::UtilsHex::base64_encode(const unsigned char* data, size_t len) {
  std::string ret;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  while (len--) {
    char_array_3[i++] = *(data++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (i = 0; (i < 4); i++) ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 3; j++) char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = 64;  // 填充符 '='

    for (j = 0; (j < i + 1); j++) ret += base64_chars[char_array_4[j]];

    while ((i++ < 3)) ret += '=';  // 补充剩余填充符
  }

  return ret;
}

}  // namespace utils
