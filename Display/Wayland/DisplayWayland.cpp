#include "DisplayWayland.h"

#include <unistd.h>
#include <memory.h>
#include <sys/mman.h>
#include <cassert>
#include <wayland-client.h>
#include <Poco/Environment.h>

namespace Mmp
{

/**
 * @sa wl_registry_listener.global
 */
static void handle_register_global(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
{
    DisplayWayland* display = reinterpret_cast<DisplayWayland*>(data);

    if (std::string(interface) == std::string(wl_compositor_interface.name))
    {
        display->_compositor = reinterpret_cast<struct wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 1));
        assert(display->_compositor);
        DISPLAY_LOG_INFO << "-- wl_registry_bind, interface is: " << std::string(interface);
    }
    else if (std::string(interface) == std::string(wl_shell_interface.name))
    {
        display->_shell = reinterpret_cast<struct wl_shell*>(wl_registry_bind(registry, name, &wl_shell_interface, 1));
        assert(display->_shell);
        DISPLAY_LOG_INFO << "-- wl_registry_bind, interface is: " << std::string(interface);
    }
    else if (std::string(interface) == std::string(wl_shm_interface.name))
    {
        display->_shm = reinterpret_cast<struct wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
        DISPLAY_LOG_INFO << "-- wl_registry_bind, interface is: " << std::string(interface);
    }
}

/**
 * @sa wl_registry_listener.global_remove
 */
static void handle_register_glbal_remove(void* /* data */, struct wl_registry* /* registry */, uint32_t /* name */)
{
    // Hint : nothing to do for now
}

/**
 * @sa wl_shell_surface_listener.ping
 */
static void handle_surface_ping(void* /* data */, struct wl_shell_surface* shell_surface, uint32_t serial)
{
    // Hint : wayland 的心跳机制,必须定时发送 pong 响应让 wayland server 知道 wayland client 还存活
    wl_shell_surface_pong(shell_surface, serial);
}

static void handle_surface_configure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height)
{
    // Hint : 不支持调整窗口大小, Sample 模块不需要过于复杂
    assert(width == 1920 && height == 1080);
}

static struct wl_registry_listener registerListener = 
{
    .global = handle_register_global,
    .global_remove = handle_register_glbal_remove
};

static struct wl_shell_surface_listener surfaceListener = 
{
    .ping = handle_surface_ping,
    .configure = handle_surface_configure
};

} // namespace Mmp

