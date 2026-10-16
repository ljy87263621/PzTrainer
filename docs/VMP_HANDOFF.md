# v1.0.1 Stable：VMProtect 交接

当前阶段：用户已确认 beta.9 修复有效，准备 v1.0.1 Stable 的 Marker DLL。尚未创建 Stable Release/tag，也未构建本版本 Launcher。

## 分支

- `main`：Stable 基线；只有明确要求 Stable 时才提交或提升到此分支。
- `beta`：默认开发和后续提交目标。临时 `codex/*` 分支从 beta 创建并回到 beta。
- GitHub 默认分支保持 main；本地完成本轮后切回 beta。
- 分支、版本准备与正式发布分别执行；本轮不发布 Release。

## 输入和输出

在仓库根目录运行（只构建 Trainer，静态 CRT `/MT`）：

```powershell
.\scripts\build-vmp-marker.ps1 -Version '1.0.1' -VmProtectRoot 'D:\Develope\VMProtect'
python .\scripts\verify-vmp-markers.py .\artifacts\pztrainer-v1.0.1-vmp\input\pztrainer.dll
```

校验工具在开发机使用系统 Python 的 `pefile`、`capstone`，不构成最终用户依赖。

- VMProtect 输入：`artifacts/pztrainer-v1.0.1-vmp/input/pztrainer.dll`。
- 请将处理结果另存为：`artifacts/pztrainer-v1.0.1-vmp/protected/pztrainer.dll`，保留原输入。
- `input-manifest.json` 记录源提交、输入 DLL/helper SHA-256 和构建阶段；不要把输入当成已保护成品。
- 若使用旧 `.vmp` 项目，重新指定本次输入并刷新 Marker 列表，不要沿用上一 DLL 的绝对地址或手选函数范围。
- 输入仍导入 `VMProtectSDK64.dll`，处理前不注入游戏、不作为便携成品发给用户。最终处理产物需要检查 SDK 导入已移除。

## 本轮 Marker 调整

总计 20 个具名 Trainer 区域，完整清单和编译模式见 `protection/markers.json`。

| 调整 | 标记 | 原因 |
|---|---|---|
| 新增 Mutation | NativeModelCaptureBindings | 原版 2.5D 模型接口绑定与帧钩子安装 |
| 新增 Mutation | ViewpointModelChamsBindings | ViewPoint 模型接口绑定与帧钩子安装 |
| 新增 Mutation | RageFireBindings | 实际开火接口绑定、目标游戏方法校验与安装 |
| 新增 Mutation | RecordedMediaBindings | VHS/CD helper 的类加载和绑定 |
| Ultra 改为 Mutation | HookBootstrap、LuaBridgeCache | 启动、文件缓存路径以兼容性和启动成本为优先；删除启动函数中多余的 End |
| 保留原模式 | 其余 14 个 JNI、物品、主线程桥接区域 | 保持既有覆盖范围；为尚未声明 noinline 的标记函数补充边界 |

新初始化区域的 ready/retry 检查在标记外；成功初始化后的每帧调用快速返回。新区域使用 Mutation，保留原生命令执行。ESP 遍历、GL hook、世界帧回调、JNI 对象身份查询、模型捕获/重放、Shader 和逐发目标处理均不扩大为虚拟化区域。Java helper 和 Shader 作为资源嵌入；C++ Marker 不会自动保护其中的 Java 字节码。

SDK 的 Begin/End 是供 VMProtect 分析的区域边界，Mutation/Ultra 预设由对应 API 指定，见 [VMProtect 官方标记说明](https://vmpsoft.com/index.php?newsid=21)。请先沿用上次可运行的保护选项并加载本次新 Marker；额外的入口保护、导入保护、压缩、反调试等选项需要在最终成品中单独验证，尤其是本项目 Launcher 使用手动映射。

## 收到处理结果后

本轮交接前已通过：完整 x64 Marker Trainer 构建（仅第三方 MinHook 既有警告）、20 个区域的源码和实际 PE 调用校验、静态 CRT 导入检查和内嵌 helper 字节校验；生产原版捕获与游戏 Shader 的身份／主角遮罩／三档视口测试，ViewPoint C++ JNI/JVMTI 帧钩子回归，实际开火 JVM 回归，VHS/CD 55 项 Java 检查与 C++ 目录回归。标记关闭下的行为回归和标记开启下的编译／结构检查均通过；保护后的运行验证需要用户完成 VMProtect 处理。

1. 核对输入 manifest、处理后的 PE 和 SDK/CRT 导入，保留原输入与保护输出的各自哈希。
2. 将处理后的 DLL 暂存到 Launcher 资源使用的位置；构建 Launcher 时必须使用 `/p:BuildProjectReferences=false /p:EnableVmProtectMarkers=false`，避免项目引用重编 Trainer 覆盖保护结果。
3. 核对 Launcher 内嵌 DLL 与处理结果逐字节一致，以及 Java/Lua 资源版本、便携目录和哈希清单。
4. 验证保护后的启动、原版和 ViewPoint 模型效果、自动视角切换及已确认功能，再按用户后续指示打包/发布。

普通 `build-portable-release.ps1` 会重编 Trainer，只用于未保护版本；现已拒绝 marker-enabled 打包，防止把依赖 SDK 的输入误当成发行包。
