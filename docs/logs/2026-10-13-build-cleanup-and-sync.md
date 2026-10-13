# 2026-10-13 构建清理与 Git 同步日志

## 结果

清理 84 个旧构建文件或目录、426 个文件，文件大小合计 580,747,638 bytes（约 553.84 MiB）。清理后最新发布包、保护 DLL 和当前 Release 二进制的 SHA256 均与清理前一致。未修改程序逻辑，也未重新构建或改写发布 ZIP。

Git 拉取成功，拉取后的比较为本地超前 3 个提交、远端新增 0 个提交，无需合并。普通推送失败：GitHub 443 连接约 21 秒后超时，退出码 128。网络错误并非合并冲突；遵循用户要求，未重复重试、未强制推送。三个源码提交及本次日志保留在本地 Git，网络恢复后可执行普通 `git push origin main`。

## 保留内容

- 最新 ZIP 与解压目录：`artifacts/pztrainer-v1.0.0-beta.5-752bb43-windows-x64-portable-vmp`。
- 最新 /MT VMP 标记 DLL、保护输出、PDB 与标记清单：`artifacts/vmp-input-static-crt/`。
- 最新保护版 Launcher 构建日志、资源校验脚本和静态 CRT 导入诊断归档：`artifacts/vmp-input-static-crt/diagnostics/`。
- 当前 `bin/x64/Release/` 输出与 `obj/pztrainer/`、`obj/pztrainer_launcher/` 构建缓存（移除旧 `pztrainer_MD.tlog`）。
- 源码检查点、审计资料和游戏 API 分析资料：`obj/checkpoints/`、`obj/audit-pr3-20261009/`、`obj/viewpoint-fixtures/`、`artifacts/pzserver-classes/`、`artifacts/pzserver-static/`。

## 保留产物校验

- 最新 ZIP SHA256：`397C32B9778A073E73054141F5FA3B0FC125FB5D2CB129DA19BB3BCC588E1C79`。
- 保护 DLL / Launcher 内嵌 DLL SHA256：`9F9DF5D80E99335DEB0266E0E027AD2EDB2B6D692B1A031068D9014D2C4AAE20`。
- ZIP 完整性及文件哈希清单：通过。
- Launcher RCDATA/101 内嵌 DLL：18,342,912 bytes，与保护输出逐字节一致。
- 清理前后的 6 个关键文件（ZIP、Launcher、标记 DLL、保护 DLL、当前 bin 下 DLL 和 EXE）尺寸与哈希：通过。
- 每个删除目标均验证为仓库内部绝对路径，无跟踪源码、无路径链接，执行后确认目标已删除。

## Git 同步快照

- 清理前本地 HEAD：`752bb43d7b11eb19b82fcf261f06784db06eac12`。
- `git fetch origin` 后的 `origin/main`：`495401346db90ecbbe4826dc8aea4c7846b91804`。
- 待推送源码提交：

```text
752bb43 build: statically link CRT for portable releases
ccf744a fix: place item refresh beside execute action
1f568d5 fix: keep item spawn controls visible with 3d item esp
```

- `git push origin main`：退出码 128，`Failed to connect to github.com:443 after 21065 ms: Could not connect to server`。
- 本次操作日志与 CHANGELOG 随后提交到本地；提交后的最终分支状态另存于本机同步记录。

## 本机详细记录

以下文件位于 Git 忽略的产物目录，记录实际删除目标与验证证据：

- `artifacts/maintenance-20261013/cleanup-plan.json`
- `artifacts/maintenance-20261013/cleanup-completed.json`
- `artifacts/maintenance-20261013/cleanup-verification.json`
- `artifacts/maintenance-20261013/preserved-files-before.json`
- `artifacts/maintenance-20261013/git-source-push.txt`
- `artifacts/maintenance-20261013/git-sync-result.json`

构建产物仅在本地保留，本次没有上传 GitHub Release 或修改远端历史。

## 删除明细

