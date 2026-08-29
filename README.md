# 360 Directional Movement

An SFSE plugin for Starfield. The character turns to face the direction it is
actually moving, instead of backpedalling or strafing with the body locked to
the camera. The camera is untouched.

> **Work in progress.** Stable and playable, with a few rough edges still being
> worked on.

## Requirements

Starfield 1.16.244, SFSE, and Address Library for SFSE Plugins.

## Where it acts

Walking, jogging and jumping, with the weapon put away. Sprinting, climbing a
ladder, zero g, sitting and having a weapon drawn are left alone: the game keeps
the body it would have had without the mod installed.

## Switching it off in play

**F11** turns the mod off and back on without leaving the game, for the places
where it still misbehaves: ladders, zero g, sitting down in a cockpit.

To move it elsewhere, put a
[virtual key code](https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes)
in `Documents/My Games/Starfield/SFSE/sf360.ini`, the folder the log is written
to. `0` removes the shortcut.

```
toggleKey=122
```

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

MIT. Do what you like with it, including bundling it in a mod pack, as long as
the copyright notice goes along.
