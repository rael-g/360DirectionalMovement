"""Address Library reader (versionlib-*.bin), format V5.

V5, which Starfield 1.16.244 uses, is a 96 byte header (fileVersion int32,
gameVersion uint32[4], name char[64], pointerSize int32, dataFormat int32,
offsetCount int32) followed by a flat uint32 array indexed directly by ID.
An offset of zero means the ID does not exist in that build.

Usage: versionlib.py <file.bin> [id ...]
"""
import struct
import sys

HEADER_SIZE_V5 = 4 + 16 + 64 + 4 + 4 + 4


def load(path):
    with open(path, "rb") as f:
        data = f.read()

    (file_format,) = struct.unpack_from("<i", data, 0)
    if file_format != 5:
        raise ValueError(f"format {file_format} is not supported (V5 only)")

    version = struct.unpack_from("<4I", data, 4)
    name = data[20:84].split(b"\0")[0].decode("utf-8", "replace")
    pointer_size, data_format, count = struct.unpack_from("<3i", data, 84)

    offsets = struct.unpack_from(f"<{count}I", data, HEADER_SIZE_V5)

    return {"version": version, "name": name, "pointer_size": pointer_size,
            "count": count, "offsets": offsets}


def offset_of(db, identifier):
    if identifier >= db["count"]:
        return None
    return db["offsets"][identifier] or None


if __name__ == "__main__":
    db = load(sys.argv[1])
    print(f"version={'.'.join(map(str, db['version']))} name={db['name']!r} "
          f"pointer_size={db['pointer_size']} entries={db['count']}")
    print(f"ids with an offset: {sum(1 for o in db['offsets'] if o)}")
    for argument in sys.argv[2:]:
        identifier = int(argument)
        value = offset_of(db, identifier)
        print(f"  id {identifier:<10} -> {'0x%X' % value if value else 'NOT FOUND'}")
