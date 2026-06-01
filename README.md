<p align="center">
  <img src="assets/logo.png" width="160" alt="Lumi logo">
</p>

<h1 align="center">Lumi</h1>

<p align="center">A lightweight Wayland dock written in C++.</p>

## Highlights

- Wayland layer-shell based dock
- Configurable layout, sizing, and animations
- YAML config for apps and appearance

> **Note:** Lumi works only on wlroots-based window managers with support for the protocols listed in the `protocols` directory.


## Build

Requirements:

- CMake 3.14+
- C++23 compiler
- pkg-config
- wayland-client, wayland-egl
- egl
- epoxy

Build steps:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Usage

Run the dock:

```bash
./build/lumi
```

Lumi loads config from:

- `$XDG_CONFIG_HOME/lumi/dock.yaml`
- `~/.config/lumi/dock.yaml`

Example config:

```yaml
looks:
  cornerRadius: 28
  padding:
    left: 10
    right: 10
    top: 10
    bottom: 10
  margin:
    left: 10
    right: 10
    top: 0
    bottom: 0
  itemSize: 48
  itemSpacing: 14
  activeDotSize: 6
  maxScale: 1.5
  maxLiftAmount: 2

items:
  - kitty
  - firefox
  - class: code
    Icon: code
    Exec: code
    StartupWMClass: code
```

## Config Reference

Lumi reads a single YAML file and applies defaults for any missing values.

`looks.cornerRadius`
Rounded corner radius for the dock background (in pixels).

`looks.padding`
Inner padding for the dock content area. Each side is measured in pixels.

`looks.margin`
Outer margin from the screen edge. Each side is measured in pixels.

`looks.itemSize`
Base icon size in pixels before hover scaling.

`looks.itemSpacing`
Gap in pixels between items.

`looks.activeDotSize`
Size in pixels of the active app indicator dot.

`looks.maxScale`
Maximum hover scale multiplier for icons. Keep this low for smoother animations
and lower GPU load.

`looks.maxLiftAmount`
Maximum vertical lift in pixels during hover.

`items`
Ordered list of dock entries. Each entry can be:

- A string, which is treated as an app class name.
- A map with `class` and optional desktop entry hints.

`items[].class`
Application class name used to match running windows.

`items[].Icon`
Desktop entry icon name override.

`items[].Exec`
Desktop entry executable override.

`items[].StartupWMClass`
Desktop entry StartupWMClass override.

## Contributing

- Open an issue for bugs or feature proposals
- Keep changes focused and include a clear description
- Match existing code style and C++ conventions