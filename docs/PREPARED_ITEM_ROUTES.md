# v1.0.1 Hotfix1：家具与录制节目的生成路径

本修复由 v1.0.1 beta1 提升为 v1.0.1 Hotfix1 Stable，进入 `main`；`beta` 保留为默认开发线。复用已验证的 beta1 VMP DLL，仅更新 Launcher 版本及分支显示；保留旧成品。本轮不创建 Release/tag。

## 问题与修正

旧版家具和具名 VHS/CD 目录的 `spawn_method_mask` 只包含 Game；切换到联机方案时，前端按能力过滤，所以两类条目全部消失。执行层和尸体批量入口也明确拒绝了这两类条目。这不是刷新失败。

| 方案 | 家具图块／隔板 | 具名 VHS/CD | 原因／限制 |
|---|---|---|---|
| 游戏原生 | 支持 | 支持 | 保留原有本地生成 |
| 尸体载荷 | 支持 | 支持 | 在游戏主线程创建独立完整对象，再加入尸体容器 |
| 灯具载荷 | 支持 | 不支持 | 灯泡字段仅传递类型，不能保留节目索引；家具仍会沿用灯具路径的颜色处理 |
| 媒体载荷 | 支持 | 支持 | 家具传可重建的图块类型；节目传真实基础类型与节目索引 |
| 联机取物中的字符串／专用物品路径 | 不支持动态家具 | 不支持指定节目 | 普通 AddItems 字符串查表缺失动态家具；仅基础类型不足以保留节目内容 |

通用 VHS/CD 基础条目也统一使用能生成有内容媒体的路径，防止其他方案重新生成空壳。原有普通物品能力保持不变。

尸体载荷改为通过 `PreparedSpawnItems.addToContainer` 逐件创建、核对容器归属；参数或创建失败时不留下部分插入。直接复用 `AddItems(String, int)` 或 `AddItems(InventoryItem, int)` 都会回到字符串查表，不能保留准备好的状态。

媒体载荷复用原协议中的媒体索引，不把 `RecordedMedia:<id>` 当作游戏物品类型发出。家具必须使用 `Moveables.<sprite>` 作为工厂类型，不能直接假定对象的 `getFullType()` 可重建；完成计数按家具图块／具体节目识别。

搜索命中被当前方案排除的家具或节目时，界面显示排除数量和可用方案，不把不支持的路径伪装成成功。

## 构建与验证

以下 Marker 构建命令仅用于以后重建 Trainer；本次版本提升直接复用 beta1 的用户保护产物，无需再次 VMP。

```powershell
.\scripts\test-prepared-item-routes.ps1 -GameRoot $env:PZ_GAME_ROOT -JdkRoot $env:PZ_JDK_ROOT
.\scripts\test-recorded-media.ps1 -GameRoot $env:PZ_GAME_ROOT -JdkRoot $env:PZ_JDK_ROOT
.\scripts\test-moveable-catalog.ps1 -GameRoot $env:PZ_GAME_ROOT -JdkRoot $env:PZ_JDK_ROOT
.\scripts\build-prepared-item-bridge.ps1 -GameRoot $env:PZ_GAME_ROOT -JdkRoot $env:PZ_JDK_ROOT
.\scripts\build-vmp-marker.ps1 -Version '1.0.1-hotfix.1' -DisplayName 'v1.0.1 Hotfix1' -VmProtectRoot 'D:\Develope\VMProtect'
```

回归覆盖全部 11 个生成方法的能力策略、真实目录、独立对象与数量、异常回滚、游戏原生物品序列化往返、节目数据、家具图块／朝向、实际 DeviceData 取出、分离 helper 类加载器，以及生产 C++ 尸体路由与静态 Java Expression 调用。UI 复用原有布局回归。

本轮结果：普通与分离类加载器下各 123 项准备物品检查、生产尸体路由、媒体原有 55 项检查、家具实际图块目录（6,399 个部件／12 个隔间样本）、144 项布局与 18 项交互检查均通过。完整 x64 Marker 构建通过，仅保留第三方 MinHook 既有警告；20 个标记的实际调用与 Lua/player 两份资源匹配检查通过。旧 Stable 的 DLL、Launcher 和 ZIP 哈希未改变。

这些是隔离的本机引擎／JNI 验证，没有连接真实联机服务器。服务端版本、Mod 数据和检查规则仍可能影响结果；请优先验证尸体与媒体方案的指定节目，以及家具取出、重连后的持久性。列表支持不等于已经完成全部服务器兼容性验收。
