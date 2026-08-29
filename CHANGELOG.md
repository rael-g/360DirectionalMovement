# Changelog

## 0.2.0

### Added

- F11 switches the mod off and back on without leaving the game

### Changed

- The body is turned only while walking, jogging and jumping with the weapon
  put away. Sprinting, ladders, zero g, sitting and a drawn weapon are left to
  the game. This is how the entries below stop misbehaving: the mod stands down
  in those situations rather than handling them.

### Fixed

- Climbing a ladder works again. 0.1.x turned the body during the climb, which
  could leave the character unable to get off at a middle floor.
- Zero g works again. 0.1.x spun the character on the spot.
- A drawn weapon no longer leaves the body pointing away from the camera, which
  made aiming while moving backwards impossible to shoot from.

### Known issues

- Turning is abrupt, most noticeably with keyboard and mouse. Unchanged in this
  release.

## 0.1.1

### Fixed

- Sitting no longer leaves the character crooked, most visibly in a cockpit
- The body is no longer rotated in first person

## 0.1.0

First release.
