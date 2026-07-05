from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class OverviewRealSurfacesTest(unittest.TestCase):
    def test_overview_disables_blur_for_real_workspace_windows(self):
        overview_hpp = (ROOT / "src" / "Overview.hpp").read_text()
        overview_cpp = (ROOT / "src" / "Overview.cpp").read_text()

        self.assertIn("oWindowNoBlur", overview_hpp)
        self.assertIn("applyWindowNoBlur", overview_cpp)
        self.assertIn("noBlur().set(true", overview_cpp)
        self.assertIn("noBlur().unset", overview_cpp)

    def test_window_preview_renders_real_surfaces_directly(self):
        render_cpp = (ROOT / "src" / "Render.cpp").read_text()

        self.assertIn("#include <hyprland/src/render/pass/SurfacePassElement.hpp>", render_cpp)
        self.assertIn("CSurfacePassElement::SRenderData renderData", render_cpp)
        self.assertIn("renderData.clipBox            = clipBox", render_cpp)
        self.assertIn("makeUnique<CSurfacePassElement>(renderData)", render_cpp)
        self.assertNotIn("(*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), window, monitor", render_cpp)

    def test_window_preview_refreshes_hidden_surface_buffers(self):
        render_cpp = (ROOT / "src" / "Render.cpp").read_text()

        self.assertIn("refreshWindowPreviewSurfaces", render_cpp)
        self.assertIn("unlockFirst(LOCK_REASON_FENCE | LOCK_REASON_FIFO | LOCK_REASON_TIMER)", render_cpp)
        self.assertIn("presentFeedback(time, monitor, true)", render_cpp)

    def test_workspace_preview_can_crop_top_empty_area(self):
        globals_hpp = (ROOT / "src" / "Globals.hpp").read_text()
        main_cpp = (ROOT / "src" / "main.cpp").read_text()
        render_cpp = (ROOT / "src" / "Render.cpp").read_text()

        self.assertIn("workspacePreviewCropTop", globals_hpp)
        self.assertIn("plugin:hyprspace:workspace_preview_crop_top", main_cpp)
        self.assertIn("readIntValue(g_pluginConfigValues.workspacePreviewCropTop", main_cpp)
        self.assertIn("previewCropTop", render_cpp)
        self.assertIn("owner->m_position.y - previewCropTop", render_cpp)
        self.assertIn("previewOrigin", render_cpp)
        self.assertIn("Config::affectStrut && previewCropTop <= 0.", render_cpp)

    def test_overview_uses_fullscreen_blurred_background_without_panel_mask(self):
        globals_hpp = (ROOT / "src" / "Globals.hpp").read_text()
        main_cpp = (ROOT / "src" / "main.cpp").read_text()
        render_cpp = (ROOT / "src" / "Render.cpp").read_text()

        self.assertIn("overviewBackgroundColor", globals_hpp)
        self.assertIn("plugin:hyprspace:overview_background_color", main_cpp)
        self.assertIn("readColorValue(g_pluginConfigValues.overviewBackgroundColor", main_cpp)
        self.assertIn("renderRectWithBlur(monitorClip, Config::overviewBackgroundColor)", render_cpp)
        self.assertNotIn("renderRectWithBlur(panelBox, Config::panelBaseColor)", render_cpp)

    def test_overview_temporarily_disables_hyprbars_blur(self):
        overview_hpp = (ROOT / "src" / "Overview.hpp").read_text()
        overview_cpp = (ROOT / "src" / "Overview.cpp").read_text()

        self.assertIn("suppressingBarBlur", overview_hpp)
        self.assertIn('CConfigValue<Config::BOOL>("plugin:hyprbars:bar_blur")', overview_cpp)
        self.assertIn("disableOverviewBarBlur", overview_cpp)
        self.assertIn("restoreOverviewBarBlur", overview_cpp)
        self.assertIn("*barBlur = false", overview_cpp)
        self.assertIn("*barBlur = *oHyprbarsBarBlur", overview_cpp)

    def test_real_layer_hiding_can_keep_selected_namespaces(self):
        globals_hpp = (ROOT / "src" / "Globals.hpp").read_text()
        main_cpp = (ROOT / "src" / "main.cpp").read_text()
        overview_cpp = (ROOT / "src" / "Overview.cpp").read_text()

        self.assertIn("keepRealLayerNamespaces", globals_hpp)
        self.assertIn("plugin:hyprspace:keep_real_layer_namespaces", main_cpp)
        self.assertIn("readStringValue(g_pluginConfigValues.keepRealLayerNamespaces", main_cpp)
        self.assertIn("shouldKeepRealLayer", overview_cpp)
        self.assertIn("m_namespace", overview_cpp)
        self.assertIn("setValueAndWarp(0.F)", overview_cpp)

    def test_active_overview_refresh_rehides_real_layers(self):
        overview_cpp = (ROOT / "src" / "Overview.cpp").read_text()
        render_cpp = (ROOT / "src" / "Render.cpp").read_text()

        self.assertNotIn("if (active)\n        return;", overview_cpp)
        self.assertIn("if (active)\n        hideRealLayers(owner);", render_cpp)
        self.assertIn("alreadyHidden", overview_cpp)
        self.assertIn("layerAlpha(layer)->setValueAndWarp(0.F);", overview_cpp)


if __name__ == "__main__":
    unittest.main()
