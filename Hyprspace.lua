local M = {}
local HOME = os.getenv("HOME") or ""

local MATUGEN_PATH = HOME .. "/.config/matugen/generated/hyprland-colors.lua"
local pluginPath = os.getenv("HYPRSPACE_PLUGIN_PATH")

local function file_exists(path)
    if type(path) ~= "string" or path == "" then
        return false
    end

    local file = io.open(path, "r")
    if not file then
        return false
    end

    file:close()
    return true
end

local function shell_quote(value)
    return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

local function load_lua_file(path)
    local env = {}
    local chunk = loadfile(path, "t", env)
    if not chunk then
        return {}
    end

    if not pcall(chunk) then
        return {}
    end

    return env
end

local function rgba_to_aarrggbb(value, fallback)
    if type(value) ~= "string" then
        return fallback
    end

    local hex = value:match("^rgba%((%x%x%x%x%x%x%x%x)%)$")
    if not hex then
        return fallback
    end

    local rr = hex:sub(1, 2)
    local gg = hex:sub(3, 4)
    local bb = hex:sub(5, 6)
    local aa = hex:sub(7, 8)
    return tonumber("0x" .. aa .. rr .. gg .. bb)
end

local function color_with_alpha(value, alpha, fallback)
    local base = rgba_to_aarrggbb(value, fallback)
    if not base then
        return fallback
    end

    local rgb = base & 0x00ffffff
    return ((alpha & 0xff) << 24) | rgb
end

local function matugen_colors()
    return load_lua_file(MATUGEN_PATH)
end

local function build_config()
    local colors = matugen_colors()

    return {
        plugin = {
            hyprspace = {
                -- Theme colors are derived from the current matugen palette, but
                -- each slot keeps an explicit fallback so the overview remains
                -- usable even when the generated file is missing.
                panel_color                   = color_with_alpha(colors.surface_container_high or colors.surface, 0xd8, 0xd8261d20),
                panel_border_color            = color_with_alpha(colors.primary or colors.outline, 0x88, 0x88ffb0cf),
                workspace_active_background   = color_with_alpha(colors.surface_container_highest or colors.surface_container_high, 0xb8, 0xb83c3235),
                workspace_inactive_background = color_with_alpha(colors.surface_container or colors.surface, 0xde, 0xde261d20),
                workspace_active_border       = color_with_alpha(colors.primary or colors.on_surface, 0xb0, 0xb0ffb0cf),
                workspace_inactive_border     = color_with_alpha(colors.outline_variant or colors.outline, 0x55, 0x55504348),

                -- Layout tuning:
                -- Keep the overview compact and reserve only the Waybar strip.
                -- `panel_height` is the visible overview height.
                -- `reserved_area` is the top gap already occupied by Waybar.
                panel_height                   = 220,
                panel_border_width             = 2,
                workspace_margin               = 10,
                reserved_area                  = 35,
                workspace_border_size          = 1,

                adaptive_height                = false,
                center_aligned                 = true,
                on_bottom                      = false,
                hide_background_layers         = false,
                hide_top_layers                = false,
                hide_overlay_layers            = false,
                draw_active_workspace          = true,
                hide_real_layers               = false,
                affect_strut                   = false,

                override_gaps                  = true,
                gaps_in                        = 20,
                gaps_out                       = 60,

                auto_drag                      = true,
                auto_scroll                    = true,
                exit_on_click                  = true,
                switch_on_drop                 = false,
                exit_on_switch                 = false,
                show_new_workspace             = true,
                show_empty_workspace           = true,
                show_special_workspace         = false,

                disable_gestures               = false,
                reverse_swipe                  = false,
                swipe_fingers                  = 3,
                swipe_distance                 = 300,
                swipe_force_speed              = 30,
                swipe_cancel_ratio             = 0.5,
                swipe_threshold                = 10.0,
                swipe_closed_padding           = 10.0,
                workspace_scroll_speed         = 2.0,

                disable_blur                   = false,
                override_anim_speed            = 0.0,
                drag_alpha                     = 0.2,
                exit_key                       = "Escape",
                click_release_threshold_ms     = 200,
            },
        },
    }
end

local function plugin_loaded()
    return hl.plugin and hl.plugin.Hyprspace and type(hl.plugin.Hyprspace.overview) == "function"
end

local function ensure_plugin_loaded()
    if plugin_loaded() then
        return true
    end

    -- hyprpm restores plugins itself. Only fall back to a manual binary path
    -- when one was explicitly provided.
    if not file_exists(pluginPath) then
        return false
    end

    hl.exec_cmd("hyprctl plugin load " .. shell_quote(pluginPath))
    return plugin_loaded()
end

function M.set_plugin_path(path)
    pluginPath = path
end

function M.apply_config()
    if not plugin_loaded() then
        return false
    end

    hl.config(build_config())
    return true
end

function M.reload()
    M.apply_config()
    hl.exec_cmd("hyprctl reload")
end

function M.overview(action)
    if not plugin_loaded() then
        return false
    end

    M.apply_config()
    hl.plugin.Hyprspace.overview(action or "toggle")
    return true
end

function M.toggle()
    return M.overview("toggle")
end

function M.setup(opts)
    if type(opts) == "table" and type(opts.plugin_path) == "string" then
        pluginPath = opts.plugin_path
    end

    -- Startup path:
    -- 1. hyprpm-managed plugins are already loaded by Hyprland
    -- 2. local/manual installs can opt-in by passing `plugin_path`
    -- 3. apply the latest Lua config once the plugin is available
    hl.on("hyprland.start", function()
        ensure_plugin_loaded()
        M.apply_config()
    end)

    -- Config reload path:
    -- if the plugin is already present, refresh its settings immediately.
    if plugin_loaded() then
        M.apply_config()
    end
end

return M
