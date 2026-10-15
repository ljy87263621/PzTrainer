# Character thermal materials implementation plan

**Goal:** Add coloured thermal and white-hot character materials for zombies and players in vanilla and ViewPoint rendering, using the existing ESP controls.

**Architecture:** Preserve the captured game/mod vertex shader and fragment alpha/discard path. Wrap their main functions to export local bind-pose position and replace only the material RGB with a shared thermal palette. Native replay supplies captured bone/transformation uniforms to the cloned program; ViewPoint keeps its own batching and palette buffers. Preview evaluates the same local-position signal.

**Tech stack:** C++17, existing ImGui controls, JNI, Java 17 bytecode, GLSL 330, Windows OpenGL. No new runtime installation.

## Constraints

- Current chat executes independently, as selected by the user.
- Only zombies/players expose new materials. Preserve all existing effect enum values and configuration structure sizes.
- Keep the prior item ESP changes intact.
- Thermal appearance is a visual model material, not a temperature measurement or full-screen scene effect.
- Existing range, opacity and forced-model-visibility controls remain applicable.
- No CS2 hooks, addresses, classes or Panorama implementation are transplanted from Osiris.

## Work

- [x] Add native tests for stable enum values, thermal signal/palette, shader rewriting, real hidden-context framebuffer rendering, alpha mask preservation, bone transforms and shader-state restoration. Run a failing test before implementation.
- [x] Implement `thermal_material.hpp` (CPU palette and shared GLSL/source builders) and `native_model_material.hpp` (cached cloned programs and uniform locations). Route character replay through these programs when selected; preserve original rendering for all other effects.
- [x] Append Thermal and WhiteHot to `ZombieModelEffect`; expose them in zombie/player custom dropdowns and Lua feature options. Add localized labels and render the animated model preview with the same local-position colour signal.
- [x] Add `ModelMaterial.java` for ViewPoint shader cloning, uniforms and failure handling. Extend `ViewpointModelChams.render` with the effect array, retaining the old overload. Bundle shared GLSL in the embedded helper JAR and update the JNI call.
- [x] Expand controlled-mesh ViewPoint framebuffer/state tests for non-flat thermal colour, white-hot output, transparency, shader uniforms, switching back and scenery isolation. Run both native and ViewPoint checks.
- [x] Build Release DLL and launcher, verify embedded helper/resources and launcher DLL, then package a local portable validation build with usage notes. Record any live-game verification limits.

Reference: Osiris current player model glow and outline glow implementations separate appearance from entity classification and configuration; their CS2 renderer is not applicable here. No source is copied.
