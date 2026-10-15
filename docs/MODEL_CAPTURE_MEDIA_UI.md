> 本文记录 beta.7。beta.8 已移除独立 VHS/CD 页面并改为自动视角匹配，当前行为见 [新版说明](AUTO_PROJECTION_NATIVE_CAPTURE.md)。

# beta.7 模型捕获与 VHS/CD 独立页面

## 原版模型捕获

已在本机游戏类中复现附件同样的 NullPointerException：直接 ModelSlotRenderData.alloc().init(slot) 时，内部 character 尚未设置。游戏引擎（以及 ViewPoint 自身 Characters.fresh）先调用 initModel(slot)，再调用 init(slot)。此次遵循该顺序，提交时增加模型槽 renderRefCount，由 postRender 成对释放；初始化异常也释放已经取得的引用，单个对象失败不再终止整批捕获。

对象选择沿用骨骼 ESP 的 FrameSnapshot.identity（System.identityHashCode），再验证 live ModelSlot.character 和 ModelInstance.object 都指向同一角色。已经释放、移除或被其他对象复用的槽不会提交。模型帧 alpha 设为 1，效果透明度仍由独立合成层控制，使关闭实际可见检测时不因原版迷雾 alpha 而消失。本地主角继续使用独立排除遮罩。

## ViewPoint 当前帧绘制

旧实现从最终叠加层直接读取 ViewPoint 当前批次。ViewPoint SceneDrawer 会在这一阶段之前退休帧，而场景数据可被生产线程 reset 复用；这与叠加层读取批次存在竞争。现在叠加层仅发布效果和骨骼 ESP 中同源的对象身份选择，实际绘制通过校验后的 WorldRenderer.finish 回调，在 ModelPass.endFrame、帧退休之前执行。这样对象、GPU 批次和相机数据属于同一帧。

该钩子只匹配指定类、finish(SceneData) 签名和一个 endFrame 调用；不兼容时显示校验失败，不猜测新布局。关闭 3D ESP／兼容模式或离开允许的会话时清除已发布选择，选择也有 500 ms 的失效时间。绘制仍使用独立上色／边缘目标并恢复 OpenGL 状态。

原版和 ViewPoint 的全身上色与边缘发光独立。仅开启边缘时，材质为 Disabled 是正常状态；需要全身上色时开启“全身上色”并选择具体材质。附件中的空角色异常本身仍是需要修复的缺陷，不由该开关组合导致。

## VHS/CD 生成

侧栏“角色”分组增加“VHS/CD 生成”页面，复用物品生成器目录、数量控件、背包／地面目标及 SpawnItem 原生执行链路。此页只显示注册表中带节目内容的具名记录，按零售 VHS、家庭 VHS、CD 分类，支持节目名、标题、作者和搜索别名。

进入页面时刷新目录，以免菜单阶段缓存了尚未注册节目时的列表。基础 Base.VHS_Retail／VHS_Home／Disc_Retail 随机模板不进入此页；同一名称有多个记录时仍按内部媒体 ID 区分。普通物品页保留原有功能。具名媒体原生生成仍仅支持本地游戏，会话不允许时禁用执行。

## 自动急停与静默自瞄

仅对“自动急停”增加圆角悬浮提示：周围没有僵尸后偶尔仍会锁住移动，取下枪械恢复后再重新装备；不需要时关闭自动急停。没有改动急停实现或其他 Rage 行为。用户已确认 beta.6 静默自瞄修复有效，本次保留该实现。

## 验证与界限

- 实际游戏类的空角色异常复现；初始化顺序字节码检查、槽归属检查和实际 postRender 引用释放测试。
- 实际安装的 ViewPoint 类转换验证，以及生产 C++ JNI/JVMTI 重转换安装测试。
- OpenGL 受控帧验证：发布阶段不绘制、世界帧回调绘制、场景 reset 后叠加层只读取绘制结果、清除后不再绘制；既有第一人称透视／Panini、上色／边缘、主角排除与 GL 状态恢复回归。
- 具名媒体实际游戏 JVM 55 项检查、C++ 目录／分类／搜索测试、物品和媒体页面布局与数量交互检查。
- Release 产物验证内嵌 Java helper、DLL 一致性及便携包 SHA-256。配置仍沿用现有格式。

本次验证使用本机安装的游戏和 ViewPoint 类以及受控渲染上下文。没有运行中的游戏进程，因此尚未完成用户存档、实际第一人称画面和连续游玩中的实机验收。发布为预发布版，并保留状态栏错误信息用于兼容性反馈。
