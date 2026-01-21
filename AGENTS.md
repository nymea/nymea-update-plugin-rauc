# Repository Guidelines

## Project Structure & Module Organization
This repository is a small Qt plugin that bridges nymea’s update framework with RAUC. Core logic lives in `updatecontrollerrauc.cpp` and `updatecontrollerrauc.h`. The qmake project file is `nymea-update-plugin-rauc.pro`. Packaging metadata is in `debian/`, `debian-qt5/`, and `debian-qt6/`. A local `build/` directory is used for out-of-tree builds; keep build artifacts out of version control.

## Build, Test, and Development Commands
- `qmake` generates Makefiles for the plugin.
- `make` builds `nymea_updatepluginrauc` as a shared library.
- `make install` installs the plugin into `$$[QT_INSTALL_LIBS]/nymea/platform/` (see `nymea-update-plugin-rauc.pro`).

Recommended out-of-tree build:
```
mkdir -p build
cd build
qmake ..
make
```

## Coding Style & Naming Conventions
The codebase is C++ with Qt (Qt5 or Qt6). Follow the existing style: 4-space indentation, braces on a new line, and Qt-style naming (`UpdateControllerRauc`, `updateType()`). Member fields use the `m_` prefix (e.g., `m_updateRunning`). Keep SPDX headers and include guards (`UPDATECONTROLLERRAUC_H`) consistent. The build uses `-Werror`; avoid introducing new warnings. Use C++11 when building with Qt5 and C++17 with Qt6.

## Testing Guidelines
There are no automated tests in this repository. Validate changes by building the plugin and, if possible, exercising it in a nymea + RAUC environment. Document any manual checks in your PR description.

## Commit & Pull Request Guidelines
This repo does not contain commit history yet, so follow your organization’s standard conventions. If unsure, use concise, imperative commit messages and reference relevant issues. For PRs, include a short summary, testing steps (e.g., `qmake && make`), and note whether Qt5/Qt6 compatibility was verified.

## Dependencies & Configuration Notes
The plugin depends on Qt’s `network` and `dbus` modules and the `nymea` pkg-config package. It communicates with RAUC via D-Bus; ensure the target system exposes the RAUC D-Bus API when running the plugin.
