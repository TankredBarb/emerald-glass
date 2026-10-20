# Liquid Glass redesign study — 2026-10-20

The accepted baseline is the native border material, presets, caption readability,
buttons, pointer halo, stacking and border blur. The approved sheet material has now been promoted to `preview/glass.frag`,
shared by the KWin effect and settings preview. The previous material is kept
in `preview/experiments/channel-glass.frag` for comparison only.

## What Apple documents

[Meet Liquid Glass, WWDC25](https://developer.apple.com/videos/play/wwdc2025/219/)
describes lensing, geometry-dependent reflections, adaptation to background and
interaction. Clear and Regular serve different contexts; Clear requires suitable
backgrounds and sometimes dimming. These are design principles, not a published
GPU implementation or numerical optical profile.

[Apple HIG: Materials](https://developer.apple.com/design/human-interface-guidelines/materials?changes=l_8_1)
explicitly says Regular blurs and adjusts background luminosity for readable
foregrounds. Clear prioritizes the underlying image. Thus maximum clarity and
arbitrary-background legibility require different compromises.

[Build a SwiftUI app with the new design, WWDC25](https://developer.apple.com/videos/play/wwdc2025/323/),
17:57 onward, explains custom shapes and shared sampling through
`GlassEffectContainer`. Nearby elements share a sampling region, and glass cannot
sample other glass in that container. This is relevant to future KDE-wide
integration; it does not authorize replacing the currently accepted per-window
stacking behavior with a wallpaper-only backdrop.

## GNOME reference

The [user's Reddit reference](https://www.reddit.com/r/gnome/comments/1t26d2c/wip_im_13_and_i_spent_the_last_month_bringing/)
links to the same [ryohsuke1231/liquid-glass repository](https://github.com/ryohsuke1231/liquid-glass)
reviewed earlier. This round reviewed the current README and architecture notes;
the embedded Reddit video and current shader source were not fetched successfully.
No claim of matching that video's appearance follows from this study.

The [project's documentation](https://github.com/ryohsuke1231/liquid-glass#glass-settings-rendering-page)
separates blur, thickness/refraction, curvature, color separation, edge lighting
and inner shading. It describes a flat-topped edge profile. Its stated 22-pixel
macOS measurement is the author's observation, not an Apple specification. Its
zero-dispersion default also argues against treating rainbow fringes as necessary
for a macOS-like appearance. The earlier rejected height-profile/footprint
experiment remains rejected; this prototype does not restore it.

## Diagnosis of the previous material

These findings describe the local shader before sheet promotion:

- `materialDistance` includes both the outer silhouette and the client hole.
  Their gradients curve the border toward opposing edges. The header is treated
  as a narrow raised channel, rather than the visible part of one glass sheet.
- Lighting combines that curved normal, a broad titlebar reflection band, a
  heavily colored contour and an inner dark lip. Much of the visible result is
  painted lighting, which can overpower sampled-background changes.
- Refraction and dispersion sample the selected blurred scene. High blur removes
  the spatial detail these controls displace; high opacity intentionally removes
  transmission. Their useful visual range must be evaluated with low blur and
  partial opacity, as well as tested at zero and full opacity.
- Larger slider deltas alone cannot fix the topology of the surface.

## First experiment: a sheet cropped to the border mask

`preview/glass.frag` now computes the optical surface from the outer
rounded rectangle. The existing frame mask still excludes client content. The
hole crops the sheet; it does not create another lens. This distinction can also
represent a filled panel or menu by omitting the hole in a future integration.

The approved material has a flat center, a 12-logical-pixel shoulder at the outer
edge, and displacement controlled directly in logical pixels. Edge reflection
replaces the broad titlebar glare band. Dispersion remains an optional artistic
control. These numbers and the displacement profile are our experimental choices,
not a reconstruction of Apple's private implementation.

The material keeps the existing blurred-only sampling contract, full-opacity
background exclusion, caption adaptation, button material and semantic Close
feedback. It does not add a raw-scene sampler, change KWin capture/damage behavior,
weaken the halo, alter standard Blur, or expand live ownership beyond our own frame.

## Comparison and verification

Build normally, then launch:

```sh
cmake --build build/blur-control -j12
build/blur-control/emerald-glass-preview --sheet-comparison
```

The upper row uses the previous channel material; the lower row uses the
current production sheet material. Both rows share presets and six immediately applied controls:
refraction, color separation, edge lighting, opacity, tint and blur. The optics
checkbox compares the accepted basic material too. No user configuration is saved.

Start at blur 0–2, opacity 20–40%, and modest tint. Compare refraction 0/90/180 and
color separation 0/50/100 independently. Then increase blur and opacity to see
how they intentionally reduce transmitted detail. At opacity 100%, the background
must stop contributing regardless of optical strength.

CTest runs the real-GLSL regressions against the production material and
additionally verifies outer-edge displacement and exact stability of the
flat caption body through the client-side edge. Common tests cover frame masks,
blurred-background isolation, opacity, tint, caption adaptation, semantic button
colors and partial-alpha stacking.

The interactive comparison built, but Qt could not create a usable graphics
context in this sandbox (desktop X11 and offscreen attempts). Shader rendering
was exercised independently with surfaceless EGL/llvmpipe. This is not live KWin
visual, cursor-motion, deformation, stacking-performance or hardware GPU proof.

## Live integration and remaining visual checks

Judge the comparison on the user's wallpapers and actual header sizes. Check
slider usefulness at partial transmission, crisp contours, caption stability,
buttons/halo and six-window stacking. Measure GPU material cost separately from
capture and blur costs. The shader is integrated; live-session appearance and motion still require
validation after installing the updated plugin.

KDE-wide use additionally needs explicit surface ownership and shared backdrop
rules for panels, menus and neighboring glass. Preserve the current border-only
scope until that separate integration is requested and validated.


## Header control-range verification after promotion

A dedicated `header-optics` CTest uses the real downsample/upsample shaders and
shared blur profile, not just final reconstruction. It renders a 39-pixel header
over broad colored transitions and fine lines, with opacity/tint 35%, specular
65%, and the default blur-finetune multiplier 0.89. It measures the outer ten
pixels, excluding corners. Each half of both slider ranges must visibly change
that region; the caption body must stay identical and client coverage stay zero.

At blur 8, the original prototype's color-separation mean RGB difference was
1.41 levels over the full 0–100% range. Its header spectral displacement range
was expanded from 8 to 32 logical pixels to make the slider useful after blur.
This adjusts only the header material; accepted button optics remain unchanged.

| Blur | Refraction 0–180% mean RGB delta | Separation 0–100% mean RGB delta |
| --- | ---: | ---: |
| 0 | 32.19 | 21.90 |
| 4 | 20.22 | 14.15 |
| 8 | 7.20 | 4.86 |

The two halves of the refraction range at blur 8 change by 3.20 and 4.06 RGB
levels; separation halves change by 2.74 and 2.26. This is a reproducible signal
on a controlled backdrop, not a perceptual guarantee for flat wallpaper or full
opacity. Full opacity deliberately suppresses all background displacement.

To reproduce and optionally save header PNGs into an existing directory:

```sh
build/blur-control/native/blur-stack-probe "$PWD" --header-optics /tmp/header-captures
```

The existing stock-blur stack diagnostic remains available with its original
stock-shader-directory and output-directory arguments.


## Header controls after live feedback

Live feedback established that a passing colored-backdrop comparison was not
sufficient: on the user's green wallpaper/material, refraction was hard to see
and dispersion almost invisible. The installed material settings at inspection
were blur 7, opacity 32%, active/inactive tint 102%, specular 59%, adaptive
contrast 61%, Dimmed Emerald, refraction 74% and dispersion 40%.

A new regression uses those material values with a uniform green backdrop.
Before this change, both control deltas were exactly zero. This isolates the
failure without relying on high-frequency, high-contrast wallpaper details.

The material still uses the same outer sheet and flat center. Refraction now
also varies the shoulder width (near the accepted 12 pixels at 75%) and a
localized rolled-edge reflection. Dispersion splits that reflection spatially
into its RGB components as well as displacing the blurred backdrop samples.
This is artistic spectral edge lighting, not a claim to reconstruct Apple's
private optical renderer. Surface reflection can remain visible at full opacity;
background transmission still vanishes completely at 100%.

On the uniform green regression, full-range mean edge RGB deltas are 17.10 for
refraction and 13.07 for dispersion. The first/second halves are 5.00/13.03 and
7.67/6.67 respectively. The caption body from y=18 through y=38 remains pixel
identical across the full control sweep. Buttons and the native halo are not
changed. These measurements prove a reproducible control response, not final
appearance on the user's wallpaper; live visual confirmation remains separate.


## Dispersion range at ordinary refraction

The previous green-scene test still evaluated dispersion only at 180% refraction.
At the user's ordinary 78% value, the full dispersion delta was only 4.03 mean
RGB levels, versus 13.07 at maximum refraction. At 40% refraction it was 1.33.
Signal/persistence and the installed binary were checked; the issue was optical
reflection amplitude being multiplied by the low thickness factor.

Dispersion now has an additional chromatic reflection gain gated by nonzero
refraction, instead of inheriting all of its strength from thickness. Dispersion
zero retains exactly the existing achromatic edge; the geometry, caption body,
blur contract and opacity behavior are unchanged. At high dispersion, the
spectrally separated rim is intentionally pronounced.

The regression now sweeps dispersion 0/50/100% at refraction 40/78/120%, using
blur 7, opacity 32%, tint 102%, specular 59%, adaptive contrast 51% and a uniform
green scene. Full-range deltas are 8.93/15.30/22.77 mean edge RGB levels. At 78%,
the two half-range deltas are 7.93 and 12.10. Optional captures include all five
positions 0/25/50/75/100%. Both GUI verification routes likewise evaluate
dispersion at 78% refraction rather than only at its maximum. GUI verification
code was built; the automatic evidence here is still EGL shader rendering.
