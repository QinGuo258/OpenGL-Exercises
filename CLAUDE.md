# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

CMake + Ninja + MSVC (cl.exe), configured via CMakePresets.json. Four presets: `x64-debug`, `x64-release`, `x86-debug`, `x86-release`.

```bash
# Configure
cmake --preset x64-debug

# Build
cmake --build out/build/x64-debug

# Run
./out/build/x64-debug/OpenGL/OpenGL.exe
```

Project requires OpenGL 4.6 Core Profile, C++20. MSVC needs `/utf-8` flag (configured in `OpenGL/CMakeLists.txt`) — source files contain Chinese comments.

No test infrastructure.

CMake POST_BUILD copies `shaders/`, `models/`, `textures/`, `audio/`, and `fonts/` directories to the exe directory. New files in any of these directories are auto-copied.

**⚠️ Build Issues**:

- **assimp CRT mismatch** (linker errors about `operator new`): Delete CMake cache and reconfigure:
  ```bash
  rm -rf out/build/x64-debug/CMakeCache.txt out/build/x64-debug/CMakeFiles
  cmake --preset x64-debug
  cmake --build out/build/x64-debug
  ```

## Runtime Controls

| Key | Action |
|-----|--------|
| WASD | Move player |
| Mouse | Rotate camera |
| Space | Jump |
| L-Ctrl | Toggle sprint/walk mode (persistent toggle, not hold). Sprint only activates when **holding W** (forward); S/A/D or backward movement stays at walk speed |
| L-Shift | Sneak (hold) |
| L-Shift | Sneak (hold) |
| Left Mouse | Attack + combat raycast (1st-person: arm attack; 3rd-person: player attack) |
| Right Mouse | Shoot arrow (25m/s velocity, from player eyes); **or hold to eat** when slot 3 (bread) is selected and HP < 20 |
| Scroll Wheel | Cycle hotbar active slot (0–8) |
| Keys 1–9 | Select hotbar slot directly |
| F1 | Hot-reload sky shader from source directory |
| F2 | Open time-of-day quick-select panel (4 buttons: 正午/黄昏/午夜/清晨 around screen center). Click a button to jump to that time. Mouse unlocked while open; mutually exclusive with T chat history |
| F5 | Cycle camera mode (FirstPerson / ThirdPersonBack / ThirdPersonFront) |
| F6 | Toggle collision debug wireframe |
| R | Toggle rain on/off (2s smooth transition) |
| F3 | Toggle debug overlay (XYZ coordinates + FPS, top-left) |
| T | Toggle chat history overlay (shows last 12 messages; locks mouse & movement while open) |
| Esc | Quit |

## Source File Architecture

`src/` directory:

