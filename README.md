# Nexus Game Launcher

Nexus is a local-first desktop app for organizing and launching PC games from
one library. It is built with C++20, Qt 6 Widgets, CMake, and SQLite. Game
entries, settings, and play-session history are stored locally; Nexus does not
require an account or a network connection.

## Features

- Add and edit games manually, including executable path, launch arguments,
  working directory, cover image, tags, genre, category, collection, and
  franchise.
- Search and filter the library, then sort games or switch between grid and
  list views.
- Mark favorites, track playtime and launch counts, and view recent sessions.
- Launch games directly, focus or stop a process launched by Nexus, and avoid
  starting the same library entry twice from Nexus.
- Import and export the library as JSON.
- Customize the theme, accent color, layout density, and default library view.

Removing a game from Nexus removes only its library entry. It does not uninstall
or delete the game's files or saved data.

## Run on Windows

The Windows presets target Windows 10/11, 64-bit.

### Requirements

1. **Visual Studio 2022** with the **Desktop development with C++** workload and
   the Windows 10/11 SDK.
2. **CMake 3.25 or newer**.
3. **Qt 6.6 or newer**, installed with the **MSVC 2022 64-bit** kit. Include Qt
   Widgets and the Qt SQL SQLite driver.

### Build and launch

1. Clone or download this repository and open **Developer PowerShell for VS
   2022**.
2. Change to the app's source directory:

   ```powershell
   cd path\to\Nexus-Game-Launcher\apps\nexus-launcher
   ```

3. Set `QT_ROOT` to the installed Qt kit directory. Update the example to match
   your Qt version and installation location:

   ```powershell
   $env:QT_ROOT = "C:\Qt\6.8.3\msvc2022_64"
   Test-Path "$env:QT_ROOT\lib\cmake\Qt6"
   ```

   The `Test-Path` command should return `True`.

4. Configure, compile, and run the tests:

   ```powershell
   cmake --preset windows-msvc
   cmake --build --preset windows-debug
   ctest --preset windows-debug
   ```

5. Start Nexus:

   ```powershell
   .\build\windows-msvc\Debug\nexus-launcher.exe
   ```

### Create a packaged Release build

To prepare a Release folder with Qt runtime dependencies:

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-release
cmake --install build/windows-msvc-release --config Release
& "$env:QT_ROOT\bin\windeployqt.exe" --release .\dist\bin\nexus-launcher.exe
& .\dist\bin\nexus-launcher.exe
```

The raw executable may need Qt runtime libraries and plugins beside it. Use
`windeployqt` before distributing the Release folder.

## Local data

- Game library and launch history: `%LOCALAPPDATA%\NexusGameLauncher\nexus.sqlite3`
- Settings: `%APPDATA%\NexusGameLauncher\nexus-settings.json`

Nexus leaves games running when the launcher closes. Playtime tracking for an
active session ends when Nexus exits.

## Project layout

- `apps/nexus-launcher/src/ui/` — Qt interface
- `apps/nexus-launcher/src/data/` — SQLite database and library repository
- `apps/nexus-launcher/src/launcher/` — process launching and session tracking
- `apps/nexus-launcher/src/app/` — persistent settings
- `apps/nexus-launcher/tests/` — Qt Test suite

See [`apps/nexus-launcher/README.md`](apps/nexus-launcher/README.md) for
architecture details and project boundaries.
