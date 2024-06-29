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