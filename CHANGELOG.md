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
