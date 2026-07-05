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
