# 原版模型效果世界帧修复

**目标：** 让原版俯视角上色／边缘通过实际世界帧提交、实体身份捕获、UI 尺寸合成完整生效。

**证据：** 本机日志中骨骼和实体已就绪，但有效模型捕获为零；游戏 console 中有约 1200 次 FBORenderObjectOutline 空对象异常。游戏主线程通用队列先于帧构建执行，SpriteRenderState.prePopulating 会清空队列；原有命令提交点不成立。生产捕获 GPU 测试还复现世界缓冲尺寸增大时轮廓错位。

**路线：** C++ 只读快照选择实体，Java 原子发布选择；在 IngameState.renderFrameInternal 的 ModelOutlines.endFrameMain 前提交。沿用实际 ModelSlotRenderData 和 ModelCameraRenderData；OpenGL 捕获继续按相同实体身份重放，在 UI 大小的 mask 内保持目标视口。ViewPoint 路径不变。

- [x] 新增只读采集约束，移除旧轮廓注册和 alpha 状态写入；模型就绪不再依赖物件轮廓 UI 时间。
- [x] 验证并新增世界帧回调；选择发布与实际提交分离，主角遮罩优先，选择超时失效。
- [x] 补齐每模型相机配置与异常恢复，用真实游戏相机数据验证归属／恢复。
- [x] 用真实游戏着色器、LWJGL、JNI/JVMTI、生产 MinHook 捕获和重放复现缩放失败并验证修复。
- [x] 扩展捕获像素检查与诊断；运行原版／ViewPoint 回归、Release 构建、资源和便携包校验。
- 按会话已有授权同步源码和新的便携预发行包；实机画面与右下角错误计数仍需重启游戏复测。