- **main.cpp** — Entry point: GLFW/GLAD init, window 1920×1080, 8 FBOs (shadow, rain depth, G-Buffer, SSAO, SSAO blur, HDR, Bloom ping-pong ×2), skybox VAO, rain VAO (VBO-less), uiVAO (2D UI), fullscreenVAO, Player, arm Model+Animator, ThirdPersonCamera, CollisionWorld, hotbar models+icons, all Shader objects, audio engine, enemy management, arrow container, particle systems, full multi-pass render loop. Also contains: **GameState** (MENU/PLAYING), **StoryState** (6-stage scripted story), chat/message system, player HP/hearts, eating mechanic, death smoke & sweep particles.
- **Audio.cpp** — miniaudio implementation unit (`#define MINIAUDIO_IMPLEMENTATION` + `#include "miniaudio.h"`). Isolated from main.cpp to avoid recompiling the large audio library on every change.
- **FontRenderer.h/.cpp** — UTF-8 TrueType font rendering: bakes ASCII (32–126), CJK punctuation (0x3000–0x303F), and CJK Unified (0x4E00–0x9FFF) into an 8192×8192 GL_RED atlas via `stb_truetype.h`. `RenderText()` handles UTF-8 → codepoint decoding, per-character quad generation with atlas UV offset/scale, and shader state fencing (uIsFont, uUvScale, uUvOffset).
- **font_baker.h/.c** — C wrapper around `stb_truetype.h` for font atlas baking. Compiled as C (not C++) via `set_source_files_properties`. Calls `stbtt_PackFontRanges` for 3 Unicode ranges (95 + 64 + 20992 chars = 21151 total).
- **stb_truetype.h** — Single-header TrueType font rasterizer (stb-style, `STB_TRUETYPE_IMPLEMENTATION` defined in `font_baker.c`).
- **Arrow.h/.cpp** — Parabolic arrow projectile: gravity, `CollisionWorld::Raycast` wall collision, player hit detection (distance < 1.0m, height in body range → HP -2 + immunity), stuck-in-wall state, 10s lifetime. Right-click to fire from player eyes.
- **CollisionWorld.h/.cpp** — CPU-side 3D collision: triangle extraction with model-matrix transform, **plants (materialType==1) are skipped during extraction**, 2D spatial hash grid (cell size 5.0), raycast (Möller–Trumbore via `glm/gtx/intersect.hpp`), debug line-vertex generation.
- **Player.h/.cpp** — Cylinder character controller (radius=0.4, height=1.8), 5-state FSM, animation dict, kinematic physics with wall repulsion + floor detection + fall-death reset. `Draw()` sets `uIsHit = false` to prevent hit-flash on player. Horizontal velocity damped to 10% per frame after being applied as impulse.
- **Enemy.h/.cpp** — Hostile mob with `EnemyType { ZOMBIE, SKELETON }` enum. Two AI branches, physics with floor detection + horizontal wall repulsion, skinned mesh rendering with bone matrix pre-fill, `printf`-based diagnostics. Managed via `std::vector<Enemy>` in `main.cpp`.
- **ThirdPersonCamera.h/.cpp** — 3-mode camera (FirstPerson / ThirdPersonBack / ThirdPersonFront), Pitch [-89°,89°], F5 toggle. Default FOV=45°, near=0.1, far=1000. **`SetFOV(float)`** supports runtime FOV changes (used by sprint FOV effect).
- **Animation.h/.cpp** — Skeletal animation: Bone (TRS keyframe interpolation), Animation (Assimp loading + `LoadAll` factory), Animator (dual-track base+overlay with bone-existence-based overlay override + head rotation/hide + `m_NodeGlobalTransforms` cache + `GetNodeGlobalTransform(name)`). `UpdateAnimation` uses `fmod(m_CurrentTime, duration)` → loops infinitely.
- **Model.h/.cpp** — Assimp model loading, recursive node tree → Mesh list + bone info + emissive point light extraction. **Material classification** in `ProcessMesh()` via substring matching on `AI_MATKEY_NAME`.
- **Mesh.h/.cpp** — VAO/VBO/EBO wrapper with bone-influence vertex layout (loc=0 Position, loc=1 Normal, loc=2 TexCoords, loc=3 BoneIDs, loc=4 Weights, loc=5 Color).
- **Shader.h/.cpp** — GLSL compile + uniform setters (`SetMat4`, `SetVec3`, `SetVec2`, `SetFloat`, `SetInt`, `SetBool`) + `Reload()` hot-reload; move-only. **⚠️ `CompileShader` returns 0 on failure** (`glDeleteShader` + log to stderr with shader type). Constructor and `Reload()` check for 0 and bail before linking broken programs.
- **Texture.h/.cpp** — stb_image, RGBA, Y-flip, CLAMP_TO_EDGE, GL_NEAREST filtering.
- **FPSCamera.h/.cpp**, **Camera.h/.cpp** — Unused.
- **miniaudio.h** — Single-header audio library, included from `Audio.cpp`.

**`OpenGL/OpenGL.h` and `OpenGL/OpenGL.cpp`** are VS wizard boilerplate ("Hello CMake"), not in `add_executable`, do not compile.

## Rendering Loop Order

