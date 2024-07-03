//
// AbstractDisplay.h
//
// Library: Common
// Package: Display
// Module:  Display
// 

#pragma once

#include <memory>

#include "Common/LogMessage.h"
#include "Common/PixelsInfo.h"

#define  DISPLAY_LOG_TRACE      MMP_MLOG_TRACE("Display")    
#define  DISPLAY_LOG_DEBUG      MMP_MLOG_DEBUG("Display")    
#define  DISPLAY_LOG_INFO       MMP_MLOG_INFO("Display")     
#define  DISPLAY_LOG_WARN       MMP_MLOG_WARN("Display")     
#define  DISPLAY_LOG_ERROR      MMP_MLOG_ERROR("Display")    
#define  DISPLAY_LOG_FATAL      MMP_MLOG_FATAL("Display")    

namespace Mmp
{

/**
 * @brief  窗口创建器
 * @note   1 - CPU
 *         2 - 非线程安全
 *         3 - 简易版,只能用于调试不可用于生产
 */
class AbstractDisplay
{
public:
    using ptr = std::shared_ptr<AbstractDisplay>;
public:
    virtual ~AbstractDisplay() = default;
public:
    /**
     * @brief      根据 className 创建 Display
     * @param[in]  className : DisplaySDL or DisplayWayland
     * @note       当 className 为空时, 寻找一个默认的 display 创建并返回
     */
    static AbstractDisplay::ptr Create(const std::string& className = "");
public:
    /**
     * @brief 初始化
     */
    virtual bool Init() = 0;
    /**
     * @brief 重置
     */
    virtual bool UnInit() = 0;
    /**
     * @brief      打开窗口
     * @param[in]  info : 像素描述信息
     * @note       PixelFormat 仅做参考用途(即实际打开的画面并不一定是此颜色),但是一定打开成功, UpdateWindow
     *             一定支持此像素格式
     * @sa         PixelsInfo
     */
    virtual bool Open(PixelsInfo info = {1080, 1920, 8, PixelFormat::RGBA8888}) = 0;
    /**
     * @brief      关闭窗口
     */
    virtual bool Close() = 0;
    /**
     * @brief      更新整个窗口
     * @param[in]  frameBuffer
     * @note       info 信息需与 Open 时保持一致, 不同平台 surface 的像素格式不同,在允许的情况下会进行软件转换
     * @todo       有需求更新 window 的话, 提供一个 UpdateSubWindow 即可
     *             (目前来看没有这个需求)
     */
    virtual void UpdateWindow(const uint32_t* frameBuffer, PixelsInfo info = {1920, 1080, 8, PixelFormat::RGBA8888}) = 0;
};

} // namespace Mmp