# Desktop widget review — 2026-10-22

Scope: all new source, packaging, previews and tests in `widgets/systemmonitor`
and `widgets/weather`, including their shared glass shader and desktop edit
integration. Wallpapers, the window decoration and the KWin effect are outside
this change.

## Findings and fixes

- **Preview could overwrite its input image.** The monitor's `--settings` and
  `--text-settings` flags remained in positional arguments, shifting the capture
  path onto the input filename. Both flags are now removed before resolving
  paths. A regression test runs both editor captures, verifies the input hash
  and checks the requested PNG output.
- **Manual coordinate fields stopped following location selection.** Restoring
  invalid input with a direct assignment removed the QML text binding. Editing
  now restores the binding after validation; tests change the selected location
  after invalid input and verify both coordinate fields.
- **Invalid weather configuration retained old fetch timestamps.** Disabling a
  configured location now clears its fetch time and in-memory refresh deadlines
  together with the old forecast. A backend regression covers this transition;
  saved per-location provider restrictions remain in their separate caches.
- **Malformed sensor-name settings broke display or further edits.** Shared
  parsing accepts a JSON object with nonempty string names, falls back safely
  for invalid JSON/types and keeps valid names. Tests cover invalid persisted
  values and subsequent renaming.
- **Sensor names could be interpreted as markup.** Sensor headings, values and
  descriptions now use plain text. A rendering regression verifies literal
  markup characters in a name.
- Both distributable plasmoid packages now include `COPYING` alongside the
  LGPL `LICENSE`. The root README links to their separate build/install paths.

## Validation and limits

The monitor's seven CTest suites and weather's two suites pass on Qt 6.12 /
Wayland. They cover shader output, movement/rotation/scaling, idle rendering,
sensor selection/naming/persistence, font/color settings, preview arguments,
weather parsing/cache/scheduling and the location/forecast UI. Metadata and
configuration schemas parse successfully. QML lint resolves the monitor; the
weather backend lacks static QML type metadata, so its import/context-property
warnings are checked against the compiled runtime tests rather than described
as a clean static check.

The earlier movement fix is retained: wallpaper mapping follows the window's
`afterAnimating` signal before scene synchronization and does not request idle
frames. The user confirmed smoother movement. Rare freezes were not reproduced
in the session recording and are not claimed fixed by this review.

The local graphics probe measured roughly 0.17 ms median GPU time for the glass
scene and 0.22 ms for the six-sensor scene at 144 Hz; these are isolated scene
measurements, not GPU costs for the whole desktop. Five seconds of static glass
produced no new rendered frames after warm-up. Provider outages, physical
suspend/resume, other Plasma versions and other GPUs remain separate live checks.

## Next development

1. Per-sensor presentation: value, history graph or bar, with the sensor's own
   name/color/range settings. Preserve existing defaults and KDE sensor IDs.
2. Optional grid spans and grouping, so a useful graph can occupy more space
   without forcing every cell to grow equally.
3. Named configuration presets after the presentation settings stabilize.

Weather can keep its current feature set while the monitor gains these display
choices. The native KDE sensor service remains responsible for discovery and
values; there is no need for another hardware polling backend.