```
Per-frame update:
  1. deltaTime, FPS counter (0.5s refresh), timeOfDay += dt * timeScale
  2. Story director (only when PLAYING): stage progression, cutscene timing, bell tolling, enemy wave spawning
  3. rainIntensity smooth interpolation toward targetRain
  4. playerImmunityTimer decrement; eating timer advance (1.5s hold → +4 HP)
  5. player death respawn (HP≤0 → reset to (-20,-7,-13))
  6. processInput(window, player, enemies)  — WASD, Ctrl sprint toggle, 1-9/scroll, T chat history, R rain, F3 debug, mouse combat + eating
  7. player.UpdatePhysics(dt, mapCollision)  — gravity, floor, wall repulsion, horizontal velocity damping
  8. Enemy update: std::erase_if by Health≤0, then enemy.UpdateAI + enemy.UpdatePhysics + enemy bite check
  9. particle lifetime + arrow physics update (mapCollision + player hit) + chatLog lifetime decay
 10. Camera occlusion handling (3rd-person only)
 11. Animation state + body/head orientation + SetHideHead(firstPerson) + player.UpdateAnimation(dt)
 12. FP arm animation update (attack timer → bind pose on completion)

Matrix computation:
 13. Dynamic FOV: 45° → 55° when sprinting (smooth interpolation via glMix, 8.0f speed)
 14. Celestial matrix: rotateZ(timeOfDay) → sunPosDir, sunY
 15. Dynamic light source: activeLightDir, lightColor, ambientColor (see Dynamic Moonlight section)
 16. Sky horizon/zenith color interpolation
 17. lightSpaceMatrix = ortho(-40,40,-40,40,1,100) * lookAt(lightPos, player, Up)

Pass 1a — Shadow Map (FBO: depthMap, 4096×4096):
 17-19. shadowShader → mapModel; shadowSkinnedShader → player(enemies (head unhide temporarily for 1st-person)

Pass 1b — Rain Occlusion Depth (FBO: rainDepthMap, 1024×1024, if rainIntensity > 0.01):
 20. shadowShader → mapModel only (no player)

Pass 1.5 — G-Buffer Geometric Pre-Pass (FBO: gBuffer, RGBA16F × 2 + DEPTH24):
 21. gBufferShader → mapModel (view-space position + normal)
 22. gBufferSkinnedShader → player, enemies, arrows, 3rd-person held items, FP arm/items

Pass 1.8 — SSAO Generation (FBO: ssaoFBO, GL_RED):
 23. ssaoShader + fullscreenVAO: reads gPosition + gNormal + noiseTexture, 16-tap hemisphere sampling

Pass 1.9 — SSAO Blur (FBO: ssaoBlurFBO, GL_RED):
 24. ssaoBlurShader + fullscreenVAO: 4×4 box blur on ssaoColorBuffer → ssaoColorBufferBlur

Pass 2 — Main Scene (FBO: hdrFBO, RGBA16F + DEPTH24):
 25. modelShader → mapModel opaque meshes only (materialType != 4, per-mesh uMaterialType set)
 26. enemies, arrows, player (3rd-person), held items — with uIsHit reset barrier after enemies
 27. Debug wireframe (if F6 toggled)
 28. Water translucent pass: shader.Use() → glEnable(BLEND) + glDepthMask(FALSE) → mapModel water meshes (materialType == 4) → glDepthMask(TRUE) + glDisable(BLEND) → reset uMaterialType=0
 29. uSsaoMap (texture unit 13) bound to ssaoColorBufferBlur → ambient *= ssao
 30. Point lights: for loop with distance > 12.0 early-out

Particles (after scene, before skybox):
 31. Death smoke + sweep particles: billboard quads via uiShader, glDepthMask(FALSE)

Pass 3 — Skybox (LEQUAL depth, Z=1.0):
 32. skyShader → unit cube VAO

Pass 4 — First-Person Arm (only in FirstPerson mode, after skybox):
 33. glClear(GL_DEPTH_BUFFER_BIT), uView=identity, arm or held item

Pass 5 — Procedural Rain (if rainIntensity > 0.01):
 34. VBO-less GL_LINES, 10000 raindrops, rain occlusion discard

Pass 6 — 2D UI (orthographic projection):
 35. uiShader, uProjection=ortho(0,1920,0,1080), draw order:
     story black-screen overlay (if blackScreenAlpha > 0) → main menu (if MENU) → hotbar → selection box → heart HUD → item icons → crosshair (FP only) → chat log (fading, bottom-left) or chat history panel (T key, semi-transparent bg) → F3 debug (XYZ + FPS)
 36. Font rendering: `FontRenderer::RenderText()` sets `uIsFont=true`, `uFontColor`, and per-character `uUvScale`/`uUvOffset` from atlas; fences back to `uIsFont=false` after. Font atlas bound to GL_TEXTURE0, uses GL_RED single-channel format.

Post-Processing:
 38. Bloom Pass 1: bright-pass extraction (threshold > 1.0) at 960×540 → pingpongColorTex[0]
 39. God Rays Pass: if sun on-screen (godrayWeight > 0), radial blur from lightScreenPos reading pingpongColorTex[0] → pingpongColorTex[1]; if active, Gaussian blur starts from [1] instead of [0]
 40. Bloom Pass 2: dual-pass Gaussian blur ping-pong (3H + 3V iterations)
 41. Bloom Pass 3: hdr_compose (scene+bloom → Reinhard → ACES → saturation boost → gamma)
```

**⚠️ Player must be constructed after `gladLoadGLLoader()`** — constructor triggers OpenGL calls.

## Dynamic Moonlight & Night Shadows

The key variable is `activeLightDir`. When `sunY > 0`: sun shines downward (`activeLightDir = -sunPosDir`). When `sunY ≤ 0`: moon takes over (`activeLightDir = sunPosDir`), `ambientColor = nightAmbient` locked.

**Current lighting parameters** (in `main.cpp` ~line 1250–1280):

