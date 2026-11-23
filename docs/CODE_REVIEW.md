# Code review — 2026-10-20

This is the original review snapshot. Subsequent fixes, regression results and
the resize investigation are documented in [REVIEW_FIXES.md](REVIEW_FIXES.md).

The review covered the custom decoration, CPU material, shared GLSL, KCM, both
preview implementations, integration changes to the vendored KWin effect,
installer, CMake, tests and documentation. Implementation sources were not
modified during the review. Temporary probes are in `/tmp/emerald-full-review`.

P1 means fix before the first release; P2 means a functional defect to fix during
release preparation; P3 means a minor UI defect. An observed symptom without a
proven cause is listed separately from confirmed code defects.

## 1. P1: Liquid Glass loses cached backdrop alpha

Location: [preview/glass.frag](../preview/glass.frag), lines 71–73, 243–249,
312–317.

The non-LG branch preserves the Kawase result's RGBA. The LG branch takes only
RGB from the premultiplied texture, treats it as straight color and outputs
alpha equal to frame coverage. Missing cache coverage becomes an opaque dark
contribution. This is the same class of defect already addressed by the fix
and stacking test in the `opticsDisabled` branch.

Reproduced with the actual production GLSL through EGL/llvmpipe. The same
backdrop color `(0.3, 0.55, 0.25)` was supplied with different premultiplied
coverage. Opacity was 20%, Tint 0%, lighting and adaptation disabled; measurements
were taken in the flat middle section of the titlebar:

| Cache alpha | Output RGBA, 8-bit |
| --- | --- |
| 1.00 | 72, 124, 63, 255 |
| 0.75 | 57, 96, 50, 255 |
| 0.00 | 11, 12, 12, 255 |

This proves incorrect handling of partially populated caches in LG. The probe
does not measure how frequently such caches occur in the live compositor.
Do not confuse this defect with the expected influence of the selected color
and Base Opacity when stacking fully populated materials.

The fix must reconcile light transmission, material coverage and backdrop
alpha, while preserving backdrop exclusion at Opacity 100% and avoiding a
second sharp sampler that bypasses blur. Partial-coverage testing is also
required with LG, including partial and full opacity.

## 2. P2: another effect's filters can disable our material without fallback

Locations: [blur.kcfg](../vendor/kwin-effects-glass/src/blur.kcfg), line 7;
[blur.cpp](../vendor/kwin-effects-glass/src/blur.cpp), lines 336–337,
413–414, 941–954; [decoration.cpp](../native/decoration.cpp), lines 50–54,
479–481.

The effect still reads `Effect-blurplus` and its whitelist/blacklist. Old
BlurPlus settings therefore affect Emerald Glass even though its own settings
panel does not expose those filters. When a window is excluded, `shouldBlur()`
returns false, but `egShader` has already been set to true. The decoration
disables standard blur and, with LG, its own CPU material. Text and button
glyphs remain without the expected glass.

A probe uses the actual `settings.cpp` and generated `BlurConfig`: an
`Effect-blurplus` group with `BlurMatching=true` and
`WindowClasses=org.kde.dolphin` enables the whitelist and excludes
`systemsettings`. No user settings were changed. The handoff consequence
follows from the code branches above; this configuration was not reproduced
in the running KWin session.

Isolate our settings and remove hidden filters from our border rendering path.
If skipping rendering is supported, the handoff must restore CPU/standard Blur
rather than leave `egShader=true`.

## 3. P2: material ignores overall window opacity on KWin 6.7.5

Location: [blur.cpp](../vendor/kwin-effects-glass/src/blur.cpp), line 1086 and
lines 1361–1362; final application in
[onscreen_rounded.glsl](../vendor/kwin-effects-glass/src/shaders/onscreen_rounded.glsl),
line 22.

Modulation uses only `data.opacity()`. In the installed KWin 6.7.5, this is an
effect multiplier that defaults to 1; overall window opacity is stored
separately. Setting window opacity through KWin can therefore leave the LG
frame fully opaque while the client and native glyphs are already translucent.
For example, with window opacity 0.5 and data.opacity 1, our multiplier is 1,
whereas standard Blur uses 0.25.

