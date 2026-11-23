# Review fixes and resize investigation — 2026-10-20

The original findings are preserved in [CODE_REVIEW.md](CODE_REVIEW.md). This
document describes subsequent changes and the limits of their verification.

## Live acceptance

On 2026-10-20, after installation, the user confirmed that the decoration-switch
bug was gone and resizing looked smooth. This verifies the current desktop
session, not all GPUs, KWin versions or overall window-opacity settings.

## Fixes

| Finding | Change | Verification |
| --- | --- | --- |
| LG turns missing cache coverage into opaque dark color | Separated straight RGB, cache coverage and opaque material contribution; spectral samples retain alpha | Production GLSL: partial/empty-cache compositing at opacity 0/20/80/100%, including buttons; opacity 100% excludes the backdrop |
| Foreign BlurPlus settings affect our frames | Dedicated `Effect-emeraldglass` group; whitelist/blacklist excluded from our border path | Production config reader with a foreign whitelist and BlurFinetune |
| Overall window opacity is ignored | Modulation accounts for `w->opacity() * data.opacity()`, following standard KWin | Build on KWin 6.7.5; desktop opacity verification remains separate |
| CPU buttons use Active tint for inactive windows | Renderer receives the window's current tint strength | Actual decoration plugin: inactive appearance independent of Active tint; hover cache invalidation |
| Partial GL failure reports false readiness | Separate successful-initialization flag; card retains native fallback, KCM retains CPU preview | Repeated calls after failure; CPU fallback in headless Qt |
| Empty labels for zero-valued sliders | Numeric label initialized immediately | Actual KCM: saved Blur=0 displays `0 / 15` |
| Inaccurate geometry preview | KCM respects left/right system layouts, spacers and the semantic Close button; full centering differs from centering between button groups | CPU preview compared across layout/alignment changes |

For a normal opaque backdrop, the LG color, lighting and optics formulas are
preserved. Alpha changes concern incomplete cache coverage. No additional raw
sampler was introduced.

## Switching Layan → Emerald Glass

Lifecycle changes address the identified risks:

- Disconnect the previous decoration's connections and event filter on a switch.
- Reread the region after commit and on client-geometry changes, even when the
  external geometry remains unchanged.
- Request a full scene repaint when switching to our frame and first creating
  its backdrop cache.
- Do not repeat that full repaint for every size during resizing.
- Keep framebuffer copying restricted to the actual dirty region.

D-Bus debugging now includes `regionChanges`, `cacheInitializations` and detailed
geometry for the `frames` parameter:

```sh
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.debug emeraldglass frames
```

The build passed. After installation, the user tested decoration switching and
confirmed that the bug was gone. This live acceptance supplements automated
tests; no universal check of all compositor/GPU configurations was performed.

## Why resizing was expensive

The CPU path recomputed SDF, gradients, powers and exponentials across the entire
frame for every size. Its cache image covered the whole window, including the
transparent client area. This path is used with LG/the effect disabled and for
native material during window transformations.

A compact sample of corners and edges is now cached. Unchanged straight sections
are stretched; the client area is not painted. At fractional DPR, the phase of
the right/bottom edge relative to physical pixels is preserved. Small windows
that cannot use stretching fall back to the full SDF. Buttons also retain their
rendered lens until material, state, scale or pointer position changes;
unrelated repaint regions are skipped.

The GPU path no longer allocates unused texture levels or expands damage based
on unused client/dock settings. The GPU allocation strategy was not fully
redesigned: backdrop texture sizes still change with the window.

## Measurements

`decoration-resize-probe` loads the actual KDecoration3 plugin with a test
DecoratedWindow, changes the client size and paints four strips like KWin's
decoration renderer. The scenario uses six buttons, an initial 1280×720 client,
four warm-up frames and 40 measured frames. Configuration is isolated from user
settings. The timer includes geometry changes, strip creation and CPU painting.