namespace Mmp
{

DisplayWayland::DisplayWayland()
{
    _title       =  "MMP";
    _wlFormat    =  WL_SHM_FORMAT_ARGB8888;
    _width       =  0;
    _height      =  0;

    _display       =  nullptr;
    _register      =  nullptr;
    _compositor    =  nullptr;
    _shell         =  nullptr;
    _surface       =  nullptr;
    _shellSurface  =  nullptr;

    _isBufferInited  = false;
    _shmFd  = -1;
    _buffer = nullptr;
    _pool   = nullptr;
    _shm    = nullptr;
    _pixels = nullptr;
}

bool DisplayWayland::Init()
{
    DISPLAY_LOG_INFO << "DisplayWayland Init";

    _xdgRuntimeDir = Poco::Environment::get("XDG_RUNTIME_DIR");
    if (_xdgRuntimeDir.empty())
    {
        DISPLAY_LOG_ERROR << "Can not get environment XDG_RUNTIME_DIR";
        goto end;
    }
    else
    {
        DISPLAY_LOG_INFO << "Get XDG_RUNTIME_DIR environment, value is: " << _xdgRuntimeDir;
    }

    _display = wl_display_connect(NULL);
    if (!_display)
    {
        DISPLAY_LOG_ERROR << "wl_display_connect fail";
        goto end;
    }
    else
    {
        DISPLAY_LOG_INFO << "wl_display_connect success";
    }

    _register = wl_display_get_registry(_display);
    if (!_register)
    {
        DISPLAY_LOG_ERROR << "wl_display_get_registry fail";
        goto end1;
    }
    else
    {
        DISPLAY_LOG_INFO << "wl_display_get_registry success";
    }

    wl_registry_add_listener(_register, &registerListener, this);

    // Hint : 手动触发一次 wayland 调度,驱动注册的事件执行
    wl_display_dispatch(_display);
    wl_display_roundtrip(_display);

    return true;
end1:
    wl_display_disconnect(_display);
    _display = nullptr;
end:
    return false;
}

bool DisplayWayland::UnInit()
{
    return true;
}

bool DisplayWayland::Open(PixelsInfo info)
{
    assert(_compositor);
    assert(_shell);

    _width  = info.width;
    _height = info.height;

    if (info.format == PixelFormat::BGRA8888 || info.format == PixelFormat::RGBA8888)
    {
        _format = info.format;
    }
    else
    {
        DISPLAY_LOG_ERROR << "Unsupport pixel format, pixel format is: " << _format;
        assert(false);
        return false;
    }

    DISPLAY_LOG_INFO << "Try to open Wayland Display";

    _surface = wl_compositor_create_surface(_compositor);
    if (!_surface)
    {
        DISPLAY_LOG_ERROR << "wl_compositor_create_surface fail";
        goto end;
    }
    else
    {
        DISPLAY_LOG_INFO << "wl_compositor_create_surface success";
    }
    
    _shellSurface = wl_shell_get_shell_surface(_shell, _surface);
    if (!_shellSurface)
    {
        DISPLAY_LOG_ERROR << "wl_shell_get_shell_surface fail";
        goto end1;
    }
    else
    {
        DISPLAY_LOG_INFO << "wl_shell_get_shell_surface success";
    }

    UnInitBuffer();
    if (!InitBuffer())
    {
        DISPLAY_LOG_ERROR << "Init (shm) buffer fail";
        goto end1;
    }

    wl_shell_surface_add_listener(_shellSurface, &surfaceListener, nullptr);

    wl_shell_surface_set_toplevel(_shellSurface);
    wl_shell_surface_set_title(_shellSurface, _title.c_str());

    return true;
end1:
    wl_surface_destroy(_surface);
    _surface = nullptr;
end:
    return false;
}

bool DisplayWayland::Close()
{
    DISPLAY_LOG_INFO << "Try to close Wayland Display";

    if (_shm)
    {
        DISPLAY_LOG_INFO << "wl_shm_destroy";
        wl_shm_destroy(_shm);
        _shm = nullptr;
    }

    if (_shell)
    {
        DISPLAY_LOG_INFO << "wl_shell_destroy";
        wl_shell_destroy(_shell);
        _shell = nullptr;
    }

    if (_compositor)
    {
        DISPLAY_LOG_INFO << "wl_compositor_destroy";
        wl_compositor_destroy(_compositor);
        _compositor = nullptr;
    }

    return true;
}

void DisplayWayland::UpdateWindow(const uint32_t* frameBuffer, PixelsInfo info)
{
    assert((int32_t)_width == info.width && (int32_t)_height == info.height);

#if 0 /* WORKAROUND : 用于性能测试,减少 CPU 操作 */
    memcpy(_pixels, frameBuffer, _width*_height*4);
#else
    // Hint : wayland 只支持输出 ARGB8888 (小端) 格式
    if (info.format == PixelFormat::BGRA8888)
    {
        memcpy(_pixels, frameBuffer, _width*_height*4);
    }
    else if (info.format == PixelFormat::RGBA8888)
    {
        // WORKAROUND : 软件处理
        // Hint : 实际测算十分耗时, i7 10代 Debug 模式单线程下耗时达10ms以上;可能导致转换时间大于刷新帧间隔
        // TOOD : 考虑使用多线程并行加速,或者引入外部库处理
        uint32_t loop = _width*_height;
        DISPLAY_LOG_INFO << "Color Space Convert Begin";
        uint8_t* pixel = _pixels;
        uint32_t* _frameBuffer = const_cast<uint32_t*>(frameBuffer);
        for (uint32_t i=0; i<loop; i++)
        {
            pixel[0] /* B */  = (*_frameBuffer & 0xFF0000) >> 16;       /* B */
            pixel[1] /* G */  = (*_frameBuffer & 0xFF00) >> 8;          /* G */
            pixel[2] /* R */  = *_frameBuffer & 0xFF;                   /* R */
            pixel[3] /* A */  = (*_frameBuffer & 0xFF000000) >> 24;     /* A */
            // Hint : 使用指针偏移减少在 debug 模式下运算计算量
            pixel += 4;
            _frameBuffer++;
        }
        DISPLAY_LOG_INFO << "Color Space Convert End";
    }
    else
    {
        assert(false);
        return;
    }
#endif
    wl_surface_damage(_surface, 0, 0, _width, _height);
    wl_surface_attach(_surface, _buffer, 0, 0);
    wl_surface_commit(_surface);

    wl_display_dispatch(_display);
    wl_display_roundtrip(_display);

    return;
}

bool DisplayWayland::InitBuffer()
{
    if (_isBufferInited)
    {
        return true;
    }

    // FIXME : a better random path generation
    _shmFd = mkstemp(const_cast<char*>((_xdgRuntimeDir +  "/MMP-XXXXXX").c_str()));
    assert(_shmFd >= 0);

    int ret;
    do
    {
        ret = ftruncate(_shmFd, _width * _height * 4);
    }
    while ((ret < 0) && (errno == EINTR));

    _pixels = reinterpret_cast<uint8_t*>(mmap(NULL, _width * _height * 4, PROT_READ | PROT_WRITE, MAP_SHARED, _shmFd, 0));
    assert(_pixels);

    _pool = wl_shm_create_pool(_shm, _shmFd, _width * _height * 4);
    assert(_pool);

    _buffer = wl_shm_pool_create_buffer(_pool, 0, _width, _height, _width*4, _wlFormat);
    assert(_buffer);

    _isBufferInited = true;
    return true;
}

void DisplayWayland::UnInitBuffer()
{
    if (!_isBufferInited)
    {
        return;
    }
    if (_pixels)
    {
        munmap(_pixels, _width*_height*4);
        _pixels = nullptr;
    }
    if (_pool)
    {
        wl_shm_pool_destroy(_pool);
        _pool = nullptr;
    }
    if (_shmFd >= 0)
    {
        close(_shmFd);
        _shmFd = -1;
    }
    _isBufferInited = false;
}

} // namespace Mmp