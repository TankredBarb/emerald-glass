# Development

Run commands from the repository root. See the README for dependencies and
supported versions. The effect source in `vendor/` includes local changes and
must not be replaced by an unmodified upstream checkout.

## Build and check

The complete build and validation path, without system installation:

```sh
./install.sh --dry-run
```

For separate native development builds:

```sh
cmake -S . -B build/dev -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_KWIN_DECORATION=ON -DBUILD_TESTING=ON
cmake --build build/dev --parallel 8
ctest --test-dir build/dev --output-on-failure
```

Build the companion effect and validate its GLSL:

```sh
cmake -S vendor/kwin-effects-glass -B build/effect -G Ninja \
  -DGLASS_WAYLAND=ON -DGLASS_X11=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/effect --parallel 8
python3 validate_shaders.py
```

EGL/epoxy enable the material-shader and header-optics tests; without them
those tests are omitted. Check the CTest list when comparing validation runs.
The tested configuration runs six tests.

## Previews

```sh
build/dev/emerald-glass-preview
build/dev/emerald-glass-preview --material-preview
build/dev/emerald-glass-preview --material-preview --verify-preview --blur 6
build/dev/emerald-glass-preview --kcm-preview --verify-preview --blur 6
build/dev/emerald-glass-preview --sheet-comparison
```

The material preview shares the production GLSL and blur stages. The sheet
comparison retains the previous material for reference. The standalone demo
and the root `preview-*.png` images are development references, not live KWin
screenshots or evidence of current desktop integration.

To capture the actual KDE decoration card using the freshly built plugin:

```sh
build/dev/native/decoration-card-test \
  build/dev/native/org.tankred.emeraldglass.so \
  --capture /tmp/emerald-decoration-card.png --require-gpu
```

Capture mode reads settings without saving them. The automatic card test uses
isolated settings and Qt Quick's software backend; `--require-gpu` additionally
requires successful GPU material rendering in a desktop session.

## Source map

- `native/decoration.cpp`: native title, frame, button layout and paint caches.
- `native/material.h`: shared material settings and CPU rendering.
- `native/kcm.cpp`: decoration settings UI.
- `native/materialpreview.*`: shared offscreen GPU/CPU preview renderer.
- `preview/glass.frag`: current Liquid Glass sheet material.
- `vendor/kwin-effects-glass/src/blur.cpp`: border-scoped KWin effect.
- `tests/`: config, shader, native decoration and preview regression coverage,
  plus diagnostic probes.

The effect owns only our decoration frame. Client content, other decorations,
menus and panels remain the responsibility of standard KWin Blur. Preserve
that boundary when changing rendering or effect lifecycle behavior.

## Validation notes

[CODE_REVIEW.md](CODE_REVIEW.md) records the original review;
[REVIEW_FIXES.md](REVIEW_FIXES.md) records the fixes and CPU resize measurements.
Theme-switch blur recovery and resize smoothness were subsequently confirmed
in the user's live development session on 2026-10-20. This is session validation,
not proof across all KWin versions or hardware.

The GL failure regression exercises headless/context failure and repeated
fallback calls; it is not a hardware-driver shader failure test. CPU resize
probe timings exclude GPU blur, compositor presentation and client frame pacing.

The desktop-backdrop option has automated coverage for configuration, its KCM
checkbox, defaults, activation and full/partial maximization. The Wayland effect
renders current desktop surfaces directly into the existing per-window/per-view
blur cache, using its logical coordinates and the output color description.
Desktop damage repaints only the active maximized frame intersecting that desktop
surface, excluding other desktops/activities; source changes schedule a full
repaint. Without a desktop surface, normal scene capture is used. During a
transformation the production shader renders an sRGB material snapshot once,
which the native decoration carries through the animation without standard Blur.
The snapshot excludes caption and native cursor halo, which remain native.

The user confirmed the ordinary desktop-backdrop behavior in a live session.
Follow-up tests independently exercise activation and maximization, optical and
non-optical snapshot painting, client exclusion and fallback/unload. Deliberately
disconnecting either state handler makes the regression fail; the previous
plugin fails the snapshot regression. GPU snapshot capture and animation still
need live verification after installing these fixes, including mixed output
scales, wallpaper changes and desktop/activity switches.

Historical design investigations are kept in [RESEARCH.md](../RESEARCH.md) and
[LIQUID_GLASS_REDESIGN.md](LIQUID_GLASS_REDESIGN.md). Local temporary paths and
older build commands in these records describe the original experiments.
