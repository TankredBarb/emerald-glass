# Emerald System Monitor

A new desktop plasmoid, separate from KDE's System Monitor applet and sensor
faces. The initial layout follows the existing desktop monitor: CPU load,
GPU temperature and CPU temperature, with one minute of history at one-second
intervals. The **Sensors** settings page uses KDE's native sensor chooser to
browse/search all sensors exposed by the local service. Add or remove sensors
and drag the selected entries to change their order; Apply saves the selection
for this widget. Existing installations start with the original three sensors.
The pencil beside each selected sensor edits its displayed title; confirm with
Enter or the checkmark, then Apply. Empty names restore the default title. Names
are saved by sensor ID, so they survive reordering and removing/readding a sensor.
An empty selection is retained and shows a settings hint. Missing sensors remain
selected, show a dash, and do not create a graph.

New sensors use KDE names, descriptions, formatted values and units. Numeric
sensors use the advertised range, or automatic scaling when no range is known;
text/boolean readings display without a history graph. The original three
sensors retain their appearance, colors and 0–100 history scale. Additional
sensors use the shared accent palette. **Sensors → Columns** chooses the grid
width (default three); 1 makes a column, and 0 / Single row retains a horizontal
row. Row heights follow the configured fonts and the card's minimum height grows
with the number of rows. Narrow layouts scroll horizontally instead of squeezing
cells below their minimum width.

The Qt Quick shader adapts the sheet material from Emerald Glass. It samples
the actual wallpaper item of the containing desktop, with refraction, subtle
dispersion, rounded edges and reflections. Text and charts remain undistorted.
The capture follows the card's desktop position and live wallpaper updates;
it includes only wallpaper, not other widgets or application windows. The
KWin effect and window decoration are unchanged. Without wallpaper access or
on the software scene graph, the card uses a tinted translucent fallback.

The current baseline is Plasma 6.7 / Qt 6.12 on Wayland. It requires Qt Quick,
Qt ShaderTools, KDE's `org.kde.ksysguard.sensors` and `org.kde.quickcharts` and `org.kde.ksysguard.faces`
QML modules. Building the preview/tests also requires KI18nQml. The optional GPU regression suite also requires Qt QuickTest. CTest runs each
QML test file in a separate process to isolate scene graph and native dialog
teardown.

From the repository root:

```sh
cmake -S widgets/systemmonitor -B build/systemmonitor -G Ninja
cmake --build build/systemmonitor
ctest --test-dir build/systemmonitor --output-on-failure
kpackagetool6 --type Plasma/Applet --install build/systemmonitor/package
```

Use `--upgrade` instead of `--install` for an existing installation. Add
**Emerald System Monitor** through Plasma's widget chooser. The package ID is
`org.tankred.emerald.systemmonitor`; it installs only in the user's plasmoid
directory. After updating a loaded package, restart Plasma to clear its QML
component cache. Installation does not change desktop layout or remove widgets.

Open the widget's **Configure → Liquid Glass** page to adjust refraction,
dispersion, edge highlights, cursor glow, blur, material opacity, tint color
and strength, adaptive contrast, and corner radius. The preview updates as
you edit. **Apply** saves settings for that widget and updates its desktop
card without restarting Plasma. **Restore first-version appearance** resets
the pending settings to the original appearance; use Apply to save the reset.
Disabling Liquid Glass optics retains the configured tint, opacity and blur,
while disabling refraction, dispersion, highlights and cursor glow.

The **Text and charts** page configures fonts (family, size and style) and colors
for values, headings and descriptions. **Use value color for all text and
dividers** is enabled by default so captions and separators follow the main
text color. Disable it to use the separately stored heading/description colors. Its preview uses the same
text component as the desktop widget. Large fonts increase the card's minimum
height. Text reset and chart reset do not change Liquid Glass settings.

Each graph has its own color. The optional thin contrasting outline adapts
to that color, keeping lines visible when material opacity and blur are both
zero, without adding an opaque background behind the graphs.

To preview with live sensors and a chosen background:

```sh
build/systemmonitor/emerald-monitor-preview /path/to/wallpaper.jpg
```

An optional second argument saves a screenshot after 6.5 seconds and exits.
Use `--settings`, `--text-settings` or `--sensor-settings` to preview the
corresponding settings page instead.
Use `--text-settings` to preview text and chart settings.
GPU tests exercise wallpaper updates, ancestor movement, rotation and scaling,
resizing, transparent corners, and the fallback without a wallpaper item.
They also cover the material controls, neutral optics, fully opaque material,
independent cursor glow, and settings-to-preview bindings and reset.
Text tests cover KConfig font/color defaults, font acceptance/cancellation,
color propagation and sizing. Chart rendering is checked on white and black
wallpaper with zero material opacity and zero blur.
They can also be run with `QT_SCALE_FACTOR=1.5` to exercise fractional scaling.

The glass maps wallpaper samples through the complete desktop transform, including
Plasma's rendering layer after rotation. Mapping is updated on the window's
`afterAnimating` signal before frame synchronization, so it follows movement each
frame without a polling timer or a continuously running animation while idle.
The applets suppress only their own
native rotation-button tooltip: on the tested Plasma/Qt version that tooltip can
cover the rotated button and prevent another press. A separate test uses Plasma's
installed edit overlay to rotate the monitor three times without changing its size.
Sensor labels explicitly use Qt's scalable text renderer so rotating them does
not rotate native bitmap glyphs, which can become jagged in desktop edit mode.
