# Nexus Game Launcher

A lightweight, native Windows game launcher built with **C++**, **Qt 6**, and **SQLite**.

Nexus aims to provide a clean desktop experience for organizing and launching your games from one place.

## Features

- 🎮 Game library management
- 🚀 Launch games directly from the application
- 💾 Local data storage with SQLite
- 🖥️ Native Windows desktop interface
- ⚡ Built with C++ and Qt 6
- 📦 Portable Windows deployment

## Tech Stack

- **Language:** C++
- **UI Framework:** Qt 6
- **Database:** SQLite
- **Build System:** CMake
- **Compiler:** MSVC 2022 (v143)

## Requirements

For development:

- Windows 10 or Windows 11
- Visual Studio 2022 with Desktop development with C++
- MSVC v143 build tools
- Windows SDK
- Qt 6.6 or newer with the MSVC 2022 64-bit kit
- CMake

## Build from Source

Clone the repository:

```bash
git clone https://github.com/MK0Ry/Nexus-Game-Launcher.git
cd Nexus-Game-Launcher/apps/nexus-Launcher
```

Configure and build:

```powershell
$env:QT_ROOT = "C:\Qt\6.12.0\msvc2022_64"
$env:CMAKE_PREFIX_PATH = $env:QT_ROOT

cmake --preset windows-msvc
cmake --build --preset windows-debug --config Release
```

See the CMake presets and project documentation for the exact build configuration.

## Download

Prebuilt Windows releases will be published on the [Releases](../../releases) page when available.

## Contributing

Contributions, bug reports, and feature suggestions are welcome. Open an issue to report a problem or suggest an improvement.

## License

See the `LICENSE` file for licensing terms.
