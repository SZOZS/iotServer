#include "../../FrameAssemblerFactory.h"
#include "../../FrameParserFactory.h"
#include "RTUAssembler.h"
#include "RTUParser.h"

// 注册解析器到工厂
static bool registerRTUParser = []() {
  FrameParserFactory::registerParser("SD_RTU", {FRAME_SD_RTU_START}, []() { return std::make_unique<RTUParser>(); });
  return true;
}();

// 注册组装器到工厂
static bool registerRTUAssembler = []() {
  FrameAssemblerFactory::registerAssembler("SD_RTU", {FRAME_SD_RTU_START}, []() { return std::make_unique<RTUAssembler>(); });
  return true;
}();

// // 未来可能添加的其他组件注册（如构建器）
// static bool registerRTUBuilder = []() {
//   FrameBuilderFactory::registerBuilder("SD_RTU", []() {
//     return std::make_unique<RTUBuilder>();
//   });
//   return true;
// }();
