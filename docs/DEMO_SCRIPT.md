# 30 秒 4K 演示脚本

在线播放：[30 秒 1080p 预览](https://raw.githubusercontent.com/lizuoshuo-lab/OpenGL/main/docs/assets/opengl-ibl-preview-1080p.mp4)

4K 成片：[下载原片](https://github.com/lizuoshuo-lab/OpenGL/raw/refs/heads/main/docs/assets/opengl-ibl-showcase-30s.mp4)

## 分镜

| 时间 | 画面 | 展示重点 |
| --- | --- | --- |
| 0-8 秒 | A Beautiful Game 棋盘与棋子 | 棋子落在棋盘上的软阴影、斜上方观察视角、多材质 glTF 渲染 |
| 8-15 秒 | Flight Helmet | 6 套源材质、金属与非金属表面、法线和粗糙度细节 |
| 15-22 秒 | Toy Car | 3 套源材质、车漆高光、布料底座与环境反射 |
| 22-30 秒 | 5 x 5 BRDF 材质矩阵 | Metallic 与 Roughness 的连续变化及 IBL 高光响应 |

## 口播建议

“这是一个使用 C++17 和 OpenGL 4.6 实现的 IBL/PBR 实时展示程序。启动阶段由 GPU 从 EXR 环境图生成环境 Cubemap、Irradiance、GGX Prefilter 和 BRDF LUT。glTF 模型保留 Base Color、Normal、Metallic-Roughness 与 AO 材质数据。棋盘场景增加了 2048 平方 Shadow Map 和 16 采样 Poisson PCF，让棋子的阴影只落在棋盘表面。场景、材质、光照、阴影、调试视图、相机和输出均可通过 ImGui 实时控制。”

## 录制方式

- 使用当前 Debug 可执行程序的真实无边框全屏输出，不使用离线替代画面。
- 录制区域为 OpenGL 窗口物理客户区，不包含桌面、任务栏或标题栏。
- 使用 FFmpeg Desktop Duplication 捕获 D3D11 帧，并由 NVENC 编码为 4K H.264。
- 四段实机镜头按上述时长无损拼接，保持原始 4K H.264 视频流。
- 程序可通过 `--fullscreen --demo` 启动自动演示，`F10` 控制演示循环，`F11` 切换全屏。
- 可运行 `tools/record_showcase_4k.ps1` 自动复录并校验成片。

## 成片核验

- 时长：30.000 秒
- 分辨率：3840 x 2160
- 编码：H.264，YUV 4:2:0
- 帧率：30 FPS
- 总帧数：900
- 已在第 3、11、18、26 秒抽帧检查四个场景，画面均覆盖完整物理客户区。
