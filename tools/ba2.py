"""Starfield .ba2 archive reader (BTDX v2/v3, GNRL only).

The header is 32 bytes: magic 'BTDX', version, type ('GNRL'), file count and
the offset of the name table. Each record is 36 bytes; names live at the end of
the archive, each prefixed with a 2 byte length. Files are individually zlib
compressed, and a packed size of zero means the file is stored uncompressed.

Usage:
  ba2.py list <archive.ba2> [pattern]
  ba2.py extract <archive.ba2> <pattern> <destination>
"""
import struct
import sys
import os
import zlib


def open_archive(path):
    f = open(path, "rb")
    magic, version, kind, count, names_offset = struct.unpack("<4sI4sIQ", f.read(24))
    if magic != b"BTDX":
        raise ValueError(f"not a BA2 archive: {magic!r}")
    if kind != b"GNRL":
        raise ValueError(f"archive type {kind!r} is not supported (GNRL only)")

    # The remainder of the 32 byte header differs between versions and is unused.
    f.seek(32)
    records = [struct.unpack("<I4sIIQIII", f.read(36)) for _ in range(count)]

    f.seek(names_offset)
    names = []
    for _ in range(count):
        (length,) = struct.unpack("<H", f.read(2))
        names.append(f.read(length).decode("cp1252").replace("\\", "/").lower())

    entries = []
    for name, record in zip(names, records):
        _hash, _ext, _dir_hash, _flags, offset, packed, unpacked, _align = record
        entries.append({"path": name, "offset": offset,
                        "packed": packed, "unpacked": unpacked})
    return f, entries


def read_entry(f, entry):
    f.seek(entry["offset"])
    if entry["packed"]:
        return zlib.decompress(f.read(entry["packed"]))
    return f.read(entry["unpacked"])


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    command, archive = sys.argv[1], sys.argv[2]
    f, entries = open_archive(archive)
    pattern = sys.argv[3].lower() if len(sys.argv) > 3 else ""

    if command == "list":
        matched = 0
        for entry in entries:
            if pattern in entry["path"]:
                print(f"{entry['unpacked']:>10}  {entry['path']}")
                matched += 1
        print(f"--- {matched} of {len(entries)} files", file=sys.stderr)
    elif command == "extract":
        destination = sys.argv[4]
        for entry in entries:
            if pattern in entry["path"]:
                target = os.path.join(destination, entry["path"])
                os.makedirs(os.path.dirname(target), exist_ok=True)
                with open(target, "wb") as out:
                    out.write(read_entry(f, entry))
                print(target)
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
