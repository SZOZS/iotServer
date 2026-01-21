#pragma once
#ifndef FRAME_SD_RTU_BUILDER_H
#define FRAME_SD_RTU_BUILDER_H

#include "RTUParser.h"

class FrameRTUBuilder
{
public:
    // 构建 注册帧-反馈帧
    static std::vector<uint8_t> frameRTUResponderRegister(const FrameRTURegister& originalFrame);

    // 构建 数据帧-反馈帧
    static std::vector<uint8_t> frameRTUResponderReportData(const FrameRTUReportData& originalFrame);
};

#endif  // FRAME_SD_RTU_BUILDER_H