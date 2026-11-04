from pathlib import Path
import tempfile
import unittest
import zipfile

from make_windows_zip import make_windows_zip


class WindowsZipTests(unittest.TestCase):
    def test_hidden_files_and_regular_extraction(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "game"
            (source / "Resources").mkdir(parents=True)
            (source / ".itch.toml").write_bytes(b'path = "dlrl.exe"\n')
            (source / "DOCUMENTATION.md").write_bytes(b"Game documentation\n")
            (source / "Resources/rom.bin").write_bytes(bytes(range(256)))
            destination = make_windows_zip(source, root / "game.zip")
            with zipfile.ZipFile(destination) as archive:
                self.assertNotIn("./", archive.namelist())
                self.assertIn(".itch.toml", archive.namelist())
                self.assertTrue(all(not n.startswith("./") for n in archive.namelist()))
                self.assertIsNone(archive.testzip())
                archive.extractall(root / "extracted")
            expected = {p.relative_to(source): p.read_bytes() for p in source.rglob("*") if p.is_file()}
            actual = {p.relative_to(root / "extracted"): p.read_bytes()
                      for p in (root / "extracted").rglob("*") if p.is_file()}
            self.assertEqual(actual, expected)

    def test_archive_cannot_include_itself(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(ValueError, "outside"):
                make_windows_zip(root, root / "game.zip")


if __name__ == "__main__":
    unittest.main()
