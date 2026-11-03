"""Verify the user-facing files and provenance in an actual tester archive."""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile
import zipfile


def verify_archive(archive, platform, documentation, clean_hdv, commit):
    archive = Path(archive)
    if platform not in {"windows", "linux", "macos"}:
        raise ValueError(f"Unsupported platform: {platform}")
    if len(commit) != 40 or any(c not in "0123456789abcdef" for c in commit):
        raise ValueError("Expected a full source commit SHA")

    container = zipfile.ZipFile(archive) if zipfile.is_zipfile(archive) else tarfile.open(archive)
    with container:
        if isinstance(container, zipfile.ZipFile):
            files = {item.filename: item for item in container.infolist() if not item.is_dir()}
            read = lambda item: container.read(item)
        else:
            files = {item.name: item for item in container.getmembers() if item.isfile()}
            read = lambda item: container.extractfile(item).read()

        normalized = {}
        for name, item in files.items():
            path = PurePosixPath(name)
            if path.is_absolute() or ".." in path.parts:
                raise ValueError(f"Unsafe archive member: {name}")
            key = str(path)
            if key in normalized:
                raise ValueError(f"Duplicate archive member: {key}")
            normalized[key] = item

        prefix = "dlrl-linux-x64/" if platform == "linux" else ""

        def contents(relative):
            path = prefix + relative
            if path not in normalized:
                raise ValueError(f"Missing archive file: {path}")
            return read(normalized[path])

        expected_doc = Path(documentation).read_bytes()
        if not expected_doc or contents("DOCUMENTATION.md") != expected_doc:
            raise ValueError("Root DOCUMENTATION.md does not match the source")
        contents("README.md")
        contents(".itch.toml")
        info = contents("BUILD-INFO.txt")
        if f"Commit: {commit}" not in info.decode("utf-8").splitlines():
            raise ValueError("BUILD-INFO.txt does not identify the selected source commit")

        resources = "dlrl.app/Contents/Resources/" if platform == "macos" else ""
        hdv_hash = hashlib.sha256(Path(clean_hdv).read_bytes()).hexdigest()
        if hashlib.sha256(contents(resources + "Images/Deathlord PRODOS.hdv")).hexdigest() != hdv_hash:
            raise ValueError("Packaged game image does not match the selected clean HDV")

        if platform == "macos":
            contents("dlrl.app/Contents/MacOS/dlrl")
            if contents(resources + "DOCUMENTATION.md") != expected_doc:
                raise ValueError("App-bundled DOCUMENTATION.md does not match the source")
            if contents(resources + "BUILD-INFO.txt") != info:
                raise ValueError("App-bundled BUILD-INFO.txt does not match the root copy")
        elif platform == "windows":
            contents("dlrl.exe")
            contents("SDL3.dll")
        else:
            contents("dlrl")

        return {"archive": archive.name, "platform": platform, "commit": commit,
                "documentation_sha256": hashlib.sha256(expected_doc).hexdigest(),
                "clean_hdv_sha256": hdv_hash, "files": len(normalized)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--platform", required=True, choices=["windows", "linux", "macos"])
    parser.add_argument("--documentation", type=Path, required=True)
    parser.add_argument("--clean-hdv", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    options = parser.parse_args()
    result = verify_archive(options.archive, options.platform, options.documentation,
                            options.clean_hdv, options.commit)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
