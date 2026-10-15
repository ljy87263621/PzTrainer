# 人物热色材质扩展

本地验证构建，2026-10-14。支持僵尸和玩家；原版投影与 ViewPoint 3D 分别沿用本项目的模型渲染桥。

## 使用

1. 完全退出已注入修改器的游戏，然后用本次构建的 `pztrainer_launcher.exe` 启动并注入。新材质需要新的 DLL 和其内嵌 Java 辅助包，不能只通过重新加载 Lua 更新。
2. 在 ESP 对应的「僵尸」或「玩家」分类开启实体 ESP，开启「全身上色」，从「上色效果」选择「彩色热能」或「白热」。模型预览会显示对应效果。「边缘发光」是独立开关。
3. 原版画面直接使用所选材质。使用 ViewPoint 时，beta.8 会根据当前视角自动切换投影。

彩色热能使用暗紫、红、黄到亮白的色阶；白热使用灰度。温区依据人物模型局部坐标分布，胸腹和头部较亮、四肢较暗，随骨骼动画和摄像机移动。它是视觉材质，不读取体温，不改变全屏场景，也不代表僵尸的真实温度。

热色模式固定使用其色阶，原模型颜色的 RGB 不替换温区色阶；模型颜色的透明度仍有效，墙后/视野内状态也继续选取各自透明度。原版和 ViewPoint 现在都支持独立边缘发光，可仅上色、仅边缘或同时开启。第一人称与重叠对象修复的详细说明见 [MODEL_COLOUR_FIXES.md](MODEL_COLOUR_FIXES.md)。

原有上色选项保留，新模式仅加入僵尸/玩家分类；动物和车辆选项未扩展。距离、强制显示模型和当前会话限制沿用已有规则。配置结构大小不变，原有枚举编号 0–7 保持不变，新增彩色热能为 8、白热为 9。Lua 功能接口对应值为 `thermal`、`white_hot`。

## 实现与参考

保留 Project Zomboid / ViewPoint 的顶点着色器、骨骼变换以及片元透明裁切，在独立的材质程序中替换人物表面 RGB；不修改或重链接游戏原程序。原版与 3D 使用同一份内嵌 GLSL 色阶，CPU 预览实现有 GPU 数值一致性检查。材质程序按 OpenGL 上下文和来源着色器身份缓存，并识别程序编号复用。

参考了 [Osiris 的人物模型发光](https://github.com/danielkrupinski/Osiris/blob/master/Source/Features/Visuals/ModelGlow/PlayerModelGlow/PlayerModelGlow.h) 和 [轮廓发光](https://github.com/danielkrupinski/Osiris/blob/master/Source/Features/Visuals/OutlineGlow/PlayerOutlineGlow/PlayerOutlineGlow.h) 对实体筛选、颜色与效果配置的组织方式。本次材质为针对 Project Zomboid 的原创实现，没有移植 CS2 地址、接口或渲染钩子，也没有复制 Osiris 源码。Osiris 当前主分支使用 Panorama UI；本项目继续使用自己的 ImGui 控件。

审计并修复了既有 ViewPoint 上色桥的兼容问题：本机 mod 的模型记录从 60 个 float 增加到 68 个，旧桥写死长度，无法初始化。现在读取 mod 的 `VALUES` 和 `OUTLINE` 常量，校验有效范围；测试覆盖 60/68 两种记录布局以及未标记场景对象隔离。

## 验证与范围

- C++ 信号、色阶、原枚举编号和 GLSL 包装检查通过。
- 原版 GPU 检查使用本机游戏的实际 `aim_outline_solid` 顶点/片元源，在隐藏 WGL 上下文与独立帧缓冲中验证：热色梯度、CPU/GPU 数值一致、白热灰度、透明纹理裁切、透明度、骨骼矩阵移动和切回普通材质。
- ViewPoint 检查使用本机 mod 的着色器预处理器编译实际 `model.vert` / `model_outline.frag`，验证克隆材质的接口兼容；同时通过实际 ModelPass/ModelBatches 与受控网格的热色、白热、切换、透明度和场景隔离像素检查。
- OpenGL 状态恢复检查覆盖程序、纹理/采样器、间接绘制缓冲、视口、深度、剔除、裁剪和混合状态。
- 上一轮物资 ESP 的 62 项回归检查以及 ViewPoint 投影/JNI 检查继续通过。
- 本次没有进入游戏存档验证真实服装组合、全部人物姿态、实际菜单交互或大量实体下的帧率。已通过 GPU 测试，不应将其表述为全部实机场景验收通过。

测试入口：

```powershell
.\scripts\test-thermal-materials.ps1 -GameRoot $env:PZ_GAME_ROOT
.\scripts\test-viewpoint-chams.ps1 -GameRoot $env:PZ_GAME_ROOT -JdkRoot $env:PZ_JDK_ROOT -ViewpointJar '<本地 ViewPoint.jar>'
```

便携构建内嵌所有自有渲染辅助资源，使用者无需安装 Python、JDK 或额外 C++ 运行库。ViewPoint mod 仍需由使用者自行安装，便携包不分发该第三方 mod。
