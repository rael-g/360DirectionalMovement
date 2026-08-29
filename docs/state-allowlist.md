# The state allowlist

Measured in play with the probe builds, 0.2.0-rc2 through rc4. Everything here
is an observation, not a reading of `anim_variables.xtbl`: the table declares
variables for every graph in the game, and being declared there turned out to
say nothing about being present in the player's.

## Absent from the player's graph

Read as unavailable throughout, despite being declared in the table:
`CurrentGraphState`, `iSyncIdleWalkRun`, `bCombatWalk`, `iSyncCoverStates`,
`iLadderClimbState`, `iPlayerLadderClimbAnimation`, `iSyncMoveDirection`,
`iSyncStandingCrouching`, `bFreeMovement`.

Present but never leaving zero, through sprints and jumps both: `iState`,
`bIsInAir`.

This is the `bIsFirstPerson` failure in another shape. A wrong name reads as a
state that is simply never entered, and nothing distinguishes the two without a
probe that reports unavailability apart from zero.

## Carrying signal

| variable | observed |
|---|---|
| `iSyncIdleLocomotion` | 0 standing, 1 for the whole time the character moves on foot, jumps included |
| `iSyncSprintState` | 1 for the whole sprint |
| `iPlayingSprintAnimation` | pulses to 2 at the start of a sprint |
| `iSyncJumpState` | 0 grounded, 1 then 2 through a jump, 3 once on a longer one |
| `bPlayerMoveStartActive` | a single frame pulse when a movement starts |
| `iIsSighted` | 1 exactly while aiming down sights, with `iSyncSighted` alongside it |
| `iSightedRequested` | goes to 1 on requests that never become sighted, so it is not the aim signal |
| `bAimActive` | flips between 0 and 1 frame to frame while a weapon is out; not a state |

## The rule

    iSyncIdleLocomotion == 1 && iSyncSprintState == 0 && iSyncJumpState == 0

Walking and jogging, nothing else. Ladders, zero g and furniture are not named
anywhere in it, and do not need to be: they are excluded by not being the on
foot value.

A failed read counts as not allowed, so a name that disappears in a game update
stops the mod rather than letting it run somewhere it breaks.

## Left open

Sprint and jump are excluded rather than fixed. The sprint turn is the one users
describe as too snappy, and the jump still slides.

`iIsSighted` is the confirmed signal for the aim lock users asked for, and is
unused so far: the mod still acts armed and unarmed alike.
