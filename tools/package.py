"""Build the release archive.

The archive holds the plugin, the readme, and the empty animation files listed
in blank-animations.txt. Those files are generated here rather than stored,
since every one of them is zero bytes and only the path carries meaning.

The layout has no Data folder: a mod manager and a manual install both expect
the archive to unpack into Data, not to contain it.

Usage: package.py <version> [--dll <path>] [--out <path>]
"""
import argparse
import os
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, "tools", "blank-animations.txt")
DESCRIPTION = os.path.join(ROOT, "docs", "nexus-description.bbcode")
CONVERTER = os.path.join(ROOT, "tools", "nexus_to_text.py")
NAME = "360 Directional Movement"


def blank_paths():
    with open(MANIFEST, encoding="utf-8") as f:
        return [line.strip() for line in f
                if line.strip() and not line.startswith("#")]


def readme_text(version):
    result = subprocess.run(
        [sys.executable, CONVERTER, DESCRIPTION, "--title", f"{NAME} {version}"],
        capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(result.stderr.strip() or "readme conversion failed")
    return result.stdout.replace("\n", "\r\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("version")
    parser.add_argument("--dll", default=os.path.join(ROOT, "build", "sf360.dll"))
    parser.add_argument("--out", default=None)
    args = parser.parse_args()

    if not os.path.exists(args.dll):
        sys.exit(f"plugin not found: {args.dll}")

    out = args.out or os.path.join(
        ROOT, f"{NAME.replace(' ', '')}-{args.version}.zip")

    paths = blank_paths()
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.write(args.dll, "SFSE/Plugins/sf360.dll")
        archive.writestr("readme.txt", readme_text(args.version))
        for path in paths:
            archive.writestr(path, b"")

    print(f"{out}\n  plugin, readme, {len(paths)} empty animations")
    return 0


if __name__ == "__main__":
    sys.exit(main())
