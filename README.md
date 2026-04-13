# Vulkan Real-Time Path Tracer

**EN:** Interactive Cornell-box style scene with hardware ray tracing, progressive path tracing, glass materials, wavelet denoising, and a live ImGui control panel.

**中文：** 基于 Vulkan 硬件光追的交互式 Cornell Box 风格场景，包含渐进式路径追踪、玻璃材质、小波降噪与 ImGui 实时参数面板。

---

## Features / 功能特性

| EN | 中文 |
|----|------|
| Vulkan ray tracing pipeline, BLAS/TLAS, shader binding table | Vulkan 光线追踪管线、底层/顶层加速结构、着色器绑定表 |
| Progressive path tracing: multi-bounce, NEE on area light, Russian roulette | 渐进路径追踪：多反弹、面光源 NEE、俄罗斯轮盘 |
| Physically motivated glass: Fresnel + refraction, total internal reflection | 物理向玻璃：菲涅尔 + 折射、全内反射 |
| A-Trous wavelet denoiser with G-buffer guidance, tone mapping in final pass | 基于 G-buffer 引导的 A-Trous 小波降噪，末通道色调映射 |
| OBJ loading (Stanford Bunny), multi-instance TLAS | OBJ 加载（Stanford Bunny）、多实例 TLAS |
| ImGui: denoiser, light intensity, IOR, max bounces, bunny rotation | ImGui：降噪、光源强度、折射率、最大反弹次数、兔子旋转 |

---

## Requirements / 系统要求

| EN | 中文 |
|----|------|
| **OS:** Windows 10/11 (primary target; Linux may work with adjustments) | **系统：** Windows 10/11（主要目标；Linux 需自行适配） |
| **GPU:** Vulkan 1.3 with ray tracing extensions | **显卡：** 支持 Vulkan 1.3 及光线追踪扩展 |
| **Build:** CMake ≥ 3.20, C++17 compiler, **Vulkan SDK** (includes `glslc`) | **构建：** CMake ≥ 3.20、C++17 编译器、**Vulkan SDK**（含 `glslc`） |

Dependencies are fetched automatically via CMake: GLFW, GLM, VMA, tinyobjloader, Dear ImGui.

依赖通过 CMake 自动拉取：GLFW、GLM、VMA、tinyobjloader、Dear ImGui。

---

## Assets / 资源文件

Place **`bunny.obj`** (Stanford Bunny) under **`assets/bunny.obj`** before running. The build copies the `assets` folder next to the executable.

运行前请将 **`bunny.obj`**（Stanford Bunny）放在 **`assets/bunny.obj`**。构建会把 `assets` 目录复制到可执行文件旁。

---

## Build / 构建

```bash
cmake -B build -S .
cmake --build build --config Release
```

On Windows, the executable is typically at `build/Release/VulkanRayTracing.exe`. Shaders are compiled with `glslc` and copied beside the binary.

在 Windows 上，可执行文件一般在 `build/Release/VulkanRayTracing.exe`。着色器由 `glslc` 编译并复制到程序旁。

---

## Controls / 操作说明

| EN | 中文 |
|----|------|
| **Right mouse button (hold):** camera look + WASD movement | **按住右键：** 视角移动 + WASD 行走 |
| **Release RMB:** free cursor for ImGui | **松开右键：** 释放鼠标以操作 ImGui |
| **ESC:** quit | **ESC：** 退出 |

Use the **Ray Tracing Controls** window to tune rendering parameters.

在 **Ray Tracing Controls** 窗口中调节渲染参数。

---

## License / 许可证

This project’s source code is provided as-is for portfolio and learning. Third-party libraries (GLFW, GLM, VMA, tinyobjloader, Dear ImGui, Stanford Bunny model) remain under their respective licenses.

本项目源码按「原样」提供，用于学习与作品集展示。第三方库（GLFW、GLM、VMA、tinyobjloader、Dear ImGui、Stanford Bunny 模型等）遵循各自许可证。
