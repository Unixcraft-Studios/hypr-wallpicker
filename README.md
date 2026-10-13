# Hypr-wallpicker

<div align="center">
  <img src="./demonstration/logo.png" alt="Hypr-wallpicker Logo" width="250">
</div>

Hypr-wallpicker a standalone hexagonal wallpaper selector... for Both Wayland and X11

![showcase](./demonstration/showcase.gif)

## Features

* **Hexagonal grid UI:** It calculates and renders a proper hexagonal grid for wallpaper thumbnails.
* **Animations:** It provides scale and brightness transitions on mouse hover.
* **Versioned thumbnail cache**: It automatically resizes, crops, and applies a hex-mask to high-res wallpapers for fast subsequent loads without reusing stale or colliding thumbnails.
* **JPEG support independent of Raylib build flags**: It decodes `.jpg` and `.jpeg` source wallpapers through `libjpeg`, even when the system Raylib package has JPEG loading disabled.
* **Transition:** Integrates with `awww` to trigger ripple transitions directly from the absolute screen coordinates of the clicked hex tile.
* **Color extraction support:** It supports `matugen` upon selection to dynamically adjust your system theme color based on the chosen wallpaper.

By utilizing raw `C` and `Raylib`, it bypasses the overhead and bloated dependencies typical of modern JS/Electron UI frameworks.

This tool adheres to the Unix philosophy—it handles the visual selection of wallpapers and delegates the actual desktop drawing to Wayland daemons (`awww`) and theming engines (`matugen`) via standard shell commands. 

## Dependencies

### Core / Build Dependencies

To build `hypr-wallpicker`, you need:

- `gcc`
- `raylib`
- `libjpeg-turbo` development headers (`libjpeg` via `pkg-config`) for JPEG wallpaper decoding

`hypr-wallpicker` links against your system `raylib` library.

Thumbnail cache entries are stored below
`$XDG_CACHE_HOME/wallpicker/thumbnails-v2/` (or
`~/.cache/wallpicker/thumbnails-v2/` when `XDG_CACHE_HOME` is unset). Their
identity includes the source file and thumbnail recipe, so changing an image or
the grid angle produces a fresh thumbnail. Existing legacy cache files are left
untouched.

---

### Runtime configuration

Save your wallpaper directory and grid preferences in a TOML file. Wallpicker
loads these settings each time it starts, and command-line options override
the saved values.

Place the file in `~/.config/hypr-wallpicker/runtime/`, using the name for your
session:

- Wayland: `runtime-wayland.toml`
- X11: `runtime-x11.toml`

If you use `XDG_CONFIG_HOME`, place it under
`$XDG_CONFIG_HOME/hypr-wallpicker/runtime/` instead. Wallpicker uses its built-in
defaults when the session's configuration file is absent.

```toml
schema_version = 1

[wallpaper]
directory = "~/Pictures/wallpapers"

[renderer]
cols = 5
spacing = 15.0
angle = 0.0
```

Keep `schema_version = 1` and include the settings you want to customize.
Omitted settings use their defaults. Set `directory` to an absolute path or a
path beginning with `~/`. Use a positive integer for `cols`, a non-negative
pixel value for `spacing`, and degrees for `angle`.

Ready-to-use examples are available for
[Wayland](examples/runtime-wayland.toml) and [X11](examples/runtime-x11.toml).
You can also select a file with `--config` and adjust individual settings for
one launch:

```sh
wallpicker --config examples/runtime-wayland.toml --cols 7
```

To check your settings, use `--check-config`. It validates the configuration,
prints the effective values as TOML, and exits:

```sh
wallpicker --check-config
wallpicker --config examples/runtime-wayland.toml --check-config
```

Run `wallpicker --help` to see all command-line options.

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
- `feh` or `nitrogen` — set wallpaper on X11
- `wal` / `pywal` *(optional)* — generate colors from the selected wallpaper
- `i3-msg` *(optional)* — reload or restart i3-managed components
- `polybar-msg` *(optional)* — reload Polybar if used
- custom user scripts for compositor, bar, or theme reloads

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

Perform a clean rebuild and reinstall:

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
The first time you use this program, it will generate cached files of wallpapers, which may take a short time. Please be 
patient.

---

### Example hook scripts

Example hook scripts are provided in:

`scripts/` in project root.

To use one, copy it to the runtime hook path expected by `hypr-wallpicker`:

`~/.config/hypr-wallpicker/apply-wallpaper.sh`

Wayland / Hyprland example:

```bash
mkdir -p ~/.config/hypr-wallpicker
cp scripts/apply-wallpaper-wayland.example.sh ~/.config/hypr-wallpicker/apply-wallpaper.sh
chmod +x ~/.config/hypr-wallpicker/apply-wallpaper.sh
```
X11 / i3 example:

```bash
mkdir -p ~/.config/hypr-wallpicker
cp scripts/apply-wallpaper-x11.example.sh ~/.config/hypr-wallpicker/apply-wallpaper.sh
chmod +x ~/.config/hypr-wallpicker/apply-wallpaper.sh
```

---

### Display the interface in full-screen mode

In Hyprland, please add this in `~/.config/hypr/hyprland.lua`:
```
hl.window_rule({
    name         = "wall-paper-picker",
    match        = { class = "^(wallpicker)$" },
    fullscreen   = true,
    center       = true,
    stay_focused = true,
})
```
