# Emerald Glass for KWin: Architecture & Research

Date: 2026-10-19. Source audit, static GLSL validation, standalone GPU prototype,
full KDecoration3 window decorator, KCModule configuration plugin, and KWin
shader effect integration are complete.

## Target Visual Direction

A translucent glass window border with emerald green, ruby red, and additional
crystal/gemstone material presets, sharp colored outer/inner contours, thin
light and dark bevels, and rich 3D glass window caption buttons. The material
preserves the transparency and depth of classic Compiz Emerald while adopting
optical techniques from Liquid Glass: Snell's-law refraction, chromatic
dispersion, adaptive backdrop contrast, geometrically aligned specular glints,
and interactive cursor response.

Active and inactive window colors are independently configurable in the KCM
settings dialog (`~/.config/emeraldglassrc`) rather than hardcoding a fixed
active/inactive color pairing in the architecture.

## Verified Local Baseline

Installed packages: `kwin 6.7.5-3.1`, `plasma-workspace 6.7.5-3.1`,
`kdecoration 6.7.5-1.1`, `qt6-base 6.12.0-2.1`, `klassy-bin 6.5.3-1`.
`kwinrc` retains `org.kde.klassy` and `blurEnabled=true` untouched unless the
user explicitly switches the active decoration in System Settings.

## Upstream Optical Base & Validation

Base optical effect reference:
https://github.com/4v3ngR/kwin-effects-glass

Pinned commit: `2ef07fa1a2a062da12289ad8de1393e01f5c1d8e` in
`vendor/kwin-effects-glass` (v1.6.3).

