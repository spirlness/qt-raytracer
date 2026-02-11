# 代码审查报告（2026-02-11）

## 范围
- 渲染后端：`src/backends/vulkan/VulkanPathTracer.cpp`
- CUDA 后端：`src/backends/CudaPathTracerKernel.cu`
- 构建可用性检查：`cmake -S . -B build && cmake --build build -j4`

## 结论概览
本次审查发现 3 个值得优先处理的问题：
1. Vulkan 资源创建阶段存在多处未检查返回值，失败时可能继续使用空句柄。
2. Vulkan 命令录制/提交路径也存在关键 API 返回值未检查，错误传播不完整。
3. CUDA 初始化失败路径存在设备内存泄漏风险（第二次分配失败时未回收第一次分配）。

---

## 详细问题

### 1) [高] Vulkan 多处创建 API 未检查返回值
**位置**：`createImagesAndBuffers()` 中以下调用没有判断 `VkResult`：
- `vkCreateCommandPool`
- `vkAllocateCommandBuffers`
- `vkCreateFence`
- `vkCreateBuffer`
- `vkBindBufferMemory`
- `vkMapMemory`

**风险**：
当驱动内存紧张、设备丢失或句柄上限触达时，这些函数可能失败。当前实现会继续执行并在后续访问无效句柄（例如 `commandBuffer` / `stagingMapped`），导致崩溃或未定义行为。

**建议**：
- 为每个 Vulkan 调用统一做返回值检查。
- 失败时设置 `m_lastError`，并立即 `return false`。
- 可引入一个小工具函数/宏减少样板代码（例如 `VK_CHECK(call, "msg")`）。

### 2) [中] Vulkan 渲染提交路径缺少错误检查
**位置**：`renderFrame()` 与 `recordAndSubmitInitClear()` 中以下调用无返回值检查：
- `vkBeginCommandBuffer` / `vkEndCommandBuffer`
- `vkQueueSubmit`
- `vkWaitForFences`

**风险**：
若命令缓冲区状态异常、队列提交失败或等待超时/设备异常，当前实现仍继续 `memcpy` 并递增 frame index，可能把错误帧当作成功帧展示，且不利于定位故障。

**建议**：
- 检查上述 API 返回值并转换为可诊断的 `m_lastError`。
- 在失败分支避免更新帧计数，避免“伪进度”。

### 3) [中] CUDA 初始化失败分支内存泄漏
**位置**：`cudaPathTracerInit()`

**现象**：
- 第一次 `cudaMalloc(dAccum)` 成功后，若第二次 `cudaMalloc(dOutput)` 失败，函数直接返回 `false`。
- 该路径未释放已成功分配的 `dAccum`。

**风险**：
在重试初始化或切换后端时，可能出现显存泄漏累积，增加后续初始化失败概率。

**建议**：
- 在任一后续分配失败时，清理此前已分配的资源再返回。
- 可使用“单出口 + cleanup 标签”或小型 RAII 封装，确保失败路径一致回收。

---

## 验证记录
- 运行构建命令后，当前环境缺少 Qt6 开发包，无法完成完整构建与单测。

## 复测与环境阻塞说明（追加）
- 已尝试安装 Qt6 并继续编译/测试，但当前环境的代理对外部仓库请求均返回 `403 Forbidden`，导致无法通过 `apt` 拉取 Qt6 包。
- 在无法安装 Qt6 的前提下，改为尝试仅构建测试（`-DBUILD_APP=OFF`），但 `FetchContent` 下载 `googletest` 同样被代理拦截（403），因此测试也无法完成。
- 建议：
  1. 在可访问的内网镜像中提供 Ubuntu/Qt6 包源。
  2. 为 `googletest` 提供本地镜像或改为子模块/仓库内 vendoring，避免构建期外网依赖。
