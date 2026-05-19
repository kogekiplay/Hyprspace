# Hyprspace

Hyprspace is a Hyprland overview / workspace-expo plugin. It renders a top or bottom workspace strip, shows live workspace thumbnails, and lets you switch or move windows between workspaces.

This fork is adapted for current Hyprland with Lua-based configuration instead of the old `.conf` / Hyprlang plugin config flow.

## Features

- Live workspace overview with window thumbnails
- Multi-monitor support
- Mouse drag/drop between workspaces
- Optional gesture support
- Lua-native configuration
- `hyprpm` install support

## Requirements

- Hyprland with plugin support
- Hyprland development headers / pkg-config metadata for manual builds
- A recent C++ compiler with C++23 support

## Installation

### Manual install

Build the plugin from the repo root:

```bash
cd ~/.config/Hyprspace
make all
```

That produces:

```text
~/.config/Hyprspace/Hyprspace.so
```

Load it manually:

```bash
hyprctl plugin load ~/.config/Hyprspace/Hyprspace.so
```

To unload it:

```bash
hyprctl plugin unload ~/.config/Hyprspace/Hyprspace.so
```

### `hyprpm` install

If this repo is pushed to Git and the updated [hyprpm.toml](./hyprpm.toml) is included, install it with:

```bash
hyprpm add https://github.com/0xl30/Hyprspace.git
hyprpm enable Hyprspace
```

Examples:

```bash
hyprpm add https://github.com/0xl30/Hyprspace.git
hyprpm enable Hyprspace
```

Notes:

- `hyprpm` matches your Hyprland commit against `commit_pins` in `hyprpm.toml`.
- For Hyprland `v0.55.2`, this repo already includes a matching pin.
- After updating plugin code, commit and push before expecting `hyprpm` to fetch the new version.

## Lua setup

### 1. Plugin helper

Add this to start-up [plugins.lua](../plugins.lua):

```lua
require("Hyprspace.Hyprspace").setup()
```

This is the preferred setup for `hyprpm` installs and already-loaded plugins.

If you want local manual loading from the Lua helper instead of loading the `.so` yourself, use:

```lua
require("Hyprspace.Hyprspace").setup({
    plugin_path = HOME .. "/.config/Hyprspace/Hyprspace.so",
})
```

Use that only for a manual local build. Do not pass `plugin_path` when the plugin is managed by `hyprpm`.

### 2. Keybind

Add a keybind in [keybinds.lua](../keybinds.lua):

```lua
hl.unbind("SUPER + A")
hl.bind("SUPER + A", function()
    if hl.plugin and hl.plugin.Hyprspace and type(hl.plugin.Hyprspace.overview) == "function" then
        hl.plugin.Hyprspace.overview("toggle")
    end
end)
```

This file does not need to `require(...)` the helper directly.

### 3. Reload

After changing the Lua config:

```bash
hyprctl reload
```

After rebuilding the plugin binary:

```bash
cd ~/.config/Hyprspace
make all
hyprctl plugin unload ~/.config/Hyprspace/Hyprspace.so
hyprctl plugin load ~/.config/Hyprspace/Hyprspace.so
hyprctl reload
```

If the plugin is installed through `hyprpm`, use:

```bash
hyprpm update
hyprpm reload
hyprctl reload
```

## Usage

### Lua API

The helper module exports:

```lua
local hyprspace = require("Hyprspace.Hyprspace")

hyprspace.setup()
hyprspace.apply_config()
hyprspace.toggle()
hyprspace.overview("open")
hyprspace.overview("close")
hyprspace.reload()
```

### Plugin Lua entrypoint

Once the plugin is loaded, you can also call:

```lua
hl.plugin.Hyprspace.overview("toggle")
hl.plugin.Hyprspace.overview("open")
hl.plugin.Hyprspace.overview("close")
```

## Configuration

The plugin is configured from [Hyprspace.lua](./Hyprspace.lua). The helper builds a Lua config table and applies it through `hl.config(...)`.

### Example

```lua
plugin = {
    hyprspace = {
        panel_height = 220,
        panel_border_width = 2,
        workspace_margin = 10,
        reserved_area = 35,
        workspace_border_size = 1,

        center_aligned = true,
        on_bottom = false,
        draw_active_workspace = true,
        hide_real_layers = false,
        affect_strut = false,

        override_gaps = true,
        gaps_in = 20,
        gaps_out = 60,

        auto_drag = true,
        auto_scroll = true,
        exit_on_click = true,
        exit_on_switch = false,

        disable_gestures = false,
        swipe_fingers = 3,
        swipe_distance = 300,
        swipe_force_speed = 30,
        swipe_cancel_ratio = 0.5,
        click_release_threshold_ms = 200,
    }
}
```

### Main options

#### Colors

- `panel_color`
- `panel_border_color`
- `workspace_active_background`
- `workspace_inactive_background`
- `workspace_active_border`
- `workspace_inactive_border`
- `drag_alpha`
- `disable_blur`

#### Layout

- `panel_height`
- `panel_border_width`
- `workspace_margin`
- `reserved_area`
- `workspace_border_size`
- `adaptive_height`
- `center_aligned`
- `on_bottom`
- `hide_background_layers`
- `hide_top_layers`
- `hide_overlay_layers`
- `draw_active_workspace`
- `hide_real_layers`
- `affect_strut`
- `override_gaps`
- `gaps_in`
- `gaps_out`

#### Behavior

- `auto_drag`
- `auto_scroll`
- `exit_on_click`
- `switch_on_drop`
- `exit_on_switch`
- `show_new_workspace`
- `show_empty_workspace`
- `show_special_workspace`
- `exit_key`

#### Gestures and input

- `disable_gestures`
- `reverse_swipe`
- `swipe_fingers`
- `swipe_distance`
- `swipe_force_speed`
- `swipe_cancel_ratio`
- `swipe_threshold`
- `swipe_closed_padding`
- `workspace_scroll_speed`
- `click_release_threshold_ms`

#### Animation

- `override_anim_speed`

## Theming

By default, [Hyprspace.lua](./Hyprspace.lua) reads colors from:

```text
~/.config/matugen/generated/hyprland-colors.lua
```

If that file does not exist, Hyprspace falls back to built-in colors.

The helper converts Matugen `rgba(rrggbbaa)` strings into the integer color format expected by the plugin.

## Troubleshooting

### The keybind does nothing

Check that the plugin is actually loaded:

```bash
hyprctl plugins
```

If you are using a manual build, load the `.so` first.

### Lua settings are not applied

Make sure [source/plugins.lua](../plugins.lua) calls:

```lua
require("Hyprspace.Hyprspace").setup()
```

Then run:

```bash
hyprctl reload
```

### `hyprpm enable` fails

Check:

- the repo URL is correct
- `hyprpm.toml` contains a pin for your current Hyprland commit
- the plugin builds cleanly on that commit

### The plugin crashes on load

Check the latest Hyprland crash report under:

```text
~/.cache/hyprland/
```

and verify the plugin was built against the same Hyprland version you are running.