| Parameter | Value | Role |
|-----------|-------|------|
| `dayLight` | `(1.3, 1.15, 0.95)` | Warm sunlight |
| `sunsetLight` | `(1.0, 0.4, 0.1)` | Orange-red sunset |
| `moonLight` | `(0.01, 0.02, 0.04)` | Very dim cool moonlight |
| `dayAmbient` | `(0.25, 0.35, 0.50)` | Cool-blue sky bounce in shadows |
| `nightAmbient` | `(0.02, 0.02, 0.06)` | Deep blue night ambient |
| `dayHorizon` | `(0.55, 0.75, 0.95)` | Sky horizon (day) |
| `dayZenith` | `(0.15, 0.35, 0.80)` | Sky zenith (day, deep blue) |
| `sunsetHorizon` | `(1.0, 0.45, 0.2)` | Sky horizon (sunset) |
| `sunsetZenith` | `(0.3, 0.1, 0.35)` | Sky zenith (sunset) |
| `nightHorizon` | `(0.05, 0.05, 0.12)` | Sky horizon (night) |
| `nightZenith` | `(0.0, 0.0, 0.02)` | Sky zenith (night, near-black) |

`lightPos = player.Position - activeLightDir * 50.0f` — shadow camera follows active light source.

## Shader Inventory

| Shader (vert/frag) | Purpose |
|--------------------|---------|
| `model.vert/frag` | Main Blinn-Phong + skeletal animation (100 bones) + **Poisson disk soft shadows** (16 taps, random rotation) + rain wetness/puddles + **water surface** (materialType==4: vertex wave displacement, noisy normal perturbation via `uPackedNoiseMap` on tex unit 12, Fresnel sky reflection with nightFade, sun glint pow-256 ×10, view-angle-driven alpha 0.4–0.9) + fresnel reflection + **atmospheric distance fog** (light blue, 0.0005 density) + wind animation (grass/leaves) + emissive materials + hit flash (`uIsHit`) + **SSAO ambient occlusion** (`uSsaoMap` on texture unit 13) + **dynamic torch point light** (slot 1 adds movable light at player position). Specular only for `uMaterialType==5` (handheld items) or wet surfaces. Requires `uniform float uTime` for water animation. |
| `shadow.vert/frag` | Static geometry depth-only render. Also reused for rain occlusion depth pass. |
| `shadow_skinned.vert` + `shadow.frag` | Skinned player/enemy model depth-only render. |
| `g_buffer.vert/frag` | Static geometry → view-space position (COLOR0, RGBA16F) + view-space normal (COLOR1, RGBA16F). Wind sway synced. |
| `g_buffer_skinned.vert` + `g_buffer.frag` | Bone-animated geometry → G-Buffer (same outputs). 4-bone weighted blending. |
| `sky.vert/frag` | Procedural skybox: gradient, stars (gridded hash anti-flicker), Minecraft-style block sun/moon, halos, **optimized volumetric clouds** (16 view steps + 2 light steps + IGN jitter for artefact-free low-step march). |
| `rain.vert/frag` | VBO-less particle rain: `gl_VertexID` → 10000 raindrops, 40m³ camera-following box, rain occlusion discard. |
| `ssao.frag` (+ `fullscreen.vert`) | 16-tap hemisphere SSAO sampling with TBN noise rotation, range check, projection to screen-space depth buffer. |
| `ssao_blur.frag` (+ `fullscreen.vert`) | 4×4 box blur on SSAO output, eliminates 4×4 noise tile high-frequency artefacts. |
| `ui.vert/frag` | 2D orthographic UI + **3D billboard particles** + **TrueType font glyphs**: unit-square TRS, `uProjection * uModel`, sprite-sheet via `uUvScale`/`uUvOffset`, `uAlpha` for fade-out. **Font mode** (`uIsFont=true`): uses `uFontColor`, atlas UV per-glyph, `GL_RED` texture. **Solid color mode** (`uUseTexture=false`): renders `uSolidColor` fill (used for black-screen overlay, menu backgrounds, chat panels). |
| `debug.vert/frag` | Green wireframe overlay for collision triangles (F6). |
| `grid.vert/frag` | Unused. |
| `fullscreen.vert` | Single fullscreen quad (NDC [-1,1]²). Vertex shader for post-processing passes. |
| `blur.frag` | Dual-mode: **bright-pass extraction** (luminance > 1.0 → keep) + **dual-pass Gaussian blur** (5-tap, 3H+3V). |
| `hdr_compose.frag` | Final tone-mapping: scene+bloom blend → Reinhard exposure → ACES filmic → **saturation boost (1.25x)** → gamma 2.4. |
| `godrays.frag` (+ `fullscreen.vert`) | Screen-space crepuscular rays: 60-step radial blur from sun screen position toward edge, exponential decay (uDecay=0.92), output stored in `pingpongColorTex[1]` then fed into Gaussian blur. |

### Shader Uniform Dependencies

