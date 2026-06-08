# Cadino

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Unified CAD + NURBS 3D modeling tool for interior designers. Draw 2D floor plans and 3D models in a single application with **bidirectional sync** — edits in either view update the other automatically.

## Architecture

Single source of truth: a parametric scene graph (`cadino::core::Document`) holds semantic entities (`Wall`, `Door`, `Window`, `Slab`, ...). The 2D plan view and the 3D viewport are independent **renderings** of the same underlying data, so any edit anywhere reflects everywhere — no syncing layer required.

```text
                    Document
                       │
        ┌──────────────┼──────────────┐
        ▼              ▼              ▼
   2D Plan View   3D Viewport   Section/Elevation
```

## Tech stack

| Layer | Tool |
| --- | --- |
| Language | C++20 |
| UI | Qt 6 (Widgets, OpenGL) |
| GPU | Vulkan (planned) |
| Geometry kernel | OpenCASCADE (OCCT) |
| NURBS / `.3dm` I/O | openNURBS (planned, McNeel) |
| Math | Eigen |
| Logging | spdlog |
| Tests | Catch2 |
| Build | CMake + Ninja + vcpkg (manifest mode) |

## Requirements

- **Windows**: Visual Studio 2022/2026 with C++ desktop workload
- **macOS**: Xcode command-line tools (planned)
- **Qt 6.5+** — install via the [official Qt Online Installer](https://www.qt.io/download-qt-installer-oss)
- **Vulkan SDK** — [LunarG](https://vulkan.lunarg.com/sdk/home)
- **vcpkg** — clone to a location and set `VCPKG_ROOT` env var
- **CMake 3.25+**
- **Ninja**

Set the environment variables:

```powershell
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "E:\dev\vcpkg", "User")
[Environment]::SetEnvironmentVariable("CMAKE_PREFIX_PATH", "C:\Qt\6.10.0\msvc2022_64", "User")
```

(Adjust the Qt path to match your install.)

## Build

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

The first configure step downloads and builds dependencies via vcpkg (OCCT compile takes 20-60 minutes).

Run:

```powershell
.\build\windows-msvc-debug\bin\Cadino.exe
```

Run tests:

```powershell
ctest --preset windows-msvc-debug
```

## Project layout

```text
cadino/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── src/
│   ├── core/                # Data model: Document, entities, commands
│   │   ├── entity/          # Wall, Door, Window, ... (semantic types)
│   │   ├── document/        # The scene graph + transactions
│   │   └── command/         # Undo/redo command stack
│   ├── ui/                  # Qt widgets: PlanView (2D), Viewport3D, tools
│   └── app/                 # Qt application + main window
├── tests/                   # Catch2 unit tests
└── third_party/             # Vendored sources (openNURBS planned)
```

## License

[MIT](LICENSE) © 2026 Cadino contributors
