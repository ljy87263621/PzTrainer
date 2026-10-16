# v1.0.1 Hotfix1：VMProtect 交接

当前阶段：将 v1.0.1 beta1 提升为 v1.0.1 Hotfix1 Stable。Trainer、helper 与 Lua 没有变化，复用用户已完成 VMP 处理的 beta1 DLL，仅构建更新版本名和 Stable 标识的 Launcher。本轮不创建 Release/tag，保留旧 Stable/Beta 成品。

复用产物：`artifacts/pztrainer-v1.0.1-beta.1-vmp/protected/pztrainer.dll`，SHA-256：`7ec823eaa7d39f1f9a2bfe6b06fe1e4eee1fcdeef7f589118ea6dd5a273354b6`。该 DLL 已通过 Windows 加载、生产手动映射以及两份 helper 资源一致性检查。Hotfix1 的打包记录须同时注明 beta1 Marker 输入和 Hotfix1 Launcher 的源提交。

## 分支

- `main`：Stable 基线；只有明确要求 Stable 时才提交或提升到此分支。
- `beta`：默认开发和后续提交目标。临时 `codex/*` 分支从 beta 创建并回到 beta。
- GitHub 默认分支保持 main；本地完成本轮后切回 beta。
- 分支、版本准备与正式发布分别执行；本轮不发布 Release。

## 输入和输出

以下命令仅供以后重建 Trainer（需要重新 VMP），本次版本提升不执行。只构建 Trainer，静态 CRT `/MT`：

```powershell
.\scripts\build-vmp-marker.ps1 -Version '1.0.1-hotfix.1' -DisplayName 'v1.0.1 Hotfix1' -VmProtectRoot 'D:\Develope\VMProtect'
python .\scripts\verify-vmp-markers.py .\artifacts\pztrainer-v1.0.1-hotfix.1-vmp\input\pztrainer.dll
```

校验工具在开发机使用系统 Python 的 `pefile`、`capstone`，不构成最终用户依赖。

- VMProtect 输入：`artifacts/pztrainer-v1.0.1-hotfix.1-vmp/input/pztrainer.dll`。
- 请将处理结果另存为：`artifacts/pztrainer-v1.0.1-hotfix.1-vmp/protected/pztrainer.dll`，保留原输入。
- `input-manifest.json` 记录源提交、输入 DLL/helper SHA-256 和构建阶段；不要把输入当成已保护成品。
- 若使用旧 `.vmp` 项目，重新指定本次输入并刷新 Marker 列表，不要沿用上一 DLL 的绝对地址或手选函数范围。
- 输入仍导入 `VMProtectSDK64.dll`，处理前不注入游戏、不作为便携成品发给用户。最终处理产物需要检查 SDK 导入已移除。

## 继承 Stable 的 Marker 范围

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

本版沿用 Stable 的 20 个 Marker 区域，物品桥接初始化与生成调度继续受原有标记覆盖。验证器现在同时核对 Lua bridge 和 player helper 两份内嵌 JAR；新增的准备物品 helper 位于 player JAR，媒体／灯具代码位于 Lua bridge JAR。具体路由与验证范围见 [PREPARED_ITEM_ROUTES.md](PREPARED_ITEM_ROUTES.md)。beta1 的用户保护产物已通过隔离加载和资源验证；若重新构建 Trainer，则必须重新完成 VMProtect 处理及验证。

1. 核对输入 manifest、处理后的 PE 和 SDK/CRT 导入，保留原输入与保护输出的各自哈希。
2. 将处理后的 DLL 暂存到 Launcher 资源使用的位置；构建 Launcher 时必须使用 `/p:BuildProjectReferences=false /p:EnableVmProtectMarkers=false`，避免项目引用重编 Trainer 覆盖保护结果。
3. 核对 Launcher 内嵌 DLL 与处理结果逐字节一致，以及 Java/Lua 资源版本、便携目录和哈希清单。
4. 验证保护后的启动、原版和 ViewPoint 模型效果、自动视角切换及已确认功能，再按用户后续指示打包/发布。

普通 `build-portable-release.ps1` 会重编 Trainer，只用于未保护版本；现已拒绝 marker-enabled 打包，防止把依赖 SDK 的输入误当成发行包。