- **`model.frag` requires `uniform float uTime`** — used by water noise animation. C++ sets it at Pass 2 entry (main.cpp `shader.SetFloat("uTime", glfwGetTime())`). If adding new fragment shaders with water code, ensure this uniform is declared.
- **`uLightDir` semantics**: Points in the **direction light travels** (toward ground during day, toward ground from moon at night). In `model.frag`, light incident on surface = `-normalize(uLightDir)` (points toward sky/light source). Water sun glint uses `reflect(-normalize(uLightDir), waterNorm)`.

### uIsHit State Pollution Warning

`model.frag` `uIsHit` is a sticky uniform — it persists across draws until explicitly changed. Each enemy draw may set `uIsHit = (StunTimer > 0.8f)`. **Always reset `uIsHit = false` after the enemy loop** and before drawing map geometry, arrows, player, FP arm, or water. The same applies to `uMaterialType` — reset to 0 after any block that overrides it (enemy loop, water translucency pass). Failure to fence these uniforms causes red flash on map geometry or water shader on non-water meshes.

### Shader Hot Reload (F1)

`Shader::Reload()` re-reads from source tree (prepends `SHADER_SRC_DIR` compile definition), recompiles, atomically swaps GL program. On failure, old program preserved.

**Only `sky.frag` is currently hot-reloadable** (F1 wired in main loop). All other shaders require application restart.

## HDR / Bloom / ACES Tone Mapping

| Stage | Shader | Details |
|-------|--------|---------|
| Bright-pass | `blur.frag` (`uExtract=true`) | Pixels with luminance > 1.0 kept, others discarded. Half res (960×540). |
| Gaussian blur | `blur.frag` (`uExtract=false`) | 6 iterations (3H+3V), 5-tap kernel, half-res ping-pong. |
| HDR compose | `hdr_compose.frag` | 5-step: blend bloom(×0.35) → Reinhard(exp=0.7) → ACES → **saturation(×1.25)** → gamma(2.4) |

### HDR Tuning Quick-Reference

| Parameter | File:Line | Current | Effect |
|-----------|-----------|---------|--------|
| `uBloomIntensity` | `main.cpp:1899` | `0.35` | Bloom blend strength |
| `uExposure` | `main.cpp` | `0.5` | Overall scene brightness |
| Bright-pass threshold | `blur.frag` | `1.0` | Lower = more bloom |
| Gaussian iterations | `main.cpp` | `6` | Bloom softness (3H+3V) |
| ACES `a`–`e` | `hdr_compose.frag` | 2.51/0.05/2.43/0.59/0.16 | Film-like contrast curve |
| Saturation | `hdr_compose.frag` | `1.25` | Color vibrance (1.0 = none) |
| Gamma | `hdr_compose.frag` | `2.4` | Display gamma |

### God Rays (Crepuscular Rays)

Screen-space radial blur post-processing pass, inserted between Bloom bright-pass extract and Gaussian blur.

| Parameter | File:Line | Current | Effect |
|-----------|-----------|---------|--------|
| `godrayWeight` | `main.cpp` | `0.02f` | Master multiplier; 0 disables pass entirely (sun off-screen) |
| `uDensity` | `main.cpp` | `0.95f` | Radial step distance (higher = longer rays) |
| `uDecay` | `main.cpp` | `0.92f` | Per-step exponential decay (closer to 1.0 = longer tails) |
| `NUM_SAMPLES` | `godrays.frag` | `60` | Radial sampling steps |
| `uLightScreenPos` | `main.cpp` | Computed from `activeLightDir` via VP-matrix NDC projection | Sun 2D position in [0,1]² |

**FBO routing**: God rays read from `pingpongColorTex[0]` (bright-pass output) and write to `pingpongFBO[1]` / `pingpongColorTex[1]`. When active, Gaussian blur loop starts from `blurSourceTex=1` instead of 0, so god rays also receive smoothing.

## Material Type System (`uMaterialType`)

Classified by **material name** at load time in `Model::ProcessMesh()`. Each `Mesh` carries `int materialType` (0–5).

| Type | Meaning | Keyword match | Shader behavior |
|------|---------|---------------|-----------------|
| 0 | Default (stone, wood, dirt…) | Everything else | Standard Blinn-Phong, **no specular** (unless wet) |
| 1 | Plants (non-solid) | `grass`(not block), `wheat`, `crop`, `flower`, `plant`, `fern`, `tall`, `vine` | Wind sway (root-anchored), **no collision** (skipped in CollisionWorld) |
| 2 | Leaves | `leaves` | Wind sway (uniform), no specular |
| 3 | Emissive | `lava`, `torch`, `glowstone`, `lantern` | Ignores shadow/ambient, `emission = texColor * 1.2` × 90% |
| 4 | Water surfaces | `water`, `fluid`, `ocean`, `river` | **Vertex wave animation**, noisy normal perturbation, Fresnel sky reflection, sun glint (pow 256, ×10), alpha blending (fresnel-driven 0.4–0.9). **No collision**. Two-pass rendering in Pass 2: opaque first, then translucent water with `glDepthMask(FALSE)` after all opaque geometry. |
| 5 | Handheld items | Set manually at draw time in `main.cpp` | **Retains specular highlight**, no wind sway |

