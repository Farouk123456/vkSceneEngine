# vkSceneEngine

A from-scratch C++/Vulkan rendering engine built as a personal showcase project. it's structured around a **layer/scene stack** so multiple independent scenes (UI demos, simulations, and eventually a raytracer) can live side by side and be swapped between at runtime.


### Why this exists

This project is meant to demonstrate hands-on understanding of modern Vulkan and real-time rendering concepts rather than just wrapping a high-level engine. Almost everything is hand-rolled: memory management via VMA, synchronization with `VK_KHR_synchronization2`, dynamic rendering (no render passes/framebuffers), a custom text/UI system with proper text shaping, and a compute-driven simulation.

## Features

### Core / Renderer
- **Vulkan 1.4**
- **`VK_KHR_synchronization2`** used throughout for barriers and submits 
- **VMA (Vulkan Memory Allocator)** for all image/buffer allocations
- MSAA support with automatic max-sample-count detection
- Descriptor indexing / bindless-style texture arrays (runtime descriptor arrays, non-uniform indexing)
- Offscreen framebuffer-per-frame rendering (`VK_IMAGE_LAYOUT_GENERAL` storage-capable target) that gets blitted/copied into the swapchain image — lets compute shaders write directly into what gets displayed
- **Multi-window support**: windows are updated and recorded in parallel 

### Scene / Layer System
- `Layer` interface
- `LayerStack`: an ordered set of layers with per-layer update frequency, draw/update toggles, and re-ordering (move front/back/forward/backward)
- `LayerEventHandler`: simple event queue for inter-layer communication (`INTERLAYER_EVENT`, semaphore registration, stack switching, etc.)
- `LayerHandler` manages one or more `LayerStack`s per window and switches between them (e.g. `Tab` to flip between scenes)
- Built-in debug overlay layer showing render FPS / frame time and main-loop FPS

### Text Rendering
A fairly complete text pipeline, not just bitmap font blitting:
- **FreeType** for glyph rasterization (+ `plutosvg` hooks for color/SVG glyph support, e.g. emoji)
- **raqm** for proper text shaping (ligatures, complex scripts) and bidi/RTL paragraph direction detection
- Font **fallback chains** — glyphs missing from the primary font are resolved through an ordered fallback list, mapped in per-run "font fallback runs"
- Dynamic **glyph atlas** packing via `stb_rect_pack`, atlases grow automatically and old atlases stay valid (append-only pages)
- Word-wrapping, alignment (left/center/right/auto+RTL), and **ellipsizing** (`…` or last-word truncation) with binary-search-based line fitting for O(log n) width fits
- Aggressive caching to keep this cheap at 60+ fps:
  - cumulative glyph-width cache
  - shaped-glyph cache
  - generated-vertex cache
  - full paragraph wrap-result cache
  - fallback font resolution cache
- Instanced GPU text rendering — one draw call for all queued glyphs per frame via per-glyph vertex + per-line transform SSBO

## Architecture at a glance

```
GLFWHandler
 └─ Window(s) ── VulkanHandler (device/instance/queues, physical device scoring)
      └─ LayerHandler
           └─ LayerStack[]
                └─ Layer[]  (TradRenderer, WaveSim, WaveSimController, DebugLayer, ...)
```

- `AssetManager` owns textures, fonts, and the glyph atlas pages, and is shared across all layers via `WindowRescources`.
- `TextDrawer` is a self-contained subsystem (its own pipeline, buffers, caches) that any layer can call into (`addLine`, `addParagraph`, `preLoadText`, `getTextMetrics`) and then flush once per frame with `writeToGPU()` / `DrawCallInRenderPass()`.
- Each window records and submits its own command buffer, so multi-window rendering scales without cross-window stalls.

## Building

```bash
#!/bin/bash
#in root of project
make -j8
./Solver
```

**Dependencies**
- Vulkan SDK (1.4+, validation layers optional via `settings::validationLayer`)
- GLFW
- [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) (header-only, vendored)
- FreeType2 (+ [plutosvg](https://github.com/sammycage/plutosvg) for color glyph support)
- [raqm](https://github.com/HOST-Oman/libraqm) (text shaping)
- stb_image / stb_image_write / stb_rect_pack (vendored)
- GLM


Shaders are automatically compiled to `.spv` and expected under `shaders/` (e.g. `text_vert.spv`, `wave_comp.spv`); fonts are auto-loaded from a `fonts/` directory at startup.

```bash
make shaders #to compile only the shaders
```


## Controls
- `Tab` — switch between scene stacks
- `` ` `` (backtick) — toggle debug FPS overlay
- `Home` — dump FPS/frametime to log
- `Esc` — quit window


Contributions/suggestions welcome, but this is primarily built to learn and demonstrate Vulkan/graphics-engine fundamentals end-to-end.