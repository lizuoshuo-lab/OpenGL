# 实时极光体积渲染

本场景实现的是面向实时展示的物理启发近似，不是空间天气或粒子输运模拟。目标是在现有 OpenGL 延迟渲染管线中保留极光最重要的视觉结构：沿磁力线拉伸的薄帘、随高度变化的发射颜色、帘幕褶皱、细丝亮度和缓慢流动。

![4K volumetric aurora](assets/aurora_4k.png)

## 物理依据

太阳活动产生的高能带电粒子沿地球磁场进入高层大气，与氧、氮原子或分子碰撞并使其进入激发态；退激发释放的光形成极光。NASA 的概述给出了适合实时近似的高度与颜色关系：约 100-200 km 的原子氧以绿色为主，约 200 km 以上可出现红色氧发射，较低高度的氮发射会贡献蓝色和粉紫色边缘。[NASA: Auroras](https://science.nasa.gov/sun/auroras/)

Lawlor 与 Genetti 的 GPU 极光体绘制方法把发光分布拆为两个主要部分：地表投影平面上的电子通量足迹，以及随高度变化的能量沉积分布。二者相乘得到三维发光体，再沿观察光线积分。该分解是当前实现的主要算法依据。[Interactive Volume Rendering Aurora on the GPU, WSCG 2011](https://www.cs.uaf.edu/~olawlor/papers/2010/aurora/lawlor_aurora_2010.pdf)

## 近似模型

### 1. 薄帘通量场

四层极光帘幕由程序化曲线描述。每层曲线在水平面中使用多个不同频率的正弦褶皱，并随时间缓慢改变相位：

```text
z_i(x, t) = z0 - i * spacing
            + foldStrength * (sin(f1*x + p1*t)
            + 0.38*sin(f2*x - p2*t)
            + 0.18*turbulence*sin(f3*x + p3*t))
```

空间点到曲线的距离通过高斯核转为薄片密度。沿帘幕水平方向的一维平滑噪声负责细丝亮暗变化，因此亮纹在高度方向保持连续，而不是变成三维烟雾：

```text
Phi(x, z, t) = sum_i exp(-(distance_i / thickness_i)^2)
                     * filament_i(x, t)
                     * spanFade_i(x)
```

### 2. 高度沉积分层

高度归一化为 `h in [0, 1]`，绿、红、蓝三组发射使用不同中心和方差的高斯分布：

```text
D_green(h) = G(h; 0.31, 0.18) + 0.32 * G(h; 0.52, 0.25)
D_red(h)   = redWeight  * G(h; 0.83, 0.23)
D_blue(h)  = blueWeight * G(h; 0.055, 0.052)
```

最终局部发射近似为：

```text
L(x, h, z, t) = Phi(x, z, t) *
                (Cgreen*Dgreen(h) + Cred*Dred(h) + Cblue*Dblue(h))
```

这里的 RGB 颜色是对可见发射谱的显示近似，没有进行逐波长光谱积分。

### 3. 自适应视线积分

相机射线先与上下高度平面求交，只在有效高度区间采样。接近薄帘时缩短步长，空区域根据最近帘幕距离增大步长：

```text
C = integral T(s) * L(ray(s)) ds
T(s + ds) = T(s) * exp(-sigma * Phi(ray(s)) * ds)
```

极光在物理上接近光学薄介质，因此外部合成使用加法混合；积分内部只加入很弱的透射衰减，用于避免多层帘幕叠加后完全失去结构。

### 4. 低分辨率体积缓冲

体积积分写入四分之一宽高的 `RGBA16F` 缓冲。在 4K 输出下，极光缓冲为 `960 x 540`。随后线性上采样，并使用全分辨率 GBuffer 深度生成天空遮罩，将结果以加法方式合成回 HDR 颜色，再进入 Bloom 与 ACES。

```mermaid
flowchart LR
    RAY["Camera ray"] --> SLAB["Height slab intersection"]
    SLAB --> FIELD["Procedural curtain flux"]
    FIELD --> HEIGHT["Height emission profiles"]
    HEIGHT --> MARCH["Adaptive ray integration"]
    MARCH --> LOW["Quarter-resolution RGBA16F"]
    DEPTH["Full-resolution depth"] --> MASK["Sky visibility mask"]
    LOW --> MASK
    MASK --> HDR["Additive HDR composite"]
    HDR --> BLOOM["Bloom + ACES"]
```

这种处理将主要像素着色工作量降至全分辨率的约 `1/16`，同时由全分辨率深度维持山体轮廓。当前机器在 1600 x 900 控制面板截帧时显示约 `2.17 ms` 的显示帧时间；实际性能会随 GPU、分辨率和 Raymarch Steps 改变。

## ImGui 参数

- `Emission Intensity`：HDR 发光强度。
- `Raymarch Steps`：最大自适应采样预算，范围 16-96。
- `Motion Speed`：帘幕褶皱和细丝的时间变化速度。
- `Lower / Upper Height`：发光体积的高度边界。
- `Distance / Layer Spacing / Horizontal Span`：帘幕位置与覆盖范围。
- `Sheet Thickness / Fold Scale / Fold Strength / Turbulence`：薄帘形态。
- `Red Oxygen / Blue Nitrogen`：高层红光和低层蓝紫边缘的相对权重。
- `Green / Red / Blue Emission`：显示空间中的发射颜色。

![Aurora ImGui controls](assets/aurora_controls.png)

## 实现入口

- 材质与参数：[auroraMaterial.h](../glframework/material/auroraMaterial.h)
- 体积顶点着色器：[aurora.vert](../assets/shaders/aurora.vert)
- 通量场、沉积分层与视线积分：[aurora.frag](../assets/shaders/aurora.frag)
- 深度感知 HDR 合成：[auroraComposite.frag](../assets/shaders/deferred/auroraComposite.frag)
- 低分辨率缓冲和 Transparent Pass 接入：[renderPipeline.cpp](../glframework/renderer/renderPipeline.cpp)
- 夜景场景与 ImGui 控制：[main.cpp](../main.cpp)

## 科学模拟边界

当前实现没有求解磁层粒子轨迹、碰撞截面、真实大气密度、地磁经纬度、电子能谱或逐波长辐射传输；高度值也采用场景单位而非直接使用千米。它适合研究岗位作品集中的实时渲染展示和算法讲解，不应作为极光位置、强度或颜色的科学预测工具。

若继续提高物理真实性，可按优先级加入磁力线坐标系、由电子能谱驱动的高度沉积查找表、真实发射线到线性 RGB 的光谱转换、地球曲率与大气散射，以及时间相干的二维流体通量纹理。

## 参考与开源实现

1. [NASA, Auroras](https://science.nasa.gov/sun/auroras/)：极光形成机制及主要颜色和高度关系。
2. [Lawlor and Genetti, Interactive Volume Rendering Aurora on the GPU](https://www.cs.uaf.edu/~olawlor/papers/2010/aurora/lawlor_aurora_2010.pdf)：二维通量足迹、高度沉积、体积积分与空区域加速。
3. [Baranoski et al., Simulating the Aurora](https://onlinelibrary.wiley.com/doi/10.1002/vis.304)：较早的极光图形模拟工作与真实图像比较。
4. [AuroraRendererUnity](https://github.com/olawlor/AuroraRendererUnity)：Lawlor 公开的 Unity/HLSL 参考实现，仓库声明相关 Shader 与数据采用 Unlicense/Public Domain。当前项目依据论文原理独立实现 GLSL，没有复制其 Shader 源码。
