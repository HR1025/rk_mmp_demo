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
cmake .. -DUSE_ROCKCHIP=ON cmake .. -DUSE_ROCKCHIP=ON -DUSE_EGL=ON -DUSE_OPENGL=ON -DNOT_AUTO_EGL=ON -DCMAKE_BUILD_TYPE=Debug
make -j8
```

## 依赖说明

`Demo` 基于 `Rockchip` 的 `3588` 进行测试, 使用的是 `orangepi5plus`(香橙派5)；
不过大差不差, `MMP-Core` 最初设计的初衷就是想 `run anywhere`，故能运行在 `Linux` 裸系统上, 更不用说其发型版本或者其他定制系统;

对于这个 `Demo` 来说, 一共需要两个系统驱动的支持:

- 编解码驱动, 这个由 `Rockchip` 提供; 一般系统上会自带, 没有的话可以参考 [mpp](https://github.com/rockchip-linux/mpp) 编译安装
- GPU驱动 (OpenGL), 这个一般由 `ARM` 提供

针对于 GPU 驱动需要检查其是否是正确的, 一般在 `/usr/lib` 或者 `/lib` 目录用 `find -name "*mali*"` 找一找, 可以找到一些东西, 比如像下面这样的:

```shell
./firmware/mali_csffw.bin
./aarch64-linux-gnu/pkgconfig/mali.pc
./aarch64-linux-gnu/libmali_hook_injector.a
./aarch64-linux-gnu/libmali_hook.so
./aarch64-linux-gnu/libmali.so
./aarch64-linux-gnu/libmali-valhall-g610-g6p0-x11-gbm.so
./aarch64-linux-gnu/libmali.so.1.9.0
./aarch64-linux-gnu/libmali_hook.so.1
./aarch64-linux-gnu/libmali_hook.so.1.9.0
./aarch64-linux-gnu/perl/5.34.0/Unicode/Normalize.pm
./aarch64-linux-gnu/perl/5.34.0/auto/Unicode/Normalize
./aarch64-linux-gnu/perl/5.34.0/auto/Unicode/Normalize/Normalize.so
./aarch64-linux-gnu/libmali.so.1
./aarch64-linux-gnu/mali
./aarch64-linux-gnu/dri/mali-dp_dri.so
./ruby/3.0.0/unicode_normalize
./ruby/3.0.0/unicode_normalize/normalize.rb
```

其中 `./aarch64-linux-gnu/libmali-valhall-g610-g6p0-x11-gbm.so` 就是 GPU 驱动了, 不然可能用的是由 `MESA` 提供的 `GPU` 软实现; (`MALI` 是 `ARM` GPU 的一种架构实现, 有兴趣可以百度一下)
然后修改 `CMakeLists.txt` 的内容, 具体见 `CMakeLists.txt`.


## 示例

- test_decoder : 解码示例
- test_encoder : 编码示例
- test_transcode : 转码示例
- test_compositor : 四分屏合成画面示例

> -help 查看具体使用

## 代办

- 补充 compositor 示例, See `MMP-Core/GPU/PG/AbstractSceneLayer.h`
- 补充 RGA 使用
- 补充 V4L2 和 DRM 的使用示例, 对应 VI 和 VO 两个部分

> 后两点如何与 `MMP-Core` 进行融合目前好没有想过, 不过可能 V4L2 和 DRM 近期内可能会在 `MMP-Core` 补充,
> 其层级比较好描述, V4L2 和 DRM 对于 Linux 系统之上的层级应当是 X11 或者 Wayland, 而在 Windows 上则可能是 DXGI,
> 或者使用更高维度的图形库如 `SDL` 等等
