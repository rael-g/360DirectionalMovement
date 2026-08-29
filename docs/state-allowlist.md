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

Availability is not fixed, which took a full sweep to discover. The ladder
variables were called absent on the strength of reading unavailable while the
player stood in a city, and they are readable the moment a ladder is being
climbed: a variable exists only while the subsystem that declares it is loaded.
So unavailable means "not right now" as often as it means "wrong name", and the
earlier conclusion that these names were wrong was itself wrong.

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
| `bAimActive` | alternates 0 and 1 across the weapon part of the session |

`bAimActive` was read two wrong ways before being ruled out. First as frame to
frame flicker, which the probe cannot show, because it logs only on change.
Then as the weapon being drawn and holstered, which fitted that part of the
session. Shipped on that inference in rc6 and refuted in one run: it reads 1
with the weapon holstered and never changes, so the gate built on it disabled
the mod outright.

There is no drawn or holstered state in the player's graph. Everything the
table declares on the subject is transient: `bIsEquipping`, `bIsUnequipping`,
`UnequipInterruptable`.

It is in `ActorState` instead, in the low three bits of the second word, beside
the sit state this plugin already reads. Logged raw across four draw and
holster cycles, the field walked the same path every time:

    0 sheathed -> 2 drawing -> 3 drawn -> 5 sheathing -> 0 sheathed

Only 0 counts as weapon away, so the mod stands down from the first frame of
the draw rather than switching back on partway through the animation.

## The rule

    iSyncIdleLocomotion == 1 && iSyncSprintState == 0

plus a weapon that is sheathed. Walking, jogging and jumping.

Ladders were expected to fall out of this by not being the on foot value, and
they do not: with the allowlist active, climbing is still broken. Whatever
`iSyncIdleLocomotion` means, it is not "on foot locomotion" in the sense
assumed here, and it does not separate a ladder from a walk. Zero g is
untested and now less likely to be excluded, not more.

The claim that unknown states are safe by default is therefore unproven. It
holds for sitting and sprinting, which are tested against directly, and failed
its first real test on ladders.

A failed read counts as not allowed, so a name that disappears in a game update
stops the mod rather than letting it run somewhere it breaks.

## Left open

Sprint is excluded rather than fixed: its turn is the one users describe as too
snappy.

The mod still acts armed and unarmed alike, because nothing in the graph says
which. `iIsSighted` is confirmed and would carry an aim lock, but aiming is a
narrower state than holding a weapon, so it does not answer the request to stand
down while armed.
