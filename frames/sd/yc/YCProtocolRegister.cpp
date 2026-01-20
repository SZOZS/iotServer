#include "../../FrameAssemblerFactory.h"
#include "../../FrameParserFactory.h"
#include "YCAssembler.h"
#include "YCParser.h"

// 注册解析器到工厂
static bool registerYCParser = []() {
  FrameParserFactory::registerParser("SD_YC", {GB_FRAME_HEADER}, []() { return std::make_unique<YCParser>(); });
  return true;
}();

// 注册组装器到工厂
static bool registerYCAssembler = []() {
  FrameAssemblerFactory::registerAssembler("SD_YC", {GB_FRAME_TAIL}, []() { return std::make_unique<YCAssembler>(); });
  return true;
}();

// // 未来可能添加的其他组件注册（如构建器）
// static bool registerYCBuilder = []() {
//   FrameBuilderFactory::registerBuilder("SD_YC", []() {
//     return std::make_unique<YCBuilder>();
//   });
//   return true;
// }();
