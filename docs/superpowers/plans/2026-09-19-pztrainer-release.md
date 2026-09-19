# PzTrainer Monorepo Release Preparation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Merge the trainer DLL and its launcher into one auditable Windows x64 repository, remove generated and machine-specific clutter, produce a portable beta artifact, and publish the source and release on GitHub.

**Architecture:** Keep trainer and launcher sources in one repository so the launcher builds the trainer first and embeds the exact resulting DLL. Use repository-root-relative paths, configurable toolchain properties, one version source, and a PowerShell release script that emits one portable ZIP.

**Tech Stack:** Visual C++ 17/C++17, MSBuild, Win32/Direct2D/DirectWrite, ImGui/OpenGL, MinHook, PowerShell, GitHub CLI Releases.

## Global Constraints

- Publish one public GitHub repository under `ljy87263621`.
- Initial release tag is `v1.0.0-beta.1`.
- Do not add an open-source license in this release.
- Do not commit generated binaries, build trees, IDE state, or user-specific absolute paths.
- Preserve manual mapper and trainer runtime behavior unless a build or packaging fix requires a scoped change.
- Release must be a portable ZIP and document authorized local x64 Project Zomboid use.

---

### Task 1: Establish clean monorepo shape

**Files:** `README.md`, `.gitignore`, `trainer/`, `launcher/`

- [ ] Inventory current source and generated paths, then move the trainer tree/project under `trainer/` and launcher tree/project under `launcher/`.
- [ ] Keep one shared root `protection/` directory and update both projects to use explicit root-relative include paths.
- [ ] Ignore `bin/`, `obj/`, `artifacts/`, `build-*`, `.vs/`, IDE files, logs, PDB/ILK/LIB/EXP files, and empty `.codegraph/` state.
- [ ] Write README covering architecture, supported x64 target, prerequisites, authorized-use boundary, and portable release contents.
- [ ] Verify `git status`, `git diff --check`, and `git check-ignore` before continuing.

### Task 2: Make the combined build reproducible

**Files:** `trainer/pztrainer.vcxproj`, `launcher/pztrainer_launcher.vcxproj`, both resource scripts, `scripts/build-portable-release.ps1`

- [ ] Replace machine-specific VMProtect defaults with overridable MSBuild properties.
- [ ] Change the launcher project reference to `..\trainer\pztrainer.vcxproj` and make the embedded DLL path resolve from the trainer output.
- [ ] Make resource compilation fail clearly when the trainer DLL input is missing.
- [ ] Add one script that builds trainer first, launcher second, copies only the runnable payload, writes SHA-256 sums, and creates the versioned ZIP.
- [ ] Run the script and record the exact prerequisite failure if the local MSVC/VMProtect environment cannot build.

### Task 3: Remove stale clutter and align release metadata

**Files:** release/handoff docs, launcher localization/version metadata, `CHANGELOG.md`

- [ ] Update all docs for the merged layout and current commands.
- [ ] Delete only the launcher’s confirmed dead `#if 0` authorization block; preserve active injection and UI code.
- [ ] Set `1.0.0-beta.1` consistently in user-facing version text, notes, and artifact name.
- [ ] Scan executable configuration for user-specific paths and secrets; historical audit notes may retain prior findings only when marked historical.

### Task 4: Build and validate the portable beta

**Files:** generated `artifacts/pztrainer-v1.0.0-beta.1-windows-x64-portable/` and ZIP

- [ ] Run `./scripts/build-portable-release.ps1 -Version '1.0.0-beta.1'` and require exit code 0 before calling the artifact buildable.
- [ ] Inspect ZIP contents and SHA-256 manifest; ensure no source tree, cache, PDB, or old ZIP is nested in the release.
- [ ] Run repository tests plus PE architecture/resource inspection. Report any unavailable game-in-process validation as residual risk.

### Task 5: Commit and publish

**Files:** Git history, GitHub repository, GitHub Release

- [ ] Review `git diff --check` and final status; stage only intended source/docs/scripts.
- [ ] Commit `chore: prepare combined trainer and launcher beta release` and tag `v1.0.0-beta.1`.
- [ ] Create public `ljy87263621/PzTrainer`, push the branch and tag, and create a prerelease with the portable ZIP and `CHANGELOG.md` notes.
- [ ] Verify repository visibility, tag, prerelease state, asset name, and published URL with `gh repo view` and `gh release view`.
