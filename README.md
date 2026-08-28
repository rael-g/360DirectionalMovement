# 360 Directional Movement

Your character turns to face the direction it is actually moving.

In the base game the body stays locked to the camera, so moving backwards or
sideways plays a backpedal or a strafe. This mod turns the character to face
its movement direction so it simply runs, the way most third person action
games handle it. The camera is untouched — you still aim it freely, and the
character follows where you are going.

First person is unaffected.

> **Work in progress.** This is an early release. It is stable and playable,
> but a few rough edges remain and are being worked on. Feedback and reports
> are welcome.

## Requirements

- **Starfield 1.16.244**
- **[SFSE](https://www.nexusmods.com/starfield/mods/106)** — Starfield Script
  Extender
- **[Address Library for SFSE Plugins](https://www.nexusmods.com/starfield/mods/3256)**
  — the plugin resolves the addresses it needs from this database at load time
  and does nothing without it

Nothing else. No framework, no ESM.

## Installation

With a mod manager, install the archive as usual and enable it.

Manual installation: open the archive and copy its contents into your Starfield
`Data` folder, so that you end up with

```
Data/SFSE/Plugins/sf360.dll
Data/meshes/actors/human/animations/...
```

The `Data` folder sits next to `Starfield.exe`. On a default Steam install that
is `steamapps/common/Starfield/Data`. To uninstall, delete the files you copied.

### Enable loose files

Starfield ignores loose files unless you tell it not to, and the mod ships
animation files that have to be read from disk. If you have already done this
for another mod, skip it.

Open `Documents/My Games/Starfield/StarfieldCustom.ini`, creating it if it does
not exist, and make sure it contains:

```ini
[Archive]
bInvalidateOlderFiles=1
sResourceDataDirsFinal=
```

`sResourceDataDirsFinal` is deliberately empty. Without these two lines the
character drags at the start of a movement.

## A note on the included animation files

The archive includes empty replacements for the movement start animations.
Those animations carry root motion that fights the rotation, so blanking them
removes a slide at the start of a movement. The visual cost is negligible, and
they will be dropped once the interaction is fixed properly.

## Reporting a problem

A log is written to `Documents/My Games/Starfield/SFSE/Logs/sf360.log`. Please
attach it to any bug report.

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

## Source and licence

Source code: <https://github.com/rael-g/360DirectionalMovement>

The code is MIT licensed. The mod itself is released under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) — do what you like
with it, including bundling it in a pack, as long as you credit it.
