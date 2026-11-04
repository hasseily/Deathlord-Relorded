import io
from pathlib import Path
import tarfile
import tempfile
import unittest
import zipfile

from verify_archive import verify_archive


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.doc = self.root / "DOCUMENTATION.md"
        self.doc.write_bytes(b"# Deathlord Relorded\nTest documentation\n")
        self.hdv = self.root / "clean.hdv"
        self.hdv.write_bytes(b"synthetic clean game image")
        self.commit = "a" * 40

    def package(self, platform, *, omit=None, replace=None):
        resources = "dlrl.app/Contents/Resources/" if platform == "macos" else ""
        files = {
            "DOCUMENTATION.md": self.doc.read_bytes(),
            "README.md": b"Tester instructions",
            ".itch.toml": b"[[actions]]",
            "BUILD-INFO.txt": f"Commit: {self.commit}\n".encode(),
            resources + "Images/Deathlord PRODOS.hdv": self.hdv.read_bytes(),
        }
        if platform == "macos":
            files["dlrl.app/Contents/MacOS/dlrl"] = b"app executable"
            files[resources + "DOCUMENTATION.md"] = files["DOCUMENTATION.md"]
            files[resources + "BUILD-INFO.txt"] = files["BUILD-INFO.txt"]
        elif platform == "windows":
            files["dlrl.exe"] = b"executable"
            files["SDL3.dll"] = b"runtime"
        else:
            files["dlrl"] = b"executable"
        if omit is not None:
            files.pop(omit)
        if replace:
            files.update(replace)
        archive = self.root / (platform + (".tar.gz" if platform == "linux" else ".zip"))
        if platform == "linux":
            with tarfile.open(archive, "w:gz") as container:
                for name, data in files.items():
                    member = tarfile.TarInfo("dlrl-linux-x64/" + name)
                    member.size = len(data)
                    container.addfile(member, io.BytesIO(data))
        else:
            with zipfile.ZipFile(archive, "w") as container:
                for name, data in files.items():
                    container.writestr(name if platform == "windows" else "./" + name, data)
        return archive

    def verify(self, platform, **options):
        return verify_archive(self.package(platform, **options), platform,
                              self.doc, self.hdv, self.commit)

    def test_all_three_package_layouts(self):
        for platform in ("windows", "linux", "macos"):
            with self.subTest(platform=platform):
                self.assertEqual(self.verify(platform)["commit"], self.commit)

    def test_documentation_must_be_at_archive_root(self):
        for platform in ("windows", "linux", "macos"):
            with self.subTest(platform=platform), self.assertRaisesRegex(ValueError, "DOCUMENTATION.md"):
                self.verify(platform, omit="DOCUMENTATION.md")

    def test_stale_documentation_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "DOCUMENTATION.md"):
            self.verify("windows", replace={"DOCUMENTATION.md": b"old documentation"})

    def test_wrong_source_commit_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "source commit"):
            self.verify("windows", replace={"BUILD-INFO.txt": b"Commit: wrong\n"})

    def test_saved_party_image_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "clean HDV"):
            self.verify("linux", replace={"Images/Deathlord PRODOS.hdv": b"saved party"})

    def test_mac_documentation_stays_with_the_app(self):
        with self.assertRaisesRegex(ValueError, "DOCUMENTATION.md"):
            self.verify("macos", omit="dlrl.app/Contents/Resources/DOCUMENTATION.md")

    def test_windows_dot_root_entry_is_rejected(self):
        archive = self.package("windows")
        with zipfile.ZipFile(archive, "a") as container:
            container.writestr("./", b"")
        with self.assertRaisesRegex(ValueError, "Non-portable Windows ZIP entry"):
            verify_archive(archive, "windows", self.doc, self.hdv, self.commit)

    def test_windows_dot_prefixed_file_is_rejected(self):
        archive = self.package("windows")
        with zipfile.ZipFile(archive, "a") as container:
            container.writestr("./extra.txt", b"extra")
        with self.assertRaisesRegex(ValueError, "Non-portable Windows ZIP entry"):
            verify_archive(archive, "windows", self.doc, self.hdv, self.commit)

    def test_corruption_in_an_extra_file_is_rejected(self):
        archive = self.package("windows")
        with zipfile.ZipFile(archive, "a") as container:
            container.writestr("extra.bin", b"test payload")
            info = container.getinfo("extra.bin")
        data = bytearray(archive.read_bytes())
        data[info.header_offset + 30 + len(info.filename)] ^= 1
        archive.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "ZIP CRC check failed"):
            verify_archive(archive, "windows", self.doc, self.hdv, self.commit)


if __name__ == "__main__":
    unittest.main()
