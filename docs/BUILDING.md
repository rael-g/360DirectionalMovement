# Building

sf360 is a Windows x64 DLL with the **MSVC ABI**, cross compiled from Linux.
The ABI is mandatory rather than preferred: `Starfield.exe` and SFSE are MSVC,
so a mingw binary cannot interoperate with them.

## Source

```
git clone --recurse-submodules https://github.com/rael-g/360DirectionalMovement
```

For an existing clone: `git submodule update --init`.

## Toolchain

`clang`, `lld`, `llvm`, `cmake`, `ninja`, and
[`xwin`](https://github.com/Jake-Shadle/xwin) to fetch the MSVC CRT and the
Windows SDK from Microsoft:

```
xwin --accept-license splat --output xwin
```

`msvc.cmake` points at that directory and drives `clang-cl` and `lld-link`.

## Build

```
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/msvc.cmake sf360
cmake --build build
```

`build/sf360.dll` goes to `Data/SFSE/Plugins/`.

## Running it

The game loads the DLL, so there is no unit test loop: build, copy into
`Data/SFSE/Plugins/`, launch, and read
`Documents/My Games/Starfield/SFSE/Logs/sf360.log`.
