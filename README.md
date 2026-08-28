# 360 Directional Movement

An SFSE plugin for Starfield. The character turns to face the direction it is
actually moving, instead of backpedalling or strafing with the body locked to
the camera. The camera is untouched.

> **Work in progress.** Stable and playable, with a few rough edges still being
> worked on.

## Requirements

Starfield 1.16.244, SFSE, and Address Library for SFSE Plugins.

## Building

Any MSVC ABI toolchain. From Linux: `clang`, `lld`, `cmake`, `ninja` and
[`xwin`](https://github.com/Jake-Shadle/xwin).

```
git clone --recurse-submodules https://github.com/rael-g/360DirectionalMovement
xwin --accept-license splat --output xwin
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/msvc.cmake sf360
cmake --build build
```

More in [docs/BUILDING.md](docs/BUILDING.md).

## Reporting a problem

Open an issue here, or use the Nexus posts tab. The plugin writes
`Documents/My Games/Starfield/SFSE/Logs/sf360.log`; attach it either way.

## Licence

The code is MIT licensed. The mod itself is released under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) — do what you like
with it, including bundling it in a pack, as long as you credit it.
