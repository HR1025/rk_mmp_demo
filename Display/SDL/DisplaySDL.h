//
// DisplaySDL.h
//
// Library: Common
// Package: Display
// Module:  SDL
// 

#pragma once

#include <cstdint>
#include <string>

#include "AbstractDisplay.h"

// forward declare
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace Mmp
{

/**
 * @brief  SDL 窗口创建器
 */
class DisplaySDL : public AbstractDisplay
{
public:
    DisplaySDL();
public:
    bool Init() override;
    bool UnInit() override;
    bool Open(PixelsInfo info) override;
    bool Close() override;
    void UpdateWindow(const uint32_t* frameBuffer, PixelsInfo info) override;
private:
    uint32_t     _displayWidth;
    uint32_t     _displayHeight;
    uint32_t     _displayRefresh;
    uint32_t     _windowWidth;
    uint32_t     _windowHeight;
    PixelFormat  _format;
    std::string  _title;
private:
    SDL_Window*      _window;
    SDL_Renderer*    _render;       // render bind to window
    SDL_Texture*     _texture;      // texture bind to render, 目前格式固定为 ABGR8888 
    uint32_t*        _frameBuffer;  // _frameBuffer "bind" to texture (格式固定为 ABGR8888)
    bool             _selfInit;
};

} // namespace Mmp