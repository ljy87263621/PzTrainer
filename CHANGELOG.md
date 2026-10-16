# v1.0.1 beta1 — Marker DLL 准备

- 修正新增家具／具名 VHS/CD 的目录与执行能力标记：家具支持原生、尸体、灯具、媒体路径；录制节目支持原生、尸体、媒体路径。
- 尸体载荷按物品对象逐件创建，保留家具图块／朝向和媒体节目；媒体载荷传递真实节目索引及可重建的家具类型，完成计数按节目／图块识别。
- 搜索命中当前方案不支持的新增条目时提示可用方案；普通物品路径保留。未放开不能保留状态的字符串生成路径。
- 补充游戏 JVM、实际序列化、媒体取出、跨 helper 类加载与生产 C++ 尸体路由回归；尚未进行真实联机服务端验收。
- 仅准备本版 VMP Marker DLL，等待用户完成 VMP 处理后再构建 Launcher；Stable 成品保留。

# v1.0.1 Stable — 已完成本地便携打包，未发布 GitHub Release

- 用户已确认 beta.9 的俯视角人物上色／边缘发光修复有效；沿用已验证的第一人称、静默自瞄、自动视角匹配和普通 Spawner 具名 VHS/CD 路径。
- main 作为 Stable 基线，beta 作为默认开发与后续提交目标；没有明确 Stable 指令时不向 main 提交。
- VMP 标记新增原版模型捕获、ViewPoint 模型效果、实际开火和 VHS/CD 接口初始化；启动与缓存区域改用 Mutation，整理 noinline 和启动标记边界。逐帧绘制与回调保持原生执行。
- 提供独立 Marker DLL 构建、20 个具名区域的源代码／PE 校验、哈希交接清单。等待用户完成 VMProtect 编译后再构建 Launcher；本阶段不创建 Release/tag、不打包。

# v1.0.0-beta.9

- 对照骨骼／方框快照和实际游戏帧时序，原版模型改为在世界帧内提交；避免通用主线程队列中的绘制命令被下一帧初始化清掉。
- 为额外模型建立各自的 ModelCameraRenderData，并恢复正常／异常退出后的相机状态。
- 采集恢复只读，删除旧轮廓注册与 alpha 状态写入，消除本工具在游戏轮廓集合上的跨线程写入。
- 修复把缩放世界缓冲视口重放到 UI 大小效果纹理导致的错位／裁剪；真实游戏着色器和生产捕获回归覆盖僵尸、玩家、独立填充／边缘、主角遮罩和三种缓冲尺寸。
- 日志新增世界帧回调、模型提交和绘制计数；保留第一人称实现及已有自动投影行为。
- 更新时需要完全退出游戏，重新加载 helper 和帧回调。当前存档中的画面及错误计数仍待实机复测，详见 docs/NATIVE_WORLD_FRAME_EFFECTS.md。

# v1.0.0-beta.8

- 修复俯视角显式模型捕获未初始化每玩家光照数据而抛出 `modelSlot.model.playerData is null` 的问题；使用引擎 updateLights → initModel → init 流程。
- ESP、模型效果、Lua 物资 ESP 与瞄准投影按 ViewPoint 当前渲染状态自动匹配；移除手动兼容／3D 按钮，旧配置开关不再覆盖自动选择，相机未就绪时暂停对应操作。
- 移除独立 VHS/CD 生成页面；普通 Spawner 保留全部具名节目搜索、指定内容生成与数量／目标位置选项。
- 保留已由用户确认正常的第一人称帧回调和静默自瞄；保留自动急停已知问题悬浮提示。
- 增加实际游戏类的空 playerData 构造回归、反复视角切换和旧配置标记回归；详见 docs/AUTO_PROJECTION_NATIVE_CAPTURE.md。当前版本俯视角画面及连续切换仍需游戏内复测。

# v1.0.0-beta.7

- 修复模型捕获遗漏 initModel 导致 character 为 null 的异常，按游戏协议维护模型槽渲染引用，并核对对象与槽的真实归属。
- ViewPoint 模型效果改为当前世界帧结束前绘制；叠加层发布选择，避免读取已退休、可复用的模型批次。
- 新增侧栏“VHS/CD 生成”页面，支持按具体节目选择、零售／家庭 VHS 与 CD 分类，复用物品生成链路。
- 为 Rage 自动急停增加已知卡住问题和取下枪械恢复方法的悬浮提示，未修改急停实现。
- 记录用户确认 beta.6 静默自瞄修复有效；本次保留其实现。
- 增加生产 JNI/JVMTI 帧钩子、帧回收流程、模型槽释放和具名媒体页面验证；实机存档和第一人称画面仍需复测。详见 docs/MODEL_CAPTURE_MEDIA_UI.md。

