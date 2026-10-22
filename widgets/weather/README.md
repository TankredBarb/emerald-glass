# Emerald Weather

An independent desktop plasmoid with the same wallpaper-based Liquid Glass
material and effect controls as Emerald System Monitor. It does not replace
KDE weather providers, system applets, the window decoration or the KWin effect.

The initial location is Odesa, Ukraine (46.48572, 30.74383, Europe/Kyiv).
Choose Open-Meteo or MET Norway under **Configure → Location and provider**.
Both forecast APIs accept coordinates. Search by a city, town or village name;
results include country and region to distinguish namesakes. If a place is
missing, enter coordinates and an IANA time zone manually. Search uses
Open-Meteo / GeoNames independently of the selected forecast provider.

The card shows temperature, condition, wind in m/s, humidity, sea-level pressure
in hPa, six forecast samples about three hours apart, and four compact daily summaries. Click the card or the calendar button for
a scrollable extended forecast: up to 16 days from Open-Meteo and the available
10-day horizon from MET Norway (which can span 11 calendar dates). It shows
minimum/maximum temperature, precipitation and maximum forecast wind. These are model forecasts, not
local station observations. MET Norway daily ranges and precipitation are
derived from the available forecast periods: today covers the remaining
forecast hours, and maximum wind is taken from the available forecast samples. A precipitation
block crossing midnight is assigned to its starting day. Overlapping precipitation blocks are not counted twice.
MET Norway does not provide apparent temperature; that line is omitted.
Unavailable values display a dash rather than zero.

**Liquid Glass** configures the same refraction, dispersion, highlights, glow,
blur, opacity, tint and corner radius as the monitor. Each widget has independent
settings. The material samples the actual desktop wallpaper and follows moves
and wallpaper changes. Zero material opacity and zero blur are supported.
**Text** configures fonts, sizes, styles and colors for temperature, city name,
and details/forecast. Larger fonts increase the minimum card height. The extended
forecast measures columns using the configured font and values; if they exceed
the available window width, the table scrolls horizontally.

## Build and install

Requires Qt 6 Quick, Network, ShaderTools and KDE Plasma 6 QML modules (Kirigami,
KCMUtils, KQuickControls). Tests also require Qt Test and QuickTest. Tested on
Plasma 6.7 / Qt 6.12, Wayland. Build from the repository root:

```sh
cmake -S widgets/weather -B build/weather -G Ninja
cmake --build build/weather
ctest --test-dir build/weather --output-on-failure
kpackagetool6 --type Plasma/Applet --install build/weather/package
```

Use `--upgrade` for an existing installation. Add **Emerald Weather** through
Plasma's widget chooser. The ID is `org.tankred.emerald.weather`; installation
is per user and does not alter existing applets. Updating a loaded QML/C++
package requires restarting Plasma to clear its component/plugin caches.
The standalone build copies shared material/settings files from
`widgets/systemmonitor`; it never modifies that package.

Preview with live provider data and a wallpaper:

```sh
QT_QUICK_CONTROLS_STYLE=org.kde.desktop build/weather/emerald-weather-preview /path/to/wallpaper.jpg
QT_QUICK_CONTROLS_STYLE=org.kde.desktop build/weather/emerald-weather-preview --met /path/to/wallpaper.jpg
```

An optional second positional argument saves a screenshot after 12 seconds
and exits. `--extended` opens the expanded forecast immediately. `--location-settings`, `--text-settings`, `--effects-settings` open
individual editor pages. QML tests run with the KDE desktop control style.
Offline tests cover parsing, units/missing values, local time, precipitation
aggregation, coordinate URLs, independent cached locations/providers, layout
and editor loading. Scheduler tests exercise real timer callbacks with simulated
network replies: periodic refresh, failed-request retry, provider deadlines,
cache restart, interval changes and overdue updates after suspend. Live provider
success and physical suspend/resume must be checked separately.

## Requests, cache and attribution

No account or API key is needed for personal, noncommercial use. Forecasts
refresh every 30 minutes by default (configurable 15–120). Manual refresh has
a minimum 60-second cooldown. Both automatic and manual requests respect the
provider's `Expires` and `Retry-After` headers. The automatic deadline is based
on the last successful fetch rather than the widget's startup time. If the
provider defers requests, the timer wakes at that deadline rather than skipping
a full refresh interval. A 30-second watchdog checks for overdue updates after
suspend or a clock change. Failed requests retry after five minutes, or later
if required by the provider. Changing the interval reschedules immediately;
reapplying unchanged settings does not postpone the deadline. Provider pauses,
retry deadlines and the latest request error survive restarts even when no
valid forecast is cached. The card and
expanded forecast show the next automatic refresh time.
Requests use an identifying User-Agent with the project's contact URL.
No automatic provider switching occurs. A failed refresh preserves the last
valid forecast and displays an error/stale marker. Changing coordinates or
provider clears the previous location's data before loading the matching cache.
A per-location/provider/time-zone cache survives restarts under
`~/.cache/emerald-glass/weather/`; snapshots older than 24 hours are ignored.
The card distinguishes fetch time from the forecast's valid time.

Forecast and geocoding data are attributed to their original providers; the
code/package license does not replace the data license:

- [Open-Meteo forecast API](https://open-meteo.com/en/docs),
  [terms](https://open-meteo.com/en/terms): CC BY 4.0 data; free API for
  noncommercial use, subject to rate limits.
- [MET Norway Locationforecast](https://api.met.no/weatherapi/locationforecast/2.0/documentation),
  [terms and attribution](https://api.met.no/doc/TermsOfService): CC BY 4.0.
- [Open-Meteo geocoding / GeoNames](https://open-meteo.com/en/docs/geocoding-api).
- [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).