Types 1, 2, 3, 4 detected automatically; type 5 is manual.

## Shadow Mapping System

- **Shadow Map**: 4096×4096 `GL_DEPTH_COMPONENT`/`GL_FLOAT`, `GL_CLAMP_TO_BORDER` (white border).
- **Soft Shadows**: **Poisson disk sampling** (16 taps) with per-pixel random rotation via `rand(gl_FragCoord.xy)`. `filterRadius = 2.5` controls softness. Defined in `model.frag`.
- **Light Space Matrix**: `ortho(-40,40,-40,40,1,100) * lookAt(lightPos, player.Position, Up)`. Light follows player (50m back along `activeLightDir`).
- Dynamic bias: `max(0.005*(1-N·L), 0.0005)`.
- Shadow texture unit: **15**. Rain depth texture unit: **14**. SSAO texture unit: **13**. Packed noise (water) texture unit: **12**.

## SSAO (Screen-Space Ambient Occlusion)

Three-pass system between shadow map and main scene:

| Pass | FBO | Shader | Output |
|------|-----|--------|--------|
| 1.5 G-Buffer | `gBuffer` (RGBA16F×2 + DEPTH24) | `g_buffer.vert/frag`, `g_buffer_skinned.vert` | View-space position + normal |
| 1.8 SSAO Gen | `ssaoFBO` (GL_RED) | `ssao.frag` + `fullscreen.vert` | Raw occlusion (16-tap hemisphere + 4×4 noise tile TBN rotation) |
| 1.9 SSAO Blur | `ssaoBlurFBO` (GL_RED) | `ssao_blur.frag` + `fullscreen.vert` | Smoothed `ssaoColorBufferBlur` |

Sample kernel: 64 vec3 generated with lerp(0.1, 1.0, scale²) bias toward hemisphere origin. Shader uses only first 16. Noise: 4×4 RGBA16F random rotation vectors, `GL_REPEAT` tiled.

In Pass 2, `model.frag` applies: `ambient *= texture(uSsaoMap, gl_FragCoord.xy / uScreenSize).r`. Only ambient is affected — diffuse, specular, and point lights are untouched.

## Combat System

### Raycast Targeting (Left Mouse)

Ray from `player.Position + camera.EyeOffset`, direction = `camera.GetFullFrontVector()` (includes pitch). Max range: 3.5m. AABB: `enemy.Position + (-0.4, 0, -0.4)` to `(+0.4, 1.8, +0.4)` via Slab algorithm.

### Damage & Knockback

| Attack | Damage | Knockback |
|--------|--------|-----------|
| Sword (slot 0) | 7 | `frontDir * 5.0 + (0, 4.5, 0)` |
| Empty hand | 1 | same |
| Sweeping edge AOE (sword only) | 3 | same |

Knockback: `Enemy::TakeDamage(damage, knockbackVec)` → `Velocity = knockback`, `IsGrounded = false`, `StunTimer = 1.0f`.

### Sweeping Edge (Sword AOE)

3m horizontal distance + 90° forward cone (dot > 0.707) around primary target when sword hits.

### Hit Flash

`model.frag` `uIsHit` → `mix(finalColor, vec3(1,0,0), 0.3)` blended BEFORE fog.
- Enemy: red flash for first 0.2s of stun (`StunTimer > 0.8f`)
- Player/Map/Arrows: always `uIsHit = false`

### Audio

| Event | Sound file |
|-------|-----------|
| Sword swing | `audio/Sweep_attack1.mp3` |
| Empty hand swing | `audio/Weak_attack1.mp3` |
| Zombie hurt/death | `audio/Zombie_hurt{1,2}.mp3` / `Zombie_death.mp3` |
| Skeleton hurt/death | `audio/Skeleton_hurt1.mp3` / `Skeleton_death.mp3` |
| Arrow hit/fire | `audio/Arrow_hit1.mp3` / `Bow_shoot.mp3` |

All sounds via `ma_engine_play_sound(&audioEngine, "audio/xxx.mp3", NULL)`.

| Story bell | `audio/Bell_use1.mp3` |

## Game State & Story System

### Main Menu → Gameplay

`GameState { MENU, PLAYING }`. App starts in MENU with cursor visible. Clicking the "开始游戏" button (centered at X:560–1360, Y:500–580 in 1080p orthographic) transitions to PLAYING, hides cursor, and triggers the first story message.

