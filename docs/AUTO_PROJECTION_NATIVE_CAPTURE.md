> beta.9 进一步修复世界帧提交、模型相机和缩放合成，当前模型检修见 [新版说明](NATIVE_WORLD_FRAME_EFFECTS.md)。

# 自动视角匹配与俯视角模型捕获（beta.8）

## 行为

视觉 ESP、模型上色／边缘发光、Lua 物资 ESP 和瞄准投影现在自动跟随游戏当前使用的渲染器。ViewPoint 已安装但未启用渲染时使用原版俯视投影；启用 ViewPoint 渲染且相机有效时使用 3D 投影。切到 3D 后相机尚未就绪时暂停对应绘制与瞄准，等待下一帧，不向 3D 画面提交俯视坐标。

原手动兼容、3D ESP、Legit/Rage 3D 和 Lua 物资 ESP 的自动／手动切换按钮已移除。各功能总开关、可见性检测及颜色选项仍独立配置。旧配置中的四个 ViewPoint 布尔值保留二进制位置用于兼容，但不再决定投影；原特性注册表中的可写投影开关也已移除。重新加载旧配置不会关闭自动适配。

独立 VHS/CD 页面已移除。普通物品生成器中的具名节目目录、标题／作者／关键词搜索和精确媒体绑定保持原有实现；选择具体节目后可按数量生成到背包或地面。普通基础 VHS/CD 项仍按原规则选择该类别节目。

## 俯视角异常根因与修复

反馈异常为 `Cannot load from object array because "modelSlot.model.playerData" is null`。原版常规渲染会更新模型光照并创建每玩家数据，但额外模型捕获会提交未经过常规可见渲染的模型，不能假设该数据已存在。

对照本机游戏 `ModelInstance.updateLights()` 与 ViewPoint 的 `Characters.fresh` 流程，原版捕获现在先调用引擎 `slot.model.updateLights()`，再执行 `ModelSlotRenderData.initModel(slot)` 和 `init(slot)`。引擎负责按当前玩家创建数据并更新光照；成功／失败时沿用模型槽引用释放协议。没有通过跳过空数据模型来隐藏异常。

第一人称继续通过 ViewPoint 当前世界帧结束前的回调绘制，不改变已由用户确认正常的回调时机。显式实体身份匹配、本地主角遮罩、独立全身／边缘效果及逐项可见性策略保持。

## 验证

- `NativeModelCaptureTest` 使用实际游戏的 ModelInstance / ModelSlotRenderData，从 `playerData=null` 开始；旧实现复现附件同一异常，修复后完成帧初始化，覆盖玩家索引 0→1→0 和引用释放。无窗口角色夹具仅替换动画获取及透明度，使用未加载几何验证构造生命周期，不代替真实角色画面验证。
- `test-model-colour.ps1`：原版身份隔离和主角遮罩 GPU 检查、独立全身／边缘模式、生产 C++ 分流、真实 ViewPoint API／着色器、JNI/JVMTI 帧回调及渲染状态恢复。
- `test-viewpoint.cmd`：无 Mod、Mod 未启用、有效 3D 相机、相机未就绪和连续八次切换；旧手动配置为 false 仍能正确选择路径。
- `test-item-esp.ps1`：在游戏 Kahlua 解释器中执行完整物资脚本，覆盖缩放、可见性、旧手动值和投影切换。
- `test-recorded-media.ps1`：实际媒体目录和创建逻辑；`run_item_quantity_layout_test.ps1`：共享生成器四语言布局与数量控件交互。
- `test-visibility-config.ps1`：配置 v21 回读、v20 升级、新增选项默认状态及默认配置选择。

本次自动检查使用本机安装的游戏与 ViewPoint 类。用户已确认上一版第一人称和静默自瞄正常；本次俯视角视觉效果、从第一人称返回俯视角、连续切换及存档中的节目播放仍需要游戏内复测，因此作为预发行版交付。

## 更新

使用新便携启动器时请重新启动游戏后加载，以便 JVM 使用新版内嵌 Java helper。Lua 示例随包更新；已有自定义物资脚本可自行合并投影函数的变动。
