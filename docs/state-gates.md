# Where the mod is allowed to act

Everything here was measured in play with the probe builds. None of it comes
from reading `anim_variables.xtbl`: the table declares names for every graph in
the game, including ones the player does not have, and says nothing about what
a value means.

## The rule

    iSyncIdleLocomotion == 1
      && not sprinting, on a ladder, in zero g, sitting, or holding a weapon

Walking, jogging and jumping. Jumping is admitted by not being mentioned:
`iSyncIdleLocomotion` holds at the on foot value throughout one.

This is a broad positive test with vetoes hung off it. It was designed as an
allowlist, on the argument that unknown states would be excluded by not being
the on foot value, and therefore that the mod would be safe in situations
nobody had reported yet. **Ladders disproved that.** They hold
`iSyncIdleLocomotion` at 1 and had to be named individually, and so did zero g.
The positive condition does not discriminate finely enough for the allowlist
property to follow, and a full sweep of all 231 state variables found nothing
that separates a walk from a climb. Each new broken situation costs a veto.

| state | signal | source |
|---|---|---|
| on foot | `iSyncIdleLocomotion == 1` | graph |
| sprinting | `iSyncSprintState == 1` | graph |
| ladder | `iLadderClimbState != -1` | graph |
| zero g | `iSyncGravity != 0`, `bZeroGSpine != 0` | graph |
| sitting | `(actorState2 >> 11) & 3` | ActorState |
| weapon out | `actorState2 & 7` | ActorState |

## Two things that cost several builds each

**Readability is not fixed.** A variable is readable only while the graph
declaring it is loaded. `iLadderClimbState` reads unavailable on a city street
and appears the moment a ladder is touched. It was written off as a wrong name
on the first reading, which sent three builds hunting for a signal that had
been there all along.

This is why the two directions of test are separate functions. For the
condition that must be true, unreadable fails it, so a name lost to a game
update stops the mod. For a veto, unreadable clears it, since a subsystem that
is not loaded cannot be the state the character is in. Getting this backwards
is what made the first ladder build disable the mod everywhere.

**Resting values are not always zero.** `iLadderClimbState` reads `-1` off a
ladder, and `0` is one of the climb states. Vetoing on `!= 0` disabled the mod
in every situation.

## ActorState, at Actor + 0xF8

The second word carries both states the graph does not expose.

Sitting, bits 11 and 12, measured across one cockpit sequence:

    0 not sitting -> 1 sitting down -> 2 seated -> 3 standing up -> 0

Weapon, bits 0 to 2, measured across four draw and holster cycles:

    0 sheathed -> 2 drawing -> 3 drawn -> 5 sheathing -> 0 sheathed

Only `0` counts as weapon away, so the mod stands down from the first frame of
the draw instead of switching back on partway through the animation.

Zero g leaves no trace in either word, which is why it is read from the graph.

## Absent from the player's graph

Read as unavailable even in the situations that would use them:
`CurrentGraphState`, `iSyncIdleWalkRun`, `bCombatWalk`, `iSyncCoverStates`,
`iSyncMoveDirection`, `iSyncStandingCrouching`, `bFreeMovement`.

Present but never leaving zero, through sprints and jumps both: `iState`,
`bIsInAir`.

`bAimActive` reads 1 with the weapon put away and was wrong twice: first read as
per frame flicker, which the probe cannot show because it logs only on change,
then as the weapon being drawn. Shipped on the second reading and refuted in one
run.

`IsUsingCodeDrivenRotation` would be the principled gate, the game naming who
owns the body's rotation. It toggles identically while walking and while
climbing a ladder, so it does not discriminate.

## Left open

Sprinting is excluded rather than fixed: its turn is the one users describe as
too snappy, which is the rate limiter in `rotator.c`, not the state gate.

`iIsSighted` is confirmed as the aim signal, reading 1 exactly while aiming down
sights. It is unused. It would support locking the body while aiming, which is
what the Nexus reports actually asked for, and is narrower than the weapon test
shipped here.