# v1.0.0-beta.6

- 修复 VHS／CD 仅生成空白基础物品的问题：按已注册节目扩展目录，支持标题／作者与中英文关键词搜索；生成时绑定实际媒体索引，通用条目随机选择同类别可播放内容。
- 修复仅启用静默自瞄时未收集目标、未调用弹道重定向的条件缺口；保留手动瞄准／开火并在未瞄准时清理覆盖。
- 汇总本会话尚未发布的物资 ESP 缩放与自动／手动 3D 切换、家具隔板目录、人物材质与独立边缘发光、ViewPoint 第一人称路径、各 ESP 项实际可见检测开关。
- 增加默认配置选择与自动载入、旧配置迁移和新增选项默认关闭处理。
- 新增真实游戏 JVM／原生弹道、媒体目录、配置迁移和 OpenGL 回归；Portable 使用静态 MSVC 运行库并嵌入对应 DLL 与 Java helper。
- 本包为预发布，未完成游戏内实际录像带播放与用户反馈射击场景验收；验证范围和使用说明见 docs/RECORDED_MEDIA_SILENT_AIM.md。

# Unreleased source baseline

## 2026-10-13 发布整理

- Trainer 和 Launcher 的 Release 构建统一使用 `/MT` 静态运行库；最新 VMP Portable 包不再附带外置 VC++ 运行库 DLL。
- 补齐物品刷新按钮位置、生成链路说明与 ItemESP ViewPoint 3D 投影接入的发布记录。
- 清理 84 个旧构建目标、426 个文件，合计 580,747,638 bytes；保留最新发布包、VMP 输入与输出、构建诊断和当前 Release 产物，清理后哈希与内嵌 DLL 校验通过。
- 远端拉取成功，无新增远端提交或合并冲突；普通推送因 GitHub 443 连接超时失败，保留本地提交，未重复重试。详见 `docs/logs/2026-10-13-build-cleanup-and-sync.md`。

## v1.0.0-beta.5

本次更新完善了运行时 Mod 切换后的物品目录处理，新增物品列表刷新按钮，刷新成功后重建目录并清理失效选择，刷新失败时保留当前可用列表。同步完成了 Trainer 与 Launcher 的 Release 联动构建，更新多语言提示与发布校验流程。当前已验证布局回归、完整 C++ Release 构建和普通 Portable 打包；VMProtect 标记 DLL 已完成编译并经本机 VMProtect GUI 处理，保护版 Launcher 已重新嵌入并打包。
- Added responsive item quantity controls with bounded step buttons, Ctrl fast-step support, and offline layout/interaction coverage.
- Added light and media payload routes with Java/C++ bridge validation and inventory-only destination handling.
- Added a local SDK check script and unified `PZ_GAME_ROOT`/`PZ_JDK_ROOT` discovery for Java and Viewpoint scripts.

# v1.0.0-beta.2

Second public beta release, including the merged Viewpoint 3D compatibility work and configurable rage weapon modifiers.

- Added optional Viewpoint 3D ESP for zombies, players, animals, and vehicles, with 3D model Chams and separate 3D Legit/Rage aiming toggles.
- Embedded the Viewpoint bridge and model-rendering Java helper in the trainer build; the feature activates when a compatible Viewpoint mod is present in the game.
- Added per-weapon-group switches for accuracy, minimum-damage, and maximum-damage modifiers while preserving legacy configuration behavior.
- Upgraded configuration storage to format 19 with migration support for Viewpoint v18, both v17 variants, and earlier files.
- The published Windows x64 Portable package embeds the VMProtect-processed trainer DLL; end users do not need to install the VMProtect SDK.

Known limitations for this beta:

- Viewpoint 3D features require a compatible Viewpoint mod and matching Project Zomboid build; the portable package does not redistribute that third-party mod.
- The launcher and trainer require a compatible x64 Project Zomboid build.
- Game-in-process injection, 3D rendering, and UI behavior still require manual validation on the target machine.
- Source builds that enable VMProtect markers require a separately installed VMProtect SDK; the published Portable package already contains the processed trainer image.

## v1.0.0-beta.1

Initial public beta release of the combined PzTrainer trainer and launcher repository.

- Combined the trainer DLL and launcher into one buildable repository.
- Added a portable x64 release build that embeds the matching trainer DLL in the launcher.
- Removed generated build trees and machine-specific build defaults from the repository scope.
- Kept VMProtect markers optional and configurable through MSBuild properties.

Known limitations:

- The launcher and trainer require a compatible x64 Project Zomboid build.
- Game-in-process injection and UI behavior still require manual validation on the target machine.
- VMProtect-protected builds require a separately installed VMProtect SDK.
