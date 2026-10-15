# 自动投影与俯视角模型捕获修复计划

**Goal:** 修复俯视角模型捕获的空 playerData；按当前渲染器自动选择 ESP/瞄准投影；移除冗余 VHS/CD 页面。

**Architecture:** 保留原版显式模型提交与 ViewPoint 帧结束前回调两条路径。原版使用引擎 updateLights → initModel → init 生命周期。投影策略以 View.enabled 和有效相机为准，保留旧配置结构布局但不再使用其中的手动开关。

**Tech Stack:** C++17 / JNI / JVMTI、Java 17 字节码、ImGui 自绘组件、Kahlua Lua、MSVC 静态运行库。

## 约束

- 当前会话独立完成；不改变已经正常的第一人称回调位置、静默自瞄或自动急停行为。
- 保留普通 Spawner 的具名 VHS/CD 目录和精确节目生成。
- 相机切换到 3D 但尚未就绪时暂停投影，避免误用俯视坐标。
- 维持配置二进制布局，旧手动值不得阻止自动切换。

## 实施与验证

- [x] 原版捕获回归：实际游戏类的 model.playerData=null 起点，prepare 必须完成初始化并正确释放引用；覆盖每玩家数据和失败清理。先运行旧实现观察失败，再在 prepare 中调用 `slot.model.updateLights()`，运行 `scripts/test-model-colour.ps1`。
- [x] 自动投影回归：在 `viewpoint_bridge_test.cpp` 覆盖缺失 Mod、安装但关闭、开启待相机、有效 3D、返回俯视及旧配置 false；在 Lua 用相同切换序列验证。策略 `UseViewpointEsp = CanUseViewpoint3D`，`ShouldDrawEsp = !status.mod_enabled || CanUseViewpoint3D`；桥不受旧兼容开关影响。运行 `scripts/test-viewpoint.cmd` 和 `scripts/test-item-esp.ps1`。
- [x] 移除 `trainer_menu.cpp`、`item_generator.cpp/.hpp` 的独立页面和媒体专属过滤器，正常搜索保留具名媒体；删除失去用途的页面测试，保留实际媒体生成及普通生成器布局回归。运行 `scripts/test-recorded-media.ps1` 与 `trainer/tests/run_item_quantity_layout_test.ps1`。
- [x] 移除 ESP、瞄准页和 Lua 物资 ESP 的手动投影按钮，以及特性注册中的旧可写开关；保留状态文本。核对所有投影消费端及配置迁移。
- [x] 更新文档与 beta.8 版本；运行相关配置、着色 GPU、静默自瞄回归，Release 构建 DLL 与便携启动器，核对嵌入 JAR/DLL 与分发文件。
- 检查 diff、提交源码、按已有授权上传 GitHub 源码与便携预发行包；验证远端提交、标签、文件摘要。明确实机仍待用户复测。（构建与校验后执行；远端结果以 Release 为准。）