While in MENU or with chat history open, mouse rotation and player movement are locked.

### Story Director (6-stage scripted sequence)

`StoryState` enum drives a linear narrative in `main.cpp`:

| Stage | Trigger | Behavior |
|-------|---------|----------|
| `WANDERING` | Game start | After 30s: hint to go home (坐标: -17,-7,-11). When player within 4m of home → CUTSCENE |
| `CUTSCENE` | Proximity to home | Screen fades black (0–2s), 5 bell tolls at 3s intervals, at 2nd bell: forced night (`timeOfDay=4.71`/270°) + rain, dialog lines at t=11s and t=16s, screen fades back (6–8s) |
| `GOTO_GATE` | Cutscene ends | Player walks to east gate (X:47–51, Z:-9–0). On arrival: clears existing enemies, spawns 2 zombies + 2 skeletons outside gate |
| `DEFENDING` | Gate reached | Wait until all enemies defeated (`enemies.empty()`) |
| `ENDING` | All enemies dead | 3s delay → villager dialog, 6s delay → "自由探索" system message |
| `COMPLETED` | Ending done | Free exploration, no further story events |

Story state only advances when `currentGameState == PLAYING`. `timeOfDay` still rotates in MENU (skybox visible).

### Chat / Message System

- `ChatLine { text, lifeTime=5.0f }` struct. `chatLog` holds up to 6 visible lines with auto-fade (alpha = lifeTime/5.0).
- `chatHistory` holds up to 100 permanent messages.
- `AddChatMessage(msg)`: pushes to both `chatLog` and `chatHistory`.
- **T key**: toggles `isChatHistoryOpen`. When open: semi-transparent black panel shows last 12 history lines, mouse unlocked, movement locked. When closed: mouse re-captured.

## Player HP & Eating System

- **HP**: 20 (displayed as 10 hearts). 3 heart states: full, half, empty (textures in `textures/UI/heart/`).
- **Damage**: Zombie bite → HP-2 + knockback (`pushDir*5.0 + (0,3.5,0)`) + 0.5s immunity timer. Player hit flash via `playerImmunityTimer`.
- **Death**: HP ≤ 0 → respawn at `(-20, -7, -13)` with full HP, velocity reset.
- **Eating**: Slot 3 (bread) + hold right-click for 1.5s → +4 HP (capped at 20). Only works when HP < 20. Visual feedback: item shake + tilt + pull-toward-camera animation during eating. `eatingTimer` resets on right-click release or completion.

## Particle Systems

### Death Smoke (DeathParticle)

Spawned on enemy death: 10 particles with random position offset (±0.4 XZ, 0–1.5 Y), random velocity, 8-texture variant (`textures/particle/generic_0–7.png`), 0.4s lifetime. Billboard quads with scale growth (0.8 × (1.2 - life/0.4)) and alpha fade. Rendered via `uiShader` in 3D with depth write disabled.

### Sweeping Edge Slash (SweepParticle)

Spawned on sword hit: single 8-frame sprite-sheet particle (`textures/particle/sweep_0–7.png`), 0.15s lifetime, 1.8×1.8 billboard. Frame index = `floor(progress * 8)`. Spawned 1.5m in front of camera at eye level.

## Enemy System

| Member | Default | Purpose |
|--------|---------|---------|
| `Health` | 20 | Dies at ≤0 → erased from vector |
| `StunTimer` | 0 | 1.0s on hit; pauses AI during stun |
| `AttackCooldown` | 0 | 1.5s between zombie bites |
| `RangedAttackTimer` | 0 | 4s between skeleton shots |

**Zombie:** >15m idle, 1.5–15m walk pursuit, ≤1.5m bite (HP-2 + knockback + 0.5s immunity). Speed 2.5.
**Skeleton:** >15m idle, 8–15m stand+shoot (4s cooldown), <8m backward kite at 80% speed + shoot. Speed 1.8.
Skeleton overlay: base (idle/walk full-body) + overlay (attack, 2 arm bones only) via dual-track animation.

Both enemies share physics: floor detection with `fallCompensation`, horizontal wall repulsion, fall-death at Y < -50.

## Player Physics & Movement

### Physics (`UpdatePhysics`, 6 steps)

1. **Gravity**: `Velocity.y += -9.8*dt; Position += Velocity*dt; Velocity.xz *= 0.1` (impulse damping)
2. **Floor detection**: `fallCompensation = max(0, -Velocity.y * dt)` added to `searchTop = Position.y + 0.5 + fallCompensation` — prevents high-speed floor tunneling
3. **Wall repulsion**: axis-aligned vertical walls, closest-point-on-rectangle push-out, excludes `tri->maxY <= Position.y + 0.2f` (step-over tolerance)
4. **Fall-death**: `Position.y < -50` → `(0, 50, 0)`
5. **Debug print** (every 60 frames)
6. **Visual Y offset**: Sneak → `-0.08`, standing → `0.0`

