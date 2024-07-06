# README

## 简介

`rk_mmp_demo` 是针对 `Rockchip` 平台编写的 `Demo` 示例, 基于 `MMP-Core` 实现；
用于验证测试以下场景:
- GPU 使用 (以 `MALI` 的 `OpenGL ES` 驱动)
- VENC 、VDEC 使用 (以 `Rockchip` 的 `mpp` 驱动实现)

## 编译

```shell
mkdir build
cd  build
cmake .. -DUSE_ROCKCHIP=ON
make -j8
```

## 示例

- test_decoder : 解码示例
- test_encoder : 编码示例
- test_transcode : 转码示例

> -help 查看具体使用

## 代办

- 补充 compositor 示例, See `MMP-Core/GPU/PG/AbstractSceneLayer.h`
- 补充 RGA 使用
- 补充 V4L2 和 DRM 的使用示例, 对应 VI 和 VO 两个部分

> 后两点如何与 `MMP-Core` 进行融合目前好没有想过, 不过可能 V4L2 和 DRM 近期内可能会在 `MMP-Core` 补充,
> 其层级比较好描述, V4L2 和 DRM 对于 Linux 系统之上的层级应当是 X11 或者 Wayland, 而在 Windows 上则可能是 DXGI,
> 或者使用更高维度的图形库如 `SDL` 等等
