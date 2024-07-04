#include "DisplaySDL.h"

#include <cassert>
#include <memory.h>

#include <SDL2/SDL.h>

namespace Mmp
{

DisplaySDL::DisplaySDL()
{
    _displayWidth   = 0;
    _displayHeight  = 0;
    _displayRefresh = 0;
    _windowWidth    = 0;
    _windowHeight   = 0;
    _window         = nullptr;
    _render         = nullptr;
    _texture        = nullptr;
    _title          = "MMP";
    _selfInit       = true;
}

bool DisplaySDL::Init()
{
    /**
     * @brief 检查 SDL 的编译时头文件和运行时库是否匹配
     */
    auto checkSDLVersionIsSame = []() -> bool
    {
        SDL_version compiled;
        SDL_version linked;
        SDL_VERSION(&compiled);
        SDL_GetVersion(&linked);
        DISPLAY_LOG_INFO << "SDL version when complie is: (" << (int)compiled.major << "." << (int)compiled.minor << "." << (int)compiled.patch << ")";
        DISPLAY_LOG_INFO << "SDL version when link is: (" << (int)linked.major << "." << (int)linked.minor << "." << (int)linked.patch << ")";
        return (compiled.major == linked.major && compiled.minor == linked.minor && compiled.patch == linked.patch);
    };
    
    /**
     * @brief 获取主屏幕相关信息
     */
    auto getMajorDisplayInfo = [this]() -> bool
    {
        SDL_DisplayMode displayMode;
        if (SDL_GetCurrentDisplayMode(0, &displayMode) != 0)
        {
            return false;
        }
        _displayWidth   = displayMode.w;
        _displayHeight  = displayMode.h;
        _displayRefresh = displayMode.refresh_rate;
        DISPLAY_LOG_INFO << "Major Display info : (" << _displayWidth << "," << _displayHeight << ")x" << _displayRefresh;
        return true;
    };

    DISPLAY_LOG_INFO << "SDL Display Init";

    if (!checkSDLVersionIsSame())
    {
        DISPLAY_LOG_ERROR << "SDL version is not same when runing as when compiling";
        return false;
    }

    if (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)
    {
        _selfInit = false;
    }
    else if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        DISPLAY_LOG_ERROR << "Failed to initialize SDL, error is: " << SDL_GetError();
        return false;
    }
    else
    {
        DISPLAY_LOG_INFO << "Initialize SDL with SDL_INIT_VIDEO flag successfully";
    }
    
    if (!getMajorDisplayInfo())
    {
        return false;
    }

    return true;
}

bool DisplaySDL::UnInit()
{
    DISPLAY_LOG_INFO << "SDL Display Uninit";
    if (_selfInit)
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
    return true;
}

bool DisplaySDL::Open(PixelsInfo info)
{
    if (_window != nullptr)
    {
        DISPLAY_LOG_WARN << "Window is already opened";
        return false; // Hint : 返回 false 提示调用者,非笔误 
    }
    DISPLAY_LOG_INFO << "Try to open SDL window";
    std::string windowTitle = _title.empty() ? "SDL Window" : _title;
    _windowWidth = info.width;
    _windowHeight = info.height;
    uint32_t format = 0;
    if (info.format == PixelFormat::BGRA8888)
    {
        format = SDL_PIXELFORMAT_ARGB8888;
    }
    else if (info.format == PixelFormat::RGBA8888)
    {
        format = SDL_PIXELFORMAT_ABGR8888;
    }
    else if (info.format == PixelFormat::NV12)
    {
        format = SDL_PIXELFORMAT_NV12;
    }
    else
    {
        DISPLAY_LOG_ERROR << "Unsupport pixel format, pixel format is: " << _format;
        assert(false);
        return false;
    }
    _format = info.format;
    _window = SDL_CreateWindow(windowTitle.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, _windowWidth, _windowHeight, 0 /* NO FLAG, mean software */);
    if (_window != nullptr)
    {
        DISPLAY_LOG_INFO << "Create SDL window successfully, window title is: " << windowTitle;
    }
    else
    {
        DISPLAY_LOG_ERROR << "Create SDL window fail, error is: " << SDL_GetError();
        return false;
    }
    _render = SDL_CreateRenderer(_window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
    if (_render != nullptr)
    {
        DISPLAY_LOG_INFO << "Create SDL render Successfully";
    }
    else
    {
        DISPLAY_LOG_ERROR << "Create SDL render fail, error is: " << SDL_GetError();
        return false;
    }
    _texture = SDL_CreateTexture(_render, format, SDL_TEXTUREACCESS_STREAMING, _windowWidth, _windowHeight);
    if (_texture != nullptr)
    {
        DISPLAY_LOG_INFO << "Create SDL texture successfully";
    }
    else
    {
        DISPLAY_LOG_ERROR << "Create SDL texture fail, error is: " << SDL_GetError();
        return false;
    }
    SDL_SetTextureBlendMode(_texture, SDL_BLENDMODE_BLEND); // 支持透明度

    DISPLAY_LOG_INFO << "Open SDL window successfully";

    return true;
}

bool DisplaySDL::Close()
{
    DISPLAY_LOG_INFO << "Try to close SDL window";
    if (_texture)
    {
        SDL_DestroyTexture(_texture);
        _texture = nullptr;
        DISPLAY_LOG_INFO << "Destory SDL texture";
    }
    if (_render)
    {
        SDL_DestroyRenderer(_render);
        _render = nullptr;
        DISPLAY_LOG_INFO << "Destory SDL render";
    }
    if (_window)
    {
        SDL_DestroyWindow(_window);
        _window = nullptr;
        DISPLAY_LOG_INFO << "Destory SDL window";
    }
    DISPLAY_LOG_INFO << "Close SDL window successfully";
    return true;
}

void DisplaySDL::UpdateWindow(const uint32_t* frameBuffer, PixelsInfo info)
{
    switch (info.format)
    {
        case PixelFormat::RGBA8888:
        case PixelFormat::BGRA8888:
        {
            SDL_UpdateTexture(_texture, NULL, reinterpret_cast<const void*>(frameBuffer), sizeof(uint32_t)*_windowWidth);
            break;
        }
        case PixelFormat::NV12:
        {
            SDL_Rect rect;
            {
                rect.x = 0;
                rect.y = 0;
                rect.w = info.width;
                rect.h = info.height;
            }
            SDL_UpdateNVTexture(_texture, &rect, (uint8_t*)frameBuffer, info.width, (uint8_t*)frameBuffer + info.width * info.height, info.width);
            break;
        }
        default:
        {
            return;
        }
    }
    SDL_RenderCopy(_render, _texture, NULL, NULL);
    SDL_RenderPresent(_render);
}

} // namespace Mmp