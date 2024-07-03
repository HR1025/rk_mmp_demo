//
// DisplayWayland.h
//
// Library: Common
// Package: Display
// Module:  Wayland
// 

#pragma once

#include <string>

#include "AbstractDisplay.h"

// forward declaration
struct wl_shm;
struct wl_shell;
struct wl_buffer;
struct wl_surface;
struct wl_display;
struct wl_shm_pool;
struct wl_registry;
struct wl_compositor;
struct wl_shell_surface;

namespace Mmp
{

/**
 * @brief 对接 wayland 桌面系统客户端协议
 * @note  实际上 wl_shell 已经过时,目前 wayland 更加推荐使用 XDG Shell,
 *        但是 wayland 使用命令行工具将 xml 所描述的 proto 转化为 C 代码,
 *        给整个项目的集成构建带来的负担(尤其对于交叉编译而言)
 *        DisplayWayland 仅仅只是用于观察图像输出,所以使用 wl_shell
 * @sa    wayland 概述 : https://blog.csdn.net/weixin_45449806/article/details/127906468
 */
class DisplayWayland : public AbstractDisplay
{
public:
    DisplayWayland();
public:
    bool Init() override;
    bool UnInit() override;
    bool Open(PixelsInfo info) override;
    bool Close() override;
    void UpdateWindow(const uint32_t* frameBuffer, PixelsInfo info) override;
private:
    bool InitBuffer();
    void UnInitBuffer();
public:
    std::string             _title;
    std::string             _xdgRuntimeDir;
    uint32_t                _wlFormat;
    uint32_t                _width;
    uint32_t                _height;
    PixelFormat             _format;
public: /* must be public due to C style callback */
    struct wl_display*          _display;
    struct wl_registry*         _register;
    struct wl_shell*            _shell;
    struct wl_compositor*       _compositor;
    struct wl_surface*          _surface;
    struct wl_shell_surface*    _shellSurface;
public: /* shm buffer */
    bool                        _isBufferInited;
    int32_t                     _shmFd;
    struct wl_buffer*           _buffer;
    struct wl_shm_pool*         _pool;
    struct wl_shm*              _shm;
    uint8_t*                    _pixels;
};

} // namespace Mmp