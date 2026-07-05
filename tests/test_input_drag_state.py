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


if __name__ == "__main__":
    unittest.main()
