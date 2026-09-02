# Changelog

## 0.2.2

### Fixed

- The walk no longer slides at the start of a movement
- Tapping a direction key no longer swings the body round

### Changed

- The body is left to the game while sneaking

### Known issues

- A half turn can make the character duck partway through
- A reversal can end a short step to the side of where it started
- Movement start animations are blank, so a sharp start looks wrong
- The body drifts off course during a jump

## 0.2.1

### Fixed

- Turning is no longer abrupt: it accelerates and eases into place
- Turning speed no longer changes with the frame rate
- A quick change of direction no longer leaves the body facing the old way

### Known issues

- The turn does not look quite like the one the game plays for itself
- A half turn can make the character duck partway through
- The body drifts off course during a jump
- Movement start animations are blank, so a sharp start looks wrong

## 0.2.0

### Added

- F11 switches the mod off and back on without leaving the game

### Changed

- The body turns only while walking, jogging and jumping with the weapon away

### Fixed

- Ladders, zero g and a drawn weapon, by leaving all three to the game

### Known issues

- Turning is abrupt, most noticeably with keyboard and mouse

## 0.1.1

### Fixed

- Sitting no longer leaves the character crooked, most visibly in a cockpit
- The body is no longer rotated in first person

## 0.1.0

First release.
