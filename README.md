# Hypr-wallpicker

<div align="center">
  <img src="./demonstration/logo.png" alt="Hypr-wallpicker Logo" width="250">
</div>

Hypr-wallpicker a standalone hexagonal wallpaper selector... for Both Wayland and X11!

![showcase](./demonstration/showcase.gif)

## Features

* **Hexagonal grid UI:** It calculates and renders a proper hexagonal grid for wallpaper thumbnails.
* **Animations:** It provides scale and brightness transitions on mouse hover.
* **Auto-caching**: It automatically resizes, crops, and applies a hex-mask to high-res wallpapers for fast subsequent loads.
* **Transition:** Integrates with `awww` to trigger ripple transitions directly from the absolute screen coordinates of the clicked hex tile.
* **Color extraction support:** It supports `matugen` upon selection to dynamically adjust your system theme color based on the chosen wallpaper.

By utilizing raw `C` and `Raylib`, it bypasses the overhead and bloated dependencies typical of modern JS/Electron UI frameworks.

This tool adheres to the Unix philosophy, it handles the visual selection of wallpapers and delegates the actual desktop drawing to Wayland daemons (`awww`) and theming engines (`matugen`) via standard shell commands. 

### Core / Build Dependencies

To build `hypr-wallpicker`, you need:

- GCC (C compiler)
- GNU Make
- CMake
- Git (including submodule support)
- Platform development libraries required by raylib.

**raylib 6.2 is included as a pinned Git submodule** under `vendor/raylib/`.

A system-installed raylib package is no longer required. 

The build system compiles the vendored raylib source into a static library (`libraylib.a`) and links it with the application. This ensures a consistent raylib version across builds without depending on the distribution's installed raylib release.

### raylib Build Configuration

raylib is compiled through CMake with customized build options:

```
-DCMAKE_BUILD_TYPE=Release
-DBUILD_SHARED_LIBS=OFF 
-DCMAKE_POLICY_VERSION_MINIMUM=3.10 
-DBUILD_EXAMPLES=OFF 
-DCUSTOMIZE_BUILD=ON 
-DSUPPORT_FILEFORMAT_JPG=ON
-DSUPPORT_MODULE_RAUDIO=ON 
-DSUPPORT_FILEFORMAT_MOD=OFF 
-DSUPPORT_FILEFORMAT_XM=OFF
```

The main application source is compiled with `-O2 -Wall -Wextra -Werror`. 

raylib is compiled separately using CMake, using `--parallel` to speep up compilation.

Static linking applies to raylib, other system libraries may remain dynamically linked.

---

### Build

Clone the repository with its Git submodules:

```bash
git clone --recurse-submodules https://github.com/Horizon0427/hypr-wallpicker.git
cd hypr-wallpicker
```

Build the release binary:

```bash
make
```

> The build system compiles raylib 6.2 from the vendored source and links it with `hypr-wallpicker`. `hypr-wallpicker` does not require or interfere with your system build of raylib if you have one.

To force fresh raylib CMake build:

```bash
rm -rf vendor/raylib/build
make
```
---

### Runtime Behavior

`hypr-wallpicker` itself only handles wallpaper selection and thumbnail caching.

When a wallpaper is selected, it:

1. checks for an optional user hook at
   `~/.config/hypr-wallpicker/apply-wallpaper.sh`
2. if no hook is present, it falls back to the built-in apply behavior

The built-in fallback is currently **Hyprland/Wayland-oriented**.

---

### Built-in Fallback Runtime Integrations (No User Hook)

If no user hook is installed, the built-in fallback may use `awww` *(optional)* for animated wallpaper transitions **if available**. 


>The built-in fallback is intentionally kept bare-minimum. To use color extraction or reload desktop components, please configure a custom hook.

---

### Wayland / Hyprland Example Hook

If you use the provided Wayland example hook (designed as a tutorial template), you can easily uncomment sections to enable:

- `awww` **(required)** — apply wallpaper + transition
- `matugen` *(optional)* — generate colors from the selected wallpaper
- `hyprctl` *(optional)* — reload Hyprland
- `makoctl` *(optional)* — reload Mako notifications
- `swaync-client` *(optional)* — reload SwayNC

> The provided Wayland example hook requires `awww`, but it can be replaced by other utils such as `hyprpaper` in your own shell script. 

---

### X11 / i3 Example Hook

If you use the provided X11 example hook, the exact runtime tools depend on your desktop setup and how you choose to manage wallpapers, theming, and bar reloads.

Common options include:

- `i3wm` **(required)**
- `feh` or `nitrogen` - set wallpaper on X11
- ![Pythonic-Palette-Generator](https://github.com/Unixcraft-Studios/Pythonic-Palette-Generator) / `pywal` *(optional)* - generate colors from the selected wallpaper.
- `rofi` *(optional)* - often used in place of `dmenu`(default).
- `polybar` *(optional)* - often used in place of `i3bar`(default).
- `picom` *(optional but recommended)*
- custom scripts for compositor, monitor/s config(via `xrandr`), bar, or other environment changes should all reside in `~/.config/i3` and be called via i3 through `~/.config`.

> The X11 example hook is a starting point and may need adjustments depending on your environment.
> (only supported environment right now is i3)

## Build, Install & Run

### Build

Build the release binary:

```bash
make
```
---

### Install

Install the binary to `/usr/local/bin`:

```bash
sudo make install
```

Remove the installed binary:

```bash
sudo make uninstall
```

Reinstall the main binary:

```bash
sudo make reinstall
```
---

### Run

Run with the default wallpaper directory at `~/Pictures/wallpapers`:

```bash
wallpicker
```

Run with a custom wallpaper directory:

```bash
wallpicker /path/to/wallpapers
```
The first time you use this program, it will generate cached files of wallpapers.

---

### Example hook scripts

Example hook scripts are provided in:

`scripts/` in project root.

To use one, copy it to the runtime hook path expected by `hypr-wallpicker`:

`~/.config/hypr-wallpicker/apply-wallpaper.sh`

Wayland/Hyprland example:

```bash
mkdir -p ~/.config/hypr-wallpicker
cp scripts/apply-wallpaper-wayland.example.sh ~/.config/hypr-wallpicker/apply-wallpaper.sh
chmod +x ~/.config/hypr-wallpicker/apply-wallpaper.sh
```
X11/i3 example:

```bash
mkdir -p ~/.config/hypr-wallpicker
cp scripts/apply-wallpaper-x11.example.sh ~/.config/hypr-wallpicker/apply-wallpaper.sh
chmod +x ~/.config/hypr-wallpicker/apply-wallpaper.sh
```

---
