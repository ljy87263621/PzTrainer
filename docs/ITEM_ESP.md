# 物资 ESP 缩放修复与 3D 切换

2026-10-14。本次为本地验证构建，未发布到 GitHub。

## 使用

已更新 `%APPDATA%\PZ Solo Assist\lua\examples\item_esp.lua`。在修改器的 Lua 页面重新加载该脚本，然后打开「启用物品 ESP」。

| 控件 | 行为 |
| --- | --- |
| 自动切换 3D Item ESP | 默认开启。复用修改器的 ViewPoint 检测；3D 视角启用时使用矩阵投影，未加载或关闭 3D 视角时使用原版投影。 |
| 手动启用 3D Item ESP（关闭自动后生效） | 先关闭自动切换。手动开启选择 3D 投影，手动关闭选择原版投影。 |

手动开关选择投影方式。有效 3D 绘制仍需要可兼容的 ViewPoint、修改器的 ViewPoint 兼容模式和有效相机矩阵。相机不可用时暂不绘制，避免使用错误的原版坐标。

新版脚本兼容已有 `PZSA.world.to_screen` 的 API 3 DLL；新 DLL 增加 `PZSA.world.viewpoint_status(width, height)`，在每次 Lua UI 绘制前刷新当前 3D 相机。要使用新的本体保护和当前帧刷新，请完全退出游戏，再用本次构建的启动器启动并注入。Lua 文件本身可通过重新加载立即更新。

试用包中的 `lua/examples/item_esp.lua` 是仓库版本的副本。当前机器已更新上述 AppData 路径；在另一台机器上，将副本放入修改器的 Lua 脚本目录后加载。无需安装 Python、JDK 或 C++ 运行库来使用构建产物。

## 修复和审计结果

- 原版错位根因：原脚本使用 `IsoUtils.XToScreen/YToScreen` 减去相机偏移，遗漏将整个相机相对坐标除以 `getCore():getZoom(0)`。已在本地游戏 JAR 的 `XToScreenExact/YToScreenExact` 字节码中确认相机偏移换算，并与修改器本体一致。
- 地面物品原先统一投影到格子中心。现在使用 `IsoWorldInventoryObject.getWorldPosX/Y/Z()`，包含物品的格内偏移与高度。容器内容继续使用所在格子的中心作为共享锚点，并堆叠标签。
- 3D Lua 物资 ESP 使用原生矩阵输出的 UI 坐标，不重复应用原版 zoom；新状态接口在绘制时读取当前相机矩阵。
- 本体的僵尸、玩家、动物、车辆原版投影路径已有 zoom 换算。发现并修复的本体边界问题是：已请求 3D ESP 且 mod 启用，但相机不可用时，之前会回退到原版坐标绘制。现在等待相机恢复。

## 验证

- 修改前在游戏自带 Kahlua 中执行完整旧脚本：`vanilla X follows zoom and camera` 断言失败。
- 修复脚本及实际 AppData 文件：62 项检查通过，包括 zoom 0.5/1/1.5/2、相机移动、格内偏移、自动与手动模式、旧 DLL 回退、不可用相机、剪裁、黑名单、主开关、死亡与卸载清理。
- C++ ViewPoint 测试：原生投影、状态组合、分辨率变化、FOV 变化、移位/旋转相机、缺失渲染帧与启停通过。JNI 使用本机安装的 ViewPoint JAR，并同时测试未加载 mod 的环境。
- Release DLL 与启动器编译成功；启动器 RCDATA 101 中的 DLL 与构建 DLL 字节完全相同。
- 导入表只包含 Windows 系统 DLL，无额外 C++ 运行库依赖。构建有既有 MinHook 警告，JNI 测试有游戏附带 JOML 的 Unsafe 弃用警告。

复现测试：

```powershell
.\scripts\test-item-esp.ps1 -GameRoot $env:PZ_GAME_ROOT -JdkRoot $env:PZ_JDK_ROOT
.\scripts\test-viewpoint.cmd $env:PZ_GAME_ROOT '<本地 ViewPoint.jar 路径>'
```

本次未启动游戏进入存档，未做实机画面或实际 UI 开关操作的视觉验收。离线测试通过不等于所有游戏/Mod 版本的实机兼容性已确认。

## 回退

旧 Lua 备份：`%APPDATA%\PZ Solo Assist\lua\examples\item_esp.lua.20261014-pre-zoom-fix.bak`。需要回退时用它恢复 `item_esp.lua`，再重新加载脚本。
