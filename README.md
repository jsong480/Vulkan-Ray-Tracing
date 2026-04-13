<a id="readme-top"></a>

# Vulkan Real-Time Path Tracer

<p align="center">
  <a href="#english"><strong>English</strong></a>
  &nbsp;·&nbsp;
  <a href="#chinese"><strong>中文</strong></a>
  &nbsp;·&nbsp;
  <a href="#readme-top"><strong>Top</strong></a>
</p>

---

<h2 id="english">English</h2>

<p align="right"><a href="#readme-top">↑ Top</a></p>

Interactive Cornell-box style scene using hardware ray tracing: progressive path tracing, glass materials, wavelet denoising, and a live ImGui control panel.

### Demo

**Video:** [Watch on YouTube](https://youtu.be/m0-LrBHhzvk)

### Features

- Vulkan ray tracing pipeline, BLAS/TLAS, shader binding table
- Progressive path tracing: multi-bounce, next-event estimation on an area light, Russian roulette
- Glass: Fresnel reflection, Snell refraction, total internal reflection
- A-Trous wavelet denoiser with G-buffer guidance; tone mapping in the final pass
- OBJ loading (Stanford Bunny), multi-instance TLAS
- ImGui panel: denoiser, light intensity, IOR, max bounces, bunny rotation

### Requirements

- **OS:** Windows 10/11 (primary target; Linux may need adjustments)
- **GPU:** Vulkan 1.3 with ray tracing extensions
- **Build:** CMake ≥ 3.20, C++17 compiler, **Vulkan SDK** (includes `glslc`)

Dependencies are fetched automatically via CMake: GLFW, GLM, VMA, tinyobjloader, Dear ImGui.

### Assets

Place **`bunny.obj`** (Stanford Bunny) at **`assets/bunny.obj`** before running. The build copies the `assets` folder next to the executable.

### Build

```bash
cmake -B build -S .
cmake --build build --config Release
```

On Windows, the executable is usually `build/Release/VulkanRayTracing.exe`. Shaders are compiled with `glslc` and copied beside the binary.

### Controls

- **Right mouse button (hold):** look around + WASD movement
- **Release RMB:** free cursor for ImGui
- **ESC:** quit

Use the **Ray Tracing Controls** window to tune parameters.

### License

This project’s source code is provided as-is for portfolio and learning. Third-party libraries (GLFW, GLM, VMA, tinyobjloader, Dear ImGui, Stanford Bunny model) remain under their respective licenses.

<p align="right"><a href="#readme-top">↑ Top</a> · <a href="#chinese">中文 →</a></p>

---

<h2 id="chinese">中文</h2>

<p align="right"><a href="#readme-top">↑ Top</a></p>

基于 Vulkan 硬件光线追踪的交互式 Cornell Box 风格场景：渐进式路径追踪、玻璃材质、小波降噪，以及 ImGui 实时参数面板。

### 演示视频

**视频：** [YouTube 观看](https://youtu.be/m0-LrBHhzvk)

### 功能特性

- Vulkan 光线追踪管线、底层/顶层加速结构、着色器绑定表
- 渐进路径追踪：多反弹、面光源上的 Next Event Estimation、俄罗斯轮盘
- 玻璃材质：菲涅尔反射、斯涅尔折射、全内反射
- 基于 G-buffer 引导的 A-Trous 小波降噪；末通道色调映射
- OBJ 模型加载（Stanford Bunny）、多实例 TLAS
- ImGui 面板：降噪、光源强度、折射率、最大反弹次数、兔子旋转

### 系统要求

- **系统：** Windows 10/11（主要目标；Linux 需自行适配）
- **显卡：** 支持 Vulkan 1.3 及光线追踪扩展
- **构建：** CMake ≥ 3.20、C++17 编译器、**Vulkan SDK**（含 `glslc`）

依赖通过 CMake 自动拉取：GLFW、GLM、VMA、tinyobjloader、Dear ImGui。

### 资源文件

运行前请将 Stanford Bunny 模型保存为 **`assets/bunny.obj`**。构建会把 `assets` 目录复制到可执行文件旁。

### 构建

```bash
cmake -B build -S .
cmake --build build --config Release
```

在 Windows 上，可执行文件一般为 `build/Release/VulkanRayTracing.exe`。着色器由 `glslc` 编译并复制到程序旁。

### 操作说明

- **按住右键：** 视角移动 + WASD 行走
- **松开右键：** 释放鼠标以操作 ImGui
- **ESC：** 退出

在 **Ray Tracing Controls** 窗口中调节渲染参数。

### 许可证

本项目源码按「原样」提供，用于学习与作品集展示。第三方库（GLFW、GLM、VMA、tinyobjloader、Dear ImGui、Stanford Bunny 模型等）遵循各自许可证。

<p align="right"><a href="#readme-top">↑ Top</a> · <a href="#english">→ English</a></p>
