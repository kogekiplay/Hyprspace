from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class GapOverrideConfigTest(unittest.TestCase):
    def test_gap_override_is_registered_reloaded_and_applied(self):
        globals_hpp = (ROOT / "src" / "Globals.hpp").read_text()
        main_cpp = (ROOT / "src" / "main.cpp").read_text()
        layout_cpp = (ROOT / "src" / "Layout.cpp").read_text()

        for symbol in ("overrideGaps", "gapsIn", "gapsOut"):
            self.assertIn(f"Config::{symbol}", main_cpp)
            self.assertIn(symbol, globals_hpp)

        for option in ("override_gaps", "gaps_in", "gaps_out"):
            self.assertIn(f"plugin:hyprspace:{option}", main_cpp)

        self.assertIn("readBoolValue(g_pluginConfigValues.overrideGaps", main_cpp)
        self.assertIn("readIntValue(g_pluginConfigValues.gapsIn", main_cpp)
        self.assertIn("readIntValue(g_pluginConfigValues.gapsOut", main_cpp)

        self.assertIn("Config::overrideGaps", layout_cpp)
        self.assertIn("Config::gapsIn", layout_cpp)
        self.assertIn("Config::gapsOut", layout_cpp)
        self.assertIn("gapsin:", layout_cpp)
        self.assertIn("gapsout:", layout_cpp)


if __name__ == "__main__":
    unittest.main()
