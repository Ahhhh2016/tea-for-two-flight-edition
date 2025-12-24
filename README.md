# Tea for Two – Flight Edition
First-person view flying over different scenes, including a cartoon space, with player’s mouse+key input controlling flying direction and speed.

## Features
- Perlin Noise for terrain and water shaping
- Shadow Mapping (depth-based shadows with PCF)
- Post-Processing Pipeline (ordered, single-FBO chain)
- Stylized Filters (toon, edge outlines, color grading)
- Portals (view-to-view rendering)
- Realtime Fog (depth-aware, exponential/height fog)
- Screen Space Motion Blur (velocity-based blur)

## Design Choices
- Modular render pipeline: geometry → shadows → post-processing, keeping passes decoupled and easy to extend.
- GLSL-centric effects: parameters are shader-driven so visuals are tunable without restructuring code.
- Perlin noise: tileable noise to avoid seams; used to perturb height/normal for natural variation.
- Shadow mapping: light-space depth map with PCF sampling; bias tuned to balance acne vs peter-panning.
- Post chain: ping-pong framebuffers to minimize bandwidth and keep effect order deterministic.
- Portals: stencil-masked secondary view; recursion depth intentionally capped to prevent feedback loops.
- Motion blur: screen-space velocity from previous-frame matrices; exposure controls streak length.
- Fog: composed in post from scene depth for stable results independent of scene complexity.

## Known Issues
- Shadows can exhibit minor acne/peter-panning at grazing angles depending on bias.
- Portal edges may show slight bleeding; recursion limited to one level by design.
- Motion blur can ghost during very fast rotations and may smear UI overlays.
- Transparent objects can show sorting artifacts with fog and certain filters.
- High resolutions and heavy blur filters can reduce FPS on integrated GPUs.
- Platform/driver precision differences (especially on macOS OpenGL) may cause subtle visual variance.