Checked against the API and sources of the installed version:
[WindowPaintData](https://raw.githubusercontent.com/KDE/kwin/v6.7.5/src/effect/effect.cpp),
[paint data creation](https://raw.githubusercontent.com/KDE/kwin/v6.7.5/src/scene/workspacescene.cpp),
[standard Blur](https://raw.githubusercontent.com/KDE/kwin/v6.7.5/src/plugins/blur/blur.cpp).
No desktop experiment changing a user window's opacity was performed.

Account for window opacity according to the supported KWin version; do not
transfer the formula between API versions without verification.

## 4. P2: inactive-window CPU buttons use Active tint

Locations: [decoration.cpp](../native/decoration.cpp), lines 644–653;
[material.h](../native/material.h), lines 530 and 536.

The call supplies the correct active/inactive accent, but the button renderer
reads `cfg.tint` without receiving the window's current tint strength.
Inactive tint therefore does not control inactive-window buttons, while Active
tint unexpectedly does. This affects disabled LG, an unloaded effect and CPU
snapshots during window transformations. The shader path already selects
`egTint` correctly, so material also changes when switching from GPU to CPU.

Reproduced with the actual decoration plugin and a test KDecoration bridge:
for an inactive window, Inactive tint remained at 0% while Active tint changed
from 0 to 200%. This changed 612 pixels inside the button rectangle; its
background and frame used the same inactive accent. Pass the current tint
strength to the button renderer, as is already done for the frame.

## 5. P2: partial GL initialization failure can replace material with the backdrop image

Locations: [materialpreview.cpp](../native/materialpreview.cpp), lines 87–91,
132–145, 283–285; [decoration.cpp](../native/decoration.cpp), lines 50–54,
326–339.

`m_initialized` becomes true before initialization completes. If the VAO is
created but a shader program fails to compile, the first call returns an empty
image. The next call skips initializeGL, passes the VAO check and returns a
nonempty demonstration backdrop with `gpu=false`.

For the grid card, `singleFrame` returns before CPU compositing. The decoration
treats any nonempty `m_previewImage` as ready shader material and disables its
CPU layer. After a compilation failure followed by a size/settings change, the
card can therefore show the backdrop itself instead of material and button
lenses. `egPreviewMaterial` and `--require-gpu` also incorrectly treat this
result as successful GPU rendering.

This is a confirmed analysis of the failure path, not hardware GL fault
injection. Distinguish successful initialization from an attempt, return an
explicit material-ready status and retain CPU fallback on incomplete
initialization.

## 6. P3: a saved zero leaves the slider's numeric label empty

Location: [kcm.cpp](../native/kcm.cpp), lines 276–281.

The label is populated only on `valueChanged`. If the saved value equals the
slider's initial value, for example Blur=0, loading does not emit the signal.
The result shows `Blurriness:` without `0 / 15`. The same applies to other
controls with a lower bound of 0. The value and the control itself still work.

Reproduced by loading the actual KCM with isolated configuration: Blur=0, with
the group's labels containing an empty string and `Blurriness:`. Initialize the
label with the current value when creating the row.

## Separate unresolved defect: blur recovers only after movement

The user reproduced this in Window Decorations:

1. Select Layan and click Apply.
2. Select Emerald Glass again and click Apply.
3. The titlebar lacks the expected blur/material appearance.
4. A small movement or resize restores the result.

This is a release-blocking P1 scenario, but its specific cause has not yet been
established. It is not automatically considered a consequence of item 1.

The current runtime D-Bus check shows:

```text
owned=1 shaderFrames=1 fallbackFrames=0 foreignTracked=0
clientOverlaps=0 rendered=1 blurStrength=7 transformedFrames=0
backdropSaturation=1.73
```

This snapshot confirms frame registration and buffer creation. It does not
prove complete/fresh contents and was not synchronized with a specific bad
frame. Configuration: LG enabled, Blur=7, Base Opacity=32%.

Two areas need targeted tracing:

- [blur.cpp](../vendor/kwin-effects-glass/src/blur.cpp), lines 379–415,
  437–452, 500–523: the region is restricted by the current `contentsRect`
  during decoration events. External geometry is monitored, but client
  geometry changes are not tracked separately. Under Wayland, decoration-state
  application and geometry updates occur at different commit stages. Capture
  which snapshot reaches `data.frame` after the theme switch.
- The same file, lines 1147–1191: new textures are cleared, then only the dirty
  region is copied. Establish whether a new cache receives a full initial
  repaint of the scene beneath the frame after a decoration switch. Existing
  textures do not prove a valid backdrop. Do not fix this by unconditionally
  reading the whole framebuffer outside the dirty region: that can capture the
  titlebar itself and reintroduce feedback.

Next diagnostic step: correlate the decoration identity, its mask, contents/frame
geometry, shouldBlur rejection reasons, cache creation and actual copied regions
before and after movement. Once the cause is established, add a decoration-switch
regression scenario without an extra resize.

The stages were checked against official KWin 6.7.5 sources:
[decoration-state application](https://raw.githubusercontent.com/KDE/kwin/v6.7.5/src/xdgshellwindow.cpp),
[client/frame geometry notifications](https://raw.githubusercontent.com/KDE/kwin/v6.7.5/src/waylandwindow.cpp),
[EffectWindow::contentsRect](https://raw.githubusercontent.com/KDE/kwin/v6.7.5/src/effect/effectwindow.cpp).
These provide a basis for diagnosis, not proof of event ordering in the failing
user session.

## Other findings before publication

- The KCM material preview always draws three buttons on the right and does not
  use `useSystemButtons`; alignment modes 1 and 2 look identical.
  [materialpreview.cpp](../native/materialpreview.cpp), lines 99–102,
  233–235, 316–330. Changing these settings does not provide a reliable geometry
  preview. The actual KDE grid card uses native geometry and is a separate,
  more accurate path.
- The workspace's root `.git` is an empty read-only placeholder; there is no
  complete root Git repository here. This is a limitation of the environment
  at the time of the review; the baseline and completeness of a future release
  diff cannot be verified. `vendor/kwin-effects-glass` has a separate Git
  checkout with five modified files. When preparing the repository, explicitly
  choose vendoring or a submodule that preserves these changes: a gitlink to
  the original upstream commit does not contain them.
- The README does not replace a release installation check from a clean
  checkout. Verify the complete dependency set, decoration selection, enabling
  our effect while retaining standard Blur, and removal/rollback. The current
  installer atomically replaces each .so inode, but does not update all three
  plugins in one transaction.
- `RESEARCH.md` describes the old channel/Snell profile as the current frame
  architecture. The current sheet model is documented in the README and
  `LIQUID_GLASS_REDESIGN.md`; align the architecture descriptions before
  publication. Historical results can be retained with an explicit status.

## Completed checks and limits

| Check | Result |
| --- | --- |
| Native/preview build, `build/blur-control` | Passed |
| Effect build, `build/glass-blur-control` | Passed |
| CTest | 4/4: material-shader, header-optics, decoration-card, decoration-scope |
| `validate_shaders.py` | 6 stages, 4 linked programs, passed |
| `git diff --check` in the modified vendor checkout | Passed |
| `bash -n install.sh`, Python syntax | Passed |
| Additional LG partial-alpha probe | Reproduces item 1 |
| BlurPlus settings through the production config reader | Reproduce the hidden whitelist |
| Inactive CPU button in the actual decoration plugin | Reproduces item 4 |
| Zero value in the actual KCM | Reproduces item 6 |
| Running KWin through read-only D-Bus | Emerald Glass decoration, Emerald Glass and Blur effects active |

EGL checks ran on llvmpipe. The automated grid-card test uses the actual KDE
PreviewBridge, but software Qt Quick, and does not require successful GPU
material rendering. The current 4/4 do not test the complete BlurEffect in the
compositor, the Layan → Emerald transition, actual backdrop-cache validity,
hardware GPU, multiple scales/displays or real desktop frame cost.

Starting an isolated compositor inside the sandbox failed because creating a
new D-Bus socket was forbidden. This did not prevent read-only queries to the
existing user session through separately permitted qdbus6 commands.

For acceptance, first resolve the decoration-switch scenario and LG alpha loss,
then the parameter and fallback defects. Passing existing tests alone is not
sufficient evidence that these findings have been fixed.