| Decoration/path | DPR 1: p50 / p95, ms | DPR 2: p50 / p95, ms |
| --- | --- | --- |
| Emerald before the fix, CPU | 10.89 / 11.88 | 42.82 / 46.61 |
| Emerald after the fix, CPU | 0.42 / 0.44 | 0.62 / 0.64 |
| Emerald after the fix, transformed | 0.42 / 0.44 | 0.61 / 0.63 |
| Emerald, GPU material: native CPU portion only | 0.37 / 0.39 | 0.41 / 0.42 |
| Klassy, same test client | 0.21 / 0.22 | 0.30 / 0.31 |

These measure CPU decoration cost, not desktop FPS. They exclude actual GPU
blur, buffer uploads to the compositor, client synchronization, VRR,
presentation queuing and perceived smoothness. Nevertheless, the original CPU
path alone exceeded the 144 Hz frame budget even at DPR 1 and is a confirmed
source of load.

To reproduce after building:

```sh
cmake --build build/blur-control --target decoration-resize-probe
QT_QPA_PLATFORM=offscreen DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  build/blur-control/native/decoration-resize-probe \
  "$PWD/build/blur-control/native/org.tankred.emeraldglass.so" cpu
# Other modes: transformed, gpu.
# For comparison, pass the absolute path to the installed Klassy plugin.
```

Comparison with full SDF rendering is tested automatically for several
geometries and DPR 1/1.25/1.5/2, excluding caption/button glyphs. The permitted
premultiplied RGBA channel difference is at most 2/255.

## Checks and subsequent acceptance

Both builds and offline GLSL checks pass. CTest: 6/6, including new settings
isolation and graphics-preview failure checks. The new regressions separately
reproduced the defects in the previous shader and previously installed native
plugin before verifying the fixed version.

After installation, log out and back in so KWin loads the new native decoration.
For a live resize comparison, use one application and identical sizes with
Klassy and Emerald; check LG on/off and transformations separately, including
fractional scaling. Test the decoration-switch scenario without moving the
window first. Hardware GPU frame time remains a separate measurement stage if
a difference persists after the CPU issue is removed.

## Crystal clipping and Close hover

With the user's current settings (Crystal, opacity 32%, tint 102%, specular
58%, glow 88%, refraction 45%, dispersion 60%), several white highlights and
hover contributions combined into overexposure. For light pastel materials,
the white rim contribution, top highlight and hover were reduced. Close uses
the same limit across all presets while retaining a contrasting signal color.
Only CPU/GPU button materials changed; geometry and frame material are preserved.

For golden Amber, Close receives a cool blue accent on hover/press: red blended
into the warm material. CPU and GLSL use the same rule for golden hues;
red/pink hues retain a gold signal, and other hues retain red.

After comparison with `--close-comparison`, variant 3 was selected: colored
depth. In the main GLSL and CPU fallback, fill was reduced from 75% to 35% and
increases smoothly from the edge into the lens. The rim and reflections use
80% theme color and 20% signal color; highlights were restored to the selected
sample's level. Comparison mode retains the previous material in row 0 and the
selected material in row 3.

In GLSL, button color is now clamped before coverage premultiplication. This
prevents a bright red signal from overpowering rounded-edge antialiasing.
Against a controlled teal backdrop, the share of pixels with red >= 254 inside
the checked Close body fell from 22–30% to zero across all eight presets; white
pixels (minimum RGB channel > 205) in Crystal hover fell from 32 to zero.

The regression reproduces the previous clipping with the saved old shader and
passes after the fix. Hover/press were checked across all presets at scale
1/1.25/1.5/2, along with lens boundaries, premultiplied alpha and Crystal CPU
fallback. Builds, 6/6 CTest and GLSL validation pass. Comparison images are
material renders through EGL/llvmpipe, not live KWin screenshots; final visual
acceptance remains after installation and logging back in.
