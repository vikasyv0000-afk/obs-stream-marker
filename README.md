# OBS Stream Marker

Made by **veekaxx** - Instagram: [@veekaxx](https://instagram.com/veekaxx)

A native OBS Studio dock for Windows that records **timestamps of interesting moments** while you
stream (built for BGMI YouTube livestreams). Click a category button - *Clutch / Kill*, *Funny Moment*,
*Winner Winner*, *Epic Moment*, or your own - and the plugin stores the exact stream time, wall-clock
time and UTC time. After the stream, export everything as CSV (Excel / Google Sheets) or JSON and use it
to cut clips.

Fully offline. No network access. All data stays in your OBS config folder.

Targets OBS Studio 31.x, Windows 10/11, 64-bit. Built against the official OBS frontend API and Qt 6.

## Using it

1. Install the plugin (below), start OBS, open **Docks > Stream Marker**.
2. Press **Start Streaming**. The status switches to **LIVE** and the timer starts from the moment OBS
   reports the stream as started. Buttons are greyed out until then.
3. Click a category whenever something happens. Each click is written to disk immediately.
4. Stop the stream - the session is closed. The next stream creates a new session.
5. **Export** menu: current stream CSV, all streams CSV, all markers JSON.

Other features:

| Feature | How |
|---|---|
| Add category | **+ Add Category** (name, colour swatches / picker, emoji icon) |
| Edit / delete / reorder | Right-click a button, or **Settings > Manage categories** |
| Notes | Double-click a history row (or enable *Ask for a note after each marker*) |
| Delete one marker | the x on its row |
| Clear history | **Clear History** (confirmation + automatic `markers.backup-*.json`) |
| Previous streams | **Streams** button: browse, rename, export, delete sessions |
| Stream title | Field in the dock; it is used in the CSV |
| Timestamp offset | Settings. Use e.g. `-10 s` to mark a little *before* the click |
| Start/Stop button | ▶ Start in the dock begins a session manually (for streams OBS cannot detect, e.g. browser extensions); also auto-starts on OBS recording or Virtual Camera |
| Markers while offline | Settings (off by default). Creates a separate "Offline markers" session |

The dock is a normal OBS dock: dock it anywhere, float it, resize it. On very narrow widths the grid
collapses to one column.

### About the timestamps

* `Elapsed Seconds` / `Stream Time` use `std::chrono::steady_clock` starting at OBS's *streaming started*
  event. They cannot be thrown off by clock changes (NTP, DST, manual changes).
* `Stream Time` is written as **HH:MM:SS** (hours keep counting past 99) rather than HH:MM so you can
  cut clips accurately. Elapsed seconds are exported as well.
* OBS's "started" moment is when the output connects. The YouTube video can start a few seconds
  earlier or later, and YouTube viewers see a delay - so spot-check one marker against the VOD and
  use **Timestamp offset** to correct any constant difference.
* If OBS or Windows crashes mid-stream, all markers up to the last click are on disk. On the next start the
  unfinished session is closed at its last marker and flagged *Interrupted*.

## Data files

Location: `%APPDATA%\obs-studio\plugin_config\obs-stream-marker\` (Settings > *Open data folder*).

| File | Content |
|---|---|
| `categories.json` | categories (id, name, colour, icon), in display order |
| `streams.json` | stream sessions (id, title, start/end, interrupted flag) |
| `markers.json` | all markers |
| `settings.json` | plugin options |

Writes are crash-safe: written to a temporary file and atomically renamed over the old one; the previous
version is kept as `<file>.bak`. A corrupt file is moved aside as `<file>.corrupt-<time>` and the `.bak`
is used instead - OBS never fails to start because of bad data. Examples are in [`examples/`](examples).

### CSV columns

`Stream ID, Stream Title, Category, Stream Time, Elapsed Seconds, Date, Actual DateTime, UTC Timestamp, Notes`

UTF-8 with BOM and CRLF line endings, so Excel opens it correctly. Cells that start with `= + - @`
get a leading apostrophe so spreadsheets never run them as formulas.

## Building

Requirements (Windows 10/11):

* Visual Studio 2022 or Build Tools with **Desktop development with C++**
* CMake 3.28+
* Git is not required. First configure downloads, with SHA-256 verification, the OBS 31 sources, pre-built
  obs-deps and Qt6 (about 600 MB) into `.deps\`.

```powershell
winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
winget install Kitware.CMake
```

Then, from the project root:

```powershell
.\scripts\build.ps1            # configure + build + package into .\dist
.\scripts\build.ps1 -Test      # additionally build and run the headless core test
.\scripts\build.ps1 -Install   # ... and install into OBS
```

Manual equivalent:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
cmake --install build_x64 --config RelWithDebInfo --prefix dist
```

The plugin is built against the OBS 31.1.x SDK; it links the Qt6 libraries that ship with your OBS install.

## Installing

**Easiest:** download `OBS-Stream-Marker-Setup-1.1.0.exe` from the [Releases](../../releases) page, close OBS, run it, then start OBS and open **Docks > Stream Marker**. (Windows SmartScreen may warn because the installer is not code-signed: *More info > Run anyway*.)

Manual / from source:

Close OBS, then either run `.\scripts\install.ps1`, or copy the package folder by hand:

```
dist\obs-stream-marker\            ->   C:\ProgramData\obs-studio\plugins\obs-stream-marker\
    bin\64bit\obs-stream-marker.dll
    data\locale\en-US.ini
```

Start OBS and open **Docks > Stream Marker**. Check *Help > Log Files > View Current Log* for a line
starting with `[obs-stream-marker] loaded`.

To uninstall, use *Apps > Installed apps*, or delete that folder. Your data stays in `plugin_config` until you remove it.

## Source layout

```
src/plugin-main.cpp            module load/unload, OBS frontend event hookup, dock registration
src/core/Models.*              Category / Session / Marker / Settings + JSON
src/core/StreamTimer.*         steady_clock stream timer
src/core/StorageManager.*      crash-safe JSON persistence
src/core/CategoryManager.*     category list + persistence
src/core/SessionManager.*      session lifecycle, marker recording, history operations
src/core/ExportManager.*       CSV / JSON export
src/ui/MarkerDock.*            the dock
src/ui/MarkerButton.*          pastel category button
src/ui/CategoryDialogs.*       add / edit / manage categories
src/ui/SessionsDialog.*        previous streams
src/ui/SettingsDialog.*        options
tests/core_smoke_test.cpp      headless test of the core logic
```

`src/core` depends only on Qt Core, not on OBS or widgets.

## OBS APIs used (checked against the 31.1.2 headers)

`obs_frontend_add_dock_by_id`, `obs_frontend_get_main_window`, `obs_frontend_add_event_callback` /
`obs_frontend_remove_event_callback`, `obs_frontend_streaming_active`, events `STREAMING_STARTED`,
`STREAMING_STOPPED`, `FINISHED_LOADING`, `EXIT`, and `obs_module_config_path`.

## License

GPL-2.0-or-later (same as OBS plugin template; see `LICENSE`).
