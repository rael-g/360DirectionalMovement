# Changelog

## 0.1.1

### Fixed

- Sitting down no longer lands the character at the wrong angle. The plugin was
  fighting the game while it turned the body to match the chair, which swung the
  camera during the animation and left it crooked once seated. Most noticeable
  in a ship cockpit.
- First person is detected again. The check had been reading a variable that
  does not exist in this game, so it never triggered.

## 0.1.0

First release.
