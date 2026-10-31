# Nexus Game Launcher

Nexus is a local-first Windows game library and direct launcher. The first
delivery implements the Qt desktop shell and persistent settings (Stage 1),
manual SQLite library management (Stage 2), and safe direct process launching
with play-session history (Stage 3). It does not require an account, network
connection, or administrator permissions.

## Architecture

- **Qt Widgets UI** (`src/ui/`): `MainWindow` owns navigation and views;
  `GameDialog` validates manual game entries. UI code calls service/repository
  interfaces rather than issuing SQL.
- **Settings** (`src/app/`): versioned JSON settings stored in the user's
  roaming application config directory and written atomically with `QSaveFile`.
- **Data** (`src/data/`): SQLite access and schema migrations are isolated from
  the UI. `GameRepository` uses prepared statements and transactions.
- **Launching** (`src/launcher/`): `QProcess` starts the selected executable
  directly with parsed arguments and a configured working directory—never
  through a shell. Running sessions are written to SQLite and closed on exit.
- **Model** (`include/nexus/model/`): value types shared by UI and services.

The database is stored at
`%LOCALAPPDATA%/NexusGameLauncher/nexus.sqlite3`; settings are stored at
`%APPDATA%/NexusGameLauncher/nexus-settings.json`. The SQLite file is created
and migrated automatically on first launch. Schema version is kept in
`PRAGMA user_version`.

## Exact Windows dependency setup

1. Install **Visual Studio 2022** with the *Desktop development with C++*
   workload and the Windows 10/11 SDK.
2. Install **CMake 3.25 or newer**. The Visual Studio 2022 installer can supply
   CMake and MSBuild; Ninja is optional because the preset uses the Visual Studio
   generator.
3. Install **Qt 6.6 or newer** with the **MSVC 2022 64-bit** kit. In the Qt
   installer, include Qt Base / Widgets and the Qt SQL SQLite driver plugin.
   The SQLite driver is bundled with Qt; no separate SQLite library is needed.
4. Set `QT_ROOT` to the selected Qt kit, for example
   `C:\Qt\6.8.3\msvc2022_64`, in the same terminal used for CMake.
5. Open a *Developer PowerShell for VS 2022*, then from this folder run:

   ```powershell
   cmake --preset windows-msvc
   cmake --build --preset windows-debug
   ctest --preset windows-debug
   ```

   To create a release build:

   ```powershell
   cmake --preset windows-msvc-release
   cmake --build --preset windows-release
   cmake --install build/windows-msvc-release --config Release
   ```

   The release executable is placed in `dist/bin`. Copy the Qt runtime
   dependencies alongside it with `windeployqt` from the same Qt kit:

   ```powershell
   & "$env:QT_ROOT\bin\windeployqt.exe" --release .\dist\bin\nexus-launcher.exe
   ```

## Current functionality

- Dark and light themes, accent color, compact layout, and default library
  view persist across launches.
- Nexus branding is drawn in-app for the window icon and startup splash; no
  third-party artwork is bundled.
- Add and edit a game by selecting its executable; configure description,
  genre, category, collection, franchise, launcher label, cover image, launch
  arguments, working directory, and tags.
- Search across game details, filter by executable availability, genre, launcher,
  and tag, sort by name/recent play/playtime/date/custom order, and switch views.
- Import and export the local library as versioned JSON; duplicates are skipped
  during import and missing paths are retained for later repair.
- Favorite games, remove entries, and inspect recently played and playtime
  summaries, including favorites pinned on Home. Removing an entry never deletes
  game files or saves.
- Missing executable paths are clearly marked and can be repaired in the edit
  dialog. Duplicate executable paths are rejected.
- Launch directly, prevent a second Nexus launch of an active entry, focus or
  stop a process started by Nexus, and record playtime and launch count.
- All library, theme, settings, and launch-history data stays local.
- Nexus does not close games when it exits. Playtime tracking for an active
  session ends when the launcher closes.

## Deliberate boundaries for this delivery

Folder scanning, metadata/artwork downloads, launcher-client detection,
performance sensors, system tray/startup behavior, dashboard rearrangement,
and game-install migration are not implemented in Stages 1–3. No DRM, ownership check,
anti-cheat, or authentication behavior is bypassed. Existing launcher-managed
games can be added manually by selecting their executable; protocol-based
launcher integrations are a later stage.

## Tests

The Qt Test suite covers settings persistence and malformed files, SQLite
library operations, duplicate and missing paths, metadata persistence,
import/export, session accounting, and command argument parsing. Build and run
it with the Windows commands above.