### Movement Speed (`UpdateMovementState`)

| State | Speed | Condition |
|-------|-------|-----------|
| WALK | `4.0f` | Moving, not sprinting, not sneaking, not eating |
| RUN (Sprint) | `8.0f` | Moving + `IsRunningMode` + **holding W** + not sneaking + not eating |
| SNEAK_WALK | `2.0f` | Moving + `IsSneaking` |
| EATING_WALK | `1.6f` | Moving + `IsEating` (20% of sprint speed) |
| IDLE / SNEAK_IDLE | `0.0f` | Not moving |

Base speed hardcoded in `Player.cpp:213`. Jump force: `6.0f` (set in `Player.h`).

### Sprint Control

Ctrl toggles `IsRunningMode` persistently (edge-triggered flip). Sprint only activates when `IsRunningMode` + **holding W** + moving + not sneaking + not eating. Pressing S/A/D (or any combination without W) while in sprint mode stays at walk speed (4.0f) with no FOV change.

### FOV Dynamic Scale

Defined in `main.cpp` before main loop: `float currentFOV = 45.0f`. Updated each frame before lighting matrix computation:
- Sprinting (`IsRunningMode` + `isMovingForward` + not sneaking): target 55°
- Otherwise: target 45°
- `currentFOV = glm::mix(currentFOV, targetFOV, deltaTime * 8.0f)`
- Applied via `thirdPersonCamera.SetFOV(currentFOV)` — all 12+ shader call sites auto-use dynamic FOV

## CollisionWorld

- **Triangle**: world-space vertices, normal, XYZ AABB.
- **Plant & water exclusion**: `if (mesh.materialType == 1 || mesh.materialType == 4) continue;` — grass/crops/flowers and water surfaces produce no collision triangles.
- **Grid**: cell size 5.0, key = `int64_t(int32 cx, int32 cz)`.
- **Raycast**: XZ AABB → grid → `glm::intersectRayTriangle` → nearest hit. `dir` must be normalized.
- Free functions: `ClosestPointOnTriangle2D`, `TriangleHeightAtXZ`.

## Distance Fog

Atmospheric scattering fog in `model.frag`:
- **Density**: `mix(0.0005, 0.005, uRainIntensity)` — near-transparent in clear, thick in rain
- **Color**: `mix(vec3(0.6, 0.8, 0.95), vec3(0.4, 0.45, 0.5), uRainIntensity)` — sky-blue to overcast grey
- Exponential-squared falloff: `exp(-pow(dist * density, 2.0))`

## Point Light System

Emissive meshes (materialType==3) have vertices clustered (0.5m radius averaging) into `Model::pointLights`. Uploaded as `uPointLights[512]` uniform array. Each point light:
- Fire color: `(0.7, 0.45, 0.2)`, attenuation: `1/(1 + 0.20d + 0.10d²)`, cutoff: **12.0m**
- Early `continue` before `normalize`/`attenuation` for out-of-range lights

## Model Loading

- Assimp: `Triangulate | GenNormals | CalcTangentSpace` — **no FlipUVs**
- Textures: `stbi_set_flip_vertically_on_load(true)`, `desired_channels=4`, `GL_CLAMP_TO_EDGE`, `GL_NEAREST`, `GL_RGBA`
- Vertex layout: loc=0 Position, loc=1 Normal, loc=2 TexCoords, loc=3 BoneIDs (ivec4), loc=4 Weights (4 floats), loc=5 Color (RGB, R channel = wind weight)
- Embedded textures via `Texture::LoadFromMemory`

## Logging

- `gLogFile = fopen("debug.log", "w")` in `main.cpp`, shared via `extern` in Model.cpp, Animation.cpp, CollisionWorld.cpp, Player.cpp
- `LOG(fmt, ...)` macro appends newline + `fflush` — only works after fopen and before fclose
- `printf()` used in Enemy.cpp and main.cpp for runtime diagnostics

## Adding a New Source File

1. Add the `.cpp` (or `.c`) to `add_executable` in `OpenGL/CMakeLists.txt`
2. For `.c` files, add `set_source_files_properties("../src/foo.c" PROPERTIES LANGUAGE C)` to force C compilation
3. Reconfigure CMake: `cmake --preset x64-debug`
4. For new resource directories, add a `copy_directory` POST_BUILD command

## Adding a New Shader

1. Create `.vert`/`.frag` in `shaders/` (auto-copied to exe dir by POST_BUILD)
2. Instantiate `Shader` in `main.cpp` with relative paths
3. For hot-reload support, wire an F-key edge-triggered `shader.Reload()` call
