from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class InputDragStateTest(unittest.TestCase):
    def test_workspace_drop_only_uses_overview_owned_drag(self):
        overview_hpp = (ROOT / "src" / "Overview.hpp").read_text()
        input_cpp = (ROOT / "src" / "Input.cpp").read_text()

        self.assertIn("overviewDragActive", overview_hpp)
        self.assertIn("overviewDragActive = true", input_cpp)
        self.assertIn("overviewDragActive && targetWindow", input_cpp)
        self.assertIn("overviewDragActive = false", input_cpp)

    def test_real_window_decorations_can_receive_clicks_while_overview_is_active(self):
        overview_hpp = (ROOT / "src" / "Overview.hpp").read_text()
        input_cpp = (ROOT / "src" / "Input.cpp").read_text()

        self.assertIn("passingThroughActiveWindowDecoration", overview_hpp)
        self.assertIn("activeWorkspaceDecorationAt", input_cpp)
        self.assertIn("Desktop::View::RESERVED_EXTENTS", input_cpp)
        self.assertIn("Desktop::View::INPUT_EXTENTS", input_cpp)
        self.assertIn("Desktop::View::WINDOW_ONLY", input_cpp)
        self.assertIn("return true;", input_cpp)

    def test_toggle_all_closes_when_any_overview_widget_is_active(self):
        main_cpp = (ROOT / "src" / "main.cpp").read_text()

        self.assertIn("bool anyOverviewActive()", main_cpp)
        self.assertIn("const bool anyActive = anyOverviewActive();", main_cpp)
        self.assertNotIn("const bool anyActive = widget->isActive();", main_cpp)
        self.assertLess(main_cpp.index('if (arg.contains("all"))'), main_cpp.index("const auto currentMonitor"))

    def test_lua_toggle_uses_active_state_from_before_config_reload(self):
        main_cpp = (ROOT / "src" / "main.cpp").read_text()
        lua_overview = main_cpp[main_cpp.index("int luaOverview") :]

        self.assertIn("const bool anyActiveBeforeReload = anyOverviewActive();", lua_overview)
        self.assertLess(lua_overview.index("const bool anyActiveBeforeReload = anyOverviewActive();"), lua_overview.index("reloadConfig();"))
        self.assertIn("toggleAllOverviews(anyActiveBeforeReload);", lua_overview)


if __name__ == "__main__":
    unittest.main()
