# Emerald Glass

A glass window decoration and companion KWin effect for KDE Plasma 6.
Inspired by the expressive decorations of **Emerald for Compiz** and the
material language of **Apple Liquid Glass**, reimagined for Plasma.

Emerald Glass combines saturated colors, substantial glass edges, sculpted
window buttons and a cursor-following glow. Tune it from softly blurred glass
to a dense, almost opaque material, with independent looks for active and
inactive windows.

This is an independent project, not an official continuation of Emerald or a
port of Apple's implementation.

## Features

- **Colored glass borders:** presets and custom colors, separate active/inactive
  tint and base opacity, adjustable titlebar height, border width and corners.
- **Liquid Glass optics:** sheet-like refraction, chromatic dispersion, bevel
  highlights and adaptive backdrop contrast, with a live settings preview.
- **Independent border blur:** adjustable blurriness even with optics disabled.
  At 100% base opacity, the material excludes the backdrop, including with LG on.
- **Readable titles:** automatic light/dark text selection or manual overrides
  for active and inactive windows, plus title alignment controls.
- **Glass window buttons:** KWin system layouts, visible application icons and
  a distinct close-button hover color that adapts to red and pink materials.
- **Cursor halo:** an accent-colored glow across the titlebar and borders.
- **Native fallback:** the decoration continues to work with standard KWin Blur
  when the companion effect is disabled.

The companion effect draws **only Emerald Glass window borders and buttons**.
Keep standard **Blur** enabled for application content, panels, menus and other
window decorations. Desktop-wide glass is a possible future direction, not a
feature of this version.

## Requirements

The supported target is **KDE Plasma 6 on Wayland**, using KDecoration3 and an
OpenGL-capable compositor. The development baseline is **KWin/KDecoration3
6.7.5 and Qt 6.12**. Other versions and distributions have not been validated;
KWin plugins use private interfaces and may need rebuilding after an update.
The upstream effect's X11 build option is not a tested Emerald Glass target.

Build tools and development packages:

- C++20 compiler, CMake, Ninja, Python 3, `pkg-config` and `glslangValidator`.
- Qt 6 Widgets, OpenGL/OpenGLWidgets, Quick, DBus, Network and XML; `qmake6`.
- Extra CMake Modules (ECM), KF6 CoreAddons, Config, KCMUtils, I18n and GuiAddons.
- KWin, KWinDBusInterface and KDecoration3 development interfaces, including
  KDecoration3 private headers for the regression tests.
- EGL and epoxy development files for the shader regression tests.

Package names differ by distribution. Install development packages matching
**your running Plasma installation**.

## Install

Clone this repository and run from its root as your regular user:

```sh
./install.sh
```

The installer builds and tests the project, validates the shaders and installs
three plugins into the Qt 6 plugin directory detected by `qmake6`. It requests
`sudo` only for installation. It does not select the decoration or change your
settings. Log out and back in after installation or an update to load the new
libraries.

To build and validate without installing:

```sh
./install.sh --dry-run
```

Builds are kept under `build/install/decoration` and `build/install/effect`.
Use `JOBS=4 ./install.sh` to reduce parallelism, or set `EMERALD_BUILD_ROOT` to
choose another build directory.

The modified effect source is included in `vendor/kwin-effects-glass`; no Git
submodule initialization is needed.

## Enable and configure

1. Open **System Settings → Colors & Themes → Window Decorations**.
2. Select **Emerald Glass** and click **Apply**.
3. Enable **Emerald Glass** in **Desktop Effects**. Leave standard **Blur** enabled.
4. Open the Emerald Glass decoration's **Configure** dialog.

The first settings page contains colors, active/inactive material controls,
title text colors, cursor halo and **Border Blur → Blurriness**. The optics
page controls the optional Liquid Glass material. Border blur requires the
companion effect; optics controls additionally require **Use Liquid Glass
optics**. Changes appear immediately in the dialog preview; **Apply** saves
and updates desktop windows.

For a first setup, choose a color preset close to your wallpaper's dominant
color, then adjust tint and opacity independently for active and inactive
windows. Light/dark title overrides help with unusual wallpaper combinations.
Wallpapers are not bundled or changed by the installer.

Settings are stored in `~/.config/emeraldglassrc`; KWin maintains its own
selection, effect and system-button layout settings in `kwinrc`. Back up your
configuration before experimenting if you want to preserve an existing setup.

## Disable or uninstall

To switch away, select another window decoration and disable the **Emerald
Glass** desktop effect. Standard Blur continues to serve its usual surfaces.

To remove the installed plugins, first switch away as above, then run:

```sh
qt_plugins="$(qmake6 -query QT_INSTALL_PLUGINS)"
sudo rm -- \
  "$qt_plugins/org.kde.kdecoration3/org.tankred.emeraldglass.so" \
  "$qt_plugins/org.kde.kdecoration3.kcm/kcm_emeraldglassdecoration.so" \
  "$qt_plugins/kwin/effects/plugins/emeraldglass.so"
```

Log out and back in. Your saved settings are retained; remove
`~/.config/emeraldglassrc` separately if you no longer need them.

## Development and testing

See [DEVELOPMENT.md](docs/DEVELOPMENT.md) for builds, previews and test commands.
The suite covers material controls, backdrop alpha/opacity, frame coverage,
settings isolation, native decoration behavior and preview fallback. Software
rendering tests do not substitute for testing a live KWin session.

Theme-switch blur recovery and improved resize smoothness have been checked
in the development desktop session. CPU resize measurements and their limits
are recorded in the [review follow-up](docs/REVIEW_FIXES.md). For visual issues,
include your Plasma/KWin and Qt versions, session type, GPU/driver, display
scale, relevant settings and steps to reproduce.

## Inspiration and acknowledgements

- **Emerald / Compiz:** expressive, colorful glass decorations and sculpted
  titlebar buttons. Emerald Glass is a new implementation for KDE Plasma.
- **Apple Liquid Glass:** the material direction described in
  [Meet Liquid Glass (WWDC25)](https://developer.apple.com/videos/play/wwdc2025/219/)
  and [Materials](https://developer.apple.com/design/human-interface-guidelines/materials).
  No Apple implementation or assets are included.
- [ryohsuke1231/liquid-glass](https://github.com/ryohsuke1231/liquid-glass): a
  GNOME glass implementation studied as a visual and technical reference.
  The experimental alternative lens was evaluated and removed; the retained
  Emerald Glass material is documented in the [redesign study](docs/LIQUID_GLASS_REDESIGN.md).
- [4v3ngR/kwin-effects-glass](https://github.com/4v3ngR/kwin-effects-glass) and
  **KWin's Blur contributors:** the basis of the companion effect and shared
  blur stages. Their source and notices are retained in `vendor/`.
- **KDE, Qt and KDecoration3:** the compositor, native decoration framework
  and settings integration.

## License

Original Emerald Glass source is licensed under **LGPL-3.0-or-later**; see
[LICENSE](LICENSE) and [COPYING](COPYING).

The repository also contains GPL-licensed third-party code. The modified
`vendor/kwin-effects-glass` effect retains its upstream licenses and copyright
notices. The preview renderer embeds blur shaders from that tree, so the
combined plugins and previews are distributed under **GPLv3-compatible terms**,
not as LGPL-only binaries. See [THIRD_PARTY.md](THIRD_PARTY.md) for the source
boundary, upstream revision and local modifications.