| 相对路径 | 文件数 | bytes |
| --- | ---: | ---: |
| `%DLL%.dll` | 1 | 24,576 |
| `%DLL%.exp` | 1 | 712 |
| `%DLL%.lib` | 1 | 1,778 |
| `artifacts/packet_audit_test.exe` | 1 | 222,718 |
| `artifacts/PzTrainer-1.0.0-beta.1-admin-fix.exe` | 1 | 2,786,304 |
| `artifacts/pztrainer-20260905-120225-portable` | 3 | 7,841,854 |
| `artifacts/pztrainer-20260905-120225-portable.zip` | 1 | 2,909,379 |
| `artifacts/pztrainer-20260905-120645-portable` | 4 | 7,906,644 |
| `artifacts/pztrainer-20260905-120645-portable.zip` | 1 | 2,937,515 |
| `artifacts/pztrainer-20260908-224729-portable` | 4 | 7,908,178 |
| `artifacts/pztrainer-20260908-224729-portable.zip` | 1 | 2,938,017 |
| `artifacts/pztrainer-20260908-232715-portable` | 3 | 7,902,522 |
| `artifacts/pztrainer-20260908-232715-portable.zip` | 1 | 3,006,845 |
| `artifacts/pztrainer-v1.0.0-beta.1-admin-fix-windows-x64-portable` | 4 | 2,782,268 |
| `artifacts/pztrainer-v1.0.0-beta.1-admin-fix-windows-x64-portable.zip` | 1 | 1,780,013 |
| `artifacts/pztrainer-v1.0.0-beta.1-windows-x64-portable` | 4 | 2,782,780 |
| `artifacts/pztrainer-v1.0.0-beta.1-windows-x64-portable.zip` | 1 | 1,779,979 |
| `artifacts/pztrainer-v1.0.0-beta.2-windows-x64-portable` | 4 | 18,131,078 |
| `artifacts/pztrainer-v1.0.0-beta.2-windows-x64-portable.zip` | 1 | 16,091,611 |
| `artifacts/pztrainer-v1.0.0-beta.3-windows-x64-portable` | 4 | 2,791,484 |
| `artifacts/pztrainer-v1.0.0-beta.3-windows-x64-portable.zip` | 1 | 1,784,276 |
| `artifacts/pztrainer-v1.0.0-beta.4-windows-x64-portable` | 4 | 2,791,996 |
| `artifacts/pztrainer-v1.0.0-beta.4-windows-x64-portable.zip` | 1 | 1,755,211 |
| `artifacts/pztrainer-v1.0.0-beta.5-ccf744a-windows-x64-portable-vmp` | 17 | 20,746,945 |
| `artifacts/pztrainer-v1.0.0-beta.5-ccf744a-windows-x64-portable-vmp.zip` | 1 | 17,435,095 |
| `artifacts/pztrainer-v1.0.0-beta.5-windows-x64-portable` | 4 | 3,002,735 |
| `artifacts/pztrainer-v1.0.0-beta.5-windows-x64-portable-vmp` | 4 | 15,485,307 |
| `artifacts/pztrainer-v1.0.0-beta.5-windows-x64-portable-vmp.zip` | 1 | 11,991,389 |
| `artifacts/pztrainer-v1.0.0-beta.5-windows-x64-portable.zip` | 1 | 1,910,383 |
| `artifacts/server_role_capability_test.exe` | 1 | 131,678 |
| `artifacts/tests` | 11 | 1,527,115 |
| `artifacts/vmp-input-ccf744a` | 6 | 35,920,224 |
| `build-mingw` | 5 | 742,977 |
| `build-mingw-release` | 3 | 861,047 |
| `obj/aim_weapon_modifier_test.exe` | 1 | 139,410 |
| `obj/aim-modifier-msvc` | 3 | 30,015 |
| `obj/aim-modifier-msvc-release` | 3 | 30,031 |
| `obj/aim-weapon-modifier-test.exe` | 1 | 139,410 |
| `obj/extension-tests` | 16 | 3,185,764 |
| `obj/inspect-beta2-current` | 4 | 18,131,078 |
| `obj/item-catalog-refresh-test` | 1 | 443,852 |
| `obj/item-generator-text-test-final` | 1 | 258,560 |
| `obj/item-generator-text-test-green` | 1 | 258,560 |
| `obj/item-generator-text-test-red` | 0 | 0 |
| `obj/item-quantity-layout-baseline` | 22 | 4,445,955 |
| `obj/item-quantity-layout-test` | 16 | 4,483,882 |
| `obj/item-quantity-test-baseline.log` | 1 | 416 |
| `obj/item-quantity-test-final.log` | 1 | 346 |
| `obj/item-quantity-test-fixed.log` | 1 | 815 |
| `obj/item-quantity-test-interaction-red.log` | 1 | 494 |
| `obj/item-quantity-test-resumed.log` | 1 | 344 |
| `obj/java-bridge` | 24 | 94,182 |
| `obj/launcher-imports.txt` | 1 | 11,219 |
| `obj/launcher-protected-static-crt-build.log` | 1 | 500,212 |
| `obj/launcher-vmp-ccf744a-build.log` | 1 | 500,110 |
| `obj/overlay-quantity-build.log` | 1 | 5,181 |
| `obj/overlay-quantity-verification` | 139 | 225,316,080 |
| `obj/packet_audit_test.exe` | 1 | 197,120 |
| `obj/protected-staging-beta2` | 0 | 0 |
| `obj/pztrainer/x64/Release/pztrainer_MD.tlog` | 0 | 0 |
| `obj/rage-fire-jvm` | 2 | 225,958 |
| `obj/rage-fire-jvm-release` | 7 | 267,595 |
| `obj/rage-tests` | 12 | 440,478 |
| `obj/release-notes-beta2.md` | 1 | 963 |
| `obj/release-staging-beta2-vmp` | 4 | 18,131,078 |
| `obj/server_role_capability_test.exe` | 1 | 155,136 |
| `obj/static-crt-launcher-build.log` | 1 | 500,202 |
| `obj/ui-layout-audit-build` | 15 | 3,830,009 |
| `obj/ui-layout-audit.exe` | 1 | 526,848 |
| `obj/ui-layout-baseline.log` | 1 | 12,338 |
| `obj/unprotected-beta2` | 3 | 7,508,466 |
| `obj/vehicle-ghost-mapper-test` | 2 | 93,696 |
| `obj/viewpoint-fixture-classes` | 5 | 1,559 |
| `obj/viewpoint-tests` | 3 | 585,674 |
| `obj/viewpoint-tests-release` | 3 | 555,826 |
| `obj/vmp-attempt-beta2` | 4 | 20,645,355 |
| `obj/vmp-attempt-beta5` | 3 | 17,922,541 |
| `obj/vmp-marker-build-ccf744a.log` | 1 | 335,530 |
| `obj/vmp-markers-beta2` | 2 | 5,622,784 |
| `obj/vmp-static-crt-trainer-build.log` | 1 | 335,534 |
| `obj/zipcheck-beta2-vmp` | 4 | 18,130,516 |
| `obj/zipcheck-beta2-vmp-final` | 4 | 18,131,078 |
| `vehicle_ghost_runtime_test.exp` | 1 | 727 |
| `vehicle_ghost_runtime_test.lib` | 1 | 2,078 |
