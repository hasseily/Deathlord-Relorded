"""Create a Windows ZIP with ordinary relative paths and included dotfiles."""

import argparse
from pathlib import Path
import zipfile


def make_windows_zip(source, destination):
    source = Path(source).resolve()
    destination = Path(destination).resolve()
    if not source.is_dir():
        raise ValueError(f"Missing package directory: {source}")
    if destination.is_relative_to(source):
        raise ValueError("The ZIP must be written outside the package directory")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED,
                         strict_timestamps=False) as archive:
        for path in sorted(source.rglob("*")):
            if path.is_symlink():
                raise ValueError(f"Windows packages cannot contain symlinks: {path}")
            # rglob includes hidden files but never emits the source root.
            archive.write(path, path.relative_to(source).as_posix())
    return destination


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    options = parser.parse_args()
    print(make_windows_zip(options.source, options.destination))
