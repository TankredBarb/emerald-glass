# Licensing and third-party provenance

## Original Emerald Glass source

Unless a file carries another notice, original source in `native/`, `preview/`,
`widgets/` and `tests/`, root build/validation/installation scripts and project-authored
documentation are licensed under **LGPL-3.0-or-later**. The full LGPLv3 text is
in [LICENSE](LICENSE); its incorporated GPLv3 terms are in [COPYING](COPYING).
The copyright holders authorize this license for the original Emerald Glass
contributions. This does not relicense upstream code.

## Vendored KWin effect

- Upstream: <https://github.com/4v3ngR/kwin-effects-glass>
- Base revision: `2ef07fa1a2a062da12289ad8de1393e01f5c1d8e`
- Directory: `vendor/kwin-effects-glass/`
- Upstream license file: [GPLv3](vendor/kwin-effects-glass/LICENSE).
  Individual files retain their own notices, including **GPL-2.0-or-later**
  on the KWin-derived effect. These notices take precedence for those files.

This is a source snapshot with Emerald Glass modifications, not an unmodified
submodule. Upstream Git history and build outputs are not part of the snapshot.
Original copyright and attribution notices, upstream documentation and license
are retained. The local changes affect these files:

- `src/CMakeLists.txt`: shared Emerald material shader and plugin output name.
- `src/blur.cpp`, `src/blur.h`: Emerald frame ownership, blur/material rendering,
  settings isolation, decoration lifecycle, repaint/cache handling and diagnostics.
- `src/blur.kcfg`: a separate settings group for Emerald Glass.
- `src/metadata.json`: Emerald effect identity and description.
- `src/shaders/onscreen_rounded.glsl`: integration of the Emerald material.

The changes to GPL-covered upstream files remain under their applicable GPL
terms. In particular, this effect cannot be represented as LGPL-only code.

## KWin SDF shader helper

`vendor/kwin-sdf-6.7.5.glsl` is an unchanged copy of KWin's
[`src/opengl/sdf.glsl` at v6.7.5](https://invent.kde.org/plasma/kwin/-/blob/v6.7.5/src/opengl/sdf.glsl).
It supplies KWin shader includes to the offline validation tool; it is not an
original LGPL Emerald Glass file. Preserve its upstream licensing context.

## Shared blur shaders and combined artifacts

`native/materialpreview.qrc` embeds `downsample.glsl` and `upsample.glsl` from
the GPL-covered vendor tree. The original preview code is LGPL-licensed, but
its combined distributed form includes those GPL resources. The native
plugins/previews and companion effect must therefore be distributed in
compliance with the applicable GPL terms; GPLv3 is used for combined artifacts.
The source license of separately reusable, original files remains LGPLv3 or
later. This distinction is reflected in native plugin metadata.

Qt, KDE Frameworks, KDecoration3 and KWin are external dependencies and retain
their respective licenses; they are not copied into this repository. The test
build generates config code from the upstream effect's schema, which also
retains the upstream licensing context.

## Design references

Emerald for Compiz, Apple's Liquid Glass documentation and
[ryohsuke1231/liquid-glass](https://github.com/ryohsuke1231/liquid-glass) informed
the visual research. They do not imply endorsement or an official affiliation.
The GNOME alternative-lens experiment was removed, as recorded in
[RESEARCH.md](RESEARCH.md); no GNOME extension is bundled. Any future reuse of
implementation code must preserve the license and notices of that code.

The root `preview-*.png` images are project-generated development captures.
Images under the vendor tree are upstream documentation assets, not current
Emerald Glass screenshots; their upstream attribution and context are retained.