`validate_shaders.py` expands `sdf.glsl` from official KWin v6.7.5
(https://github.com/KDE/kwin/blob/v6.7.5/src/opengl/sdf.glsl) and validates all
six GLSL stages and four linked shader programs via `glslangValidator -l`.

## Architecture

| Component | Responsibilities |
| --- | --- |
| `KDecoration3` (`org.tankred.emeraldglass.so`) | Window geometry, titlebar, left/right `DecorationButtonGroup`s, hit-testing, cached CPU SDF frame/button glass rendering (for System Settings previews and standard KWin blur), and publishing dynamic properties to the KWin shader effect |
| KCM Settings (`kcm_emeraldglassdecoration.so`) | Embedded configuration UI in KDE System Settings -> Window Decorations with a GPU Active/Inactive preview using the effect's GLSL and shared blur profile, color presets, custom colors, Liquid Glass optics sliders, and geometry controls |
| KWin Effect (`emeraldglass.so`) | Capturing the live scene behind the window frame, Kawase blur passes, and executing the GLSL material (`preview/glass.frag`) with Snell's-law refraction, chromatic dispersion, and premultiplied corner coverage |
| Text & Vector Icons | Crisp DPI-aware rendering on top of the glass pass without self-refraction |

## Historical channel frame & button SDF geometry

The frame profile below describes the earlier channel material. The current
LG frame uses a sheet cropped to the border mask; see
[the redesign](docs/LIQUID_GLASS_REDESIGN.md). Snell-based button lenses remain.
Subsequent review fixes and resize measurements are recorded in
[REVIEW_FIXES.md](docs/REVIEW_FIXES.md).

The window frame is a rounded rectangle with a rectangular/rounded client hole:

    dFrame(p) = max(dOuter(p), -dContent(p))

1. Evaluate distance to the outer/inner edge and subpixel antialiasing coverage.
2. Compute a smooth lens shoulder, limited to half the local border thickness (up to 12 logical pixels), and its normal from the SDF gradient. The slope reaches zero before opposing edges meet, avoiding an optical seam in thin borders.
3. Refract the view ray (`refract(vec3(0, 0, -1), normal, 1.0 / 1.48)`) and intersect it with the backdrop using `ray.xy / -ray.z`. RGB dispersion varies the index of refraction for the red and blue rays. Base Opacity controls transmission of the refracted backdrop; at 100%, the surface is opaque while retaining bevel lighting.
4. Apply adaptive backdrop luminance balance, glass tint, directional edge lighting, Fresnel rim, and specular highlight (`pow(N·H, 40)`).
5. Composite the sharp outer/inner contour, dark inner lip, and polished titlebar reflection band.
6. Render individual 3D convex/concave glass lenses for up to 12 titlebar buttons (`buttonRect[12]`) with cursor-distance hover glow and pressed bevel inversion.

## Liquid Glass GNOME Reference Review (2026-10-19)

Reference: https://github.com/ryohsuke1231/liquid-glass

Reviewed commit: `b14a1cce06e37f6f90cd183329084c7e7426307b`.
The checkout used for inspection is `/tmp/emerald-liquid-glass-reference`.
This is a source and bundled-image review, not a live GNOME rendering test.
The preview experiment was evaluated and removed on 2026-10-20; the existing Emerald material is retained.

Relevant implementation paths, relative to
`liquid-glass@thinkingcoding1231.gmail.com/`:

- `shaders/glass.frag`: `profileHeight()`, `heightGradient()`,
  `getDisplacement()`, `sampleBackdrop()`, `backdropHighlight()`.
- `src/rendering/geometry.ts`: sampling reach and capture/blur margins.
- `src/rendering/blur.ts`: Gaussian/Dual Kawase passes and reuse keys.
- `src/applicationManager.ts`: per-window lighting policy.
- `src/contrastSampler.ts` and `src/adaptiveText.ts`: sampled text readability,
  separate from material brightness/contrast/saturation.

Findings relevant to our borders:

1. The lens normal comes from the derivative of a superellipse height profile,
   rather than directly assigning a slope to the distance-to-edge value. The
   reference bounds the optical band to 22 pixels, shrinking both height and
   displacement for thin surfaces. A port must use our local border thickness,
   including the client hole, rather than treating the window as a solid plate.
2. Strong lensing estimates how much source area a screen pixel covers and
   averages extra samples along that direction. The reference also smooths
   magnification of the reduced-resolution blur texture. These are promising
   candidates for reducing edge aliasing during movement; improvement in KWin
   remains unmeasured.
3. Refraction and chromatic sampling use the blurred sampler exclusively. The
   sharp sampler is declared in the shader but not sampled. This supports our
   decision to prevent a sharp backdrop from bypassing the blur.
4. Highlights use screen blending and can recover the backdrop hue in Oklab,
   with gamut handling, rather than simply adding white. The application-window
   integration disables rim/specular/sheen altogether to avoid bright frames;
   that policy should not replace our intentional gem contours and cursor glow.
5. Text adaptation measures the predicted displayed material and chooses light
   or dark text using contrast ratios, smoothing and switching hysteresis.
   This is a separate policy, not a substitute for our caption-band adaptation.

The first experiment evaluated the height-profile lens plus sampling-footprint
filtering in our existing preview against the current shader. Preserve selected blur strength, frame-only coverage,
crisp native captions/buttons, and signature cursor glow. Compare overlapping
windows and motion as well as static appearance; measure GPU cost before
adopting extra sampling. The reference has an MIT license; retain its notice
if implementation code is reused.


### Comparison outcome (2026-10-20)

Decision: retain the existing Emerald lens. The visual difference did not
justify the additional material cost for this project. Experimental shader
branches, preview controls and their dedicated tests were removed. The border
blur, caption readability and overlap fixes remain in the retained material.

For reference, the discarded candidate used a superellipse height profile
(exponent 3.6) and distributed its sampling footprint over the Kawase taps.
On four shifted fine backdrops, summed edge error against a 4x4 supersampled
reference fell from 50856 (unfiltered) to 20072 (filtered), about 60.5 percent.
This measured spatial sampling error, not live KWin motion or perceived quality.

GPU material-composite timings for two preview headers at 1000x210, blur 6,
refraction 180%, dispersion 100%, averaged over 16 draws after four warmups:
current 86.4 us, height-only 91.7 us, height+filter 93.9 us. Capture, blur passes,
readback and native text painting were excluded; these are not total desktop
frame timings. The filtered candidate cost about 8.7 percent more in this test.


## Neutral blur stacking alpha fix (2026-10-20)

With Liquid Glass optics disabled, the neutral branch discarded the sampled
backdrop alpha and emitted opaque frame coverage. Transparent cached texels
therefore contributed black rather than leaving their share of the current
render target visible. The neutral blur now preserves the premultiplied RGBA
Kawase result, matching the standard Blur compositing rule. Native material
colors, opacity and frame lighting were not changed.

A shader regression models six layers with 75-percent valid premultiplied
backdrop coverage. Before the fix, the green channel fell from an input 140
to 105, 79, 59, 44, 33 and 25. After the fix every layer remains 140. This is
controlled OpenGL evidence; the user's actual overlapping windows still need
validation with the updated effect loaded in KWin.


## Standard Blur saturation and stacked borders (2026-10-20)

Compared the actual downsample, upsample and final shaders from
[KWin v6.7.5](https://github.com/KDE/kwin/tree/v6.7.5/src/plugins/blur)
with this effect, using `tests/blur_stack_probe.cpp`. Stock Blur applies a
Rec.709 saturation matrix during final composition. The local standard Blur
configuration has Saturation=173, whereas our optics-disabled branch previously
left saturation at 100. The effect now reads that setting on load/reconfigure
and applies the equivalent transform while preserving backdrop alpha. This
change is restricted to the basic blur branch, with Liquid Glass optics off.

The diagnostic runs real shaders through three downsample and upsample stages,
then composites the same native decoration coat and white client area six
times. Both pipelines use offset 4.4, frame opacity 36%, tint 50%, and identical
input colors. On llvmpipe, mean caption RGB max-minus-min at layer six was:

| Pipeline | Saturation | Channel spread |
| --- | ---: | ---: |
| Stock | 100% | 15.76 |
| Stock | 173% | 58.69 |
| Ours | 100% | 16.86 |
| Ours | 173% | 63.70 |

This is a controlled color/pipeline comparison, with fully initialized buffers.
It excludes noise, KWin damage propagation, cache lifetime, animations and
real desktop stacking. It establishes a substantial color-processing mismatch;
it does not establish that every live stacking artifact is resolved.

Build `blur-stack-probe` explicitly in the native build, then run it with the
repository path, a directory containing stock `downsample.frag`,
`upsample.frag`, `onscreen_rounded.frag` from the linked tag, and an output
directory. It prints per-layer measurements and saves comparison images.

Active and inactive Tint now span 0–200%. The original 0–100% calculation is
retained; above 100%, CPU and GPU materials move toward the preset hue at
constant luminance, limited to the RGB gamut, without increasing alpha.
Regression checks cover both sliders and persistence, native hue/luminance,
GPU hue/luminance/alpha, and saturation with optics disabled. Existing six-layer
partial-alpha coverage remains separate from this full-buffer comparison.


## Liquid Glass Base Opacity (2026-10-20)

Previously the optical material multiplied tint strength by frame opacity but
always retained the sampled desktop in its RGB calculation. Opacity now mixes
the refracted/blurred scene toward the neutral material base before applying
tint, bevel lighting, contours and highlights. This also covers optical buttons.
At 100%, changing the backdrop cannot change any rendered frame/button pixel,
including with Tint=0 and maximum refraction/dispersion. The basic blur path and
native material opacity calculation remain unchanged.

A real-GLSL regression first failed on the previous shader at 100%. It checks
progressively reduced green-versus-purple background influence at opacity
0/50/90/100%, with Tint=0/100/200%, and identical full rendered images at 100%.
Refraction/dispersion regressions now use a partially transparent material;
those controls continue to affect transmitted light at low opacity.
