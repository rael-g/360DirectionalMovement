# Turning

Two users reported that the turn is abrupt, most noticeably with keyboard and
mouse. This is what the rotator does about it, what the logs measured, and where
the approach runs out.

## What was wrong

The limit was `maxStep`, half a radian **per hook call**. The hook fires about
once per graph update, so at the measured 10.7 ms between updates that is around
2700 degrees a second. The cap never engaged. The body was matching the camera
exactly, and with a mouse the camera moves in flicks.

It was also per call rather than per second, so the same setting turned at
different speeds on different machines.

## What it does now

`turnRate` in degrees a second and `turnSmoothing` in seconds, both against a
clock read with `QueryPerformanceCounter`, since the graph update carries no
timestep this code can reach.

The chase is a critically damped spring. It carries a velocity between calls,
which is what buys the acceleration at the start of a turn: a step computed from
the remaining arc alone is at full speed on its first frame. Critical damping
means it arrives without swinging past.

Measured on a 45 degree turn, degrees per graph update:

    turnSmoothing 0.10:  2.1  3.2  4.4  3.0  3.6  2.7  3.2  3.0  1.8  1.3
    turnSmoothing 0.18:  1.0  1.6  2.0  2.0  1.7

Peak was 424 degrees a second at 0.10 and about 180 at 0.18. `turnRate` was 540
in both, so it never engaged in either. Tuning it was two wasted rounds.

## The snap at the start of a movement is load bearing

`rotator_reset` sends the first step of a movement straight to the target
instead of easing it. That was written believing the target equalled the body
angle at that moment, so the snap covered nothing. It does not: the target is
the new heading, which can be half a turn away, and the log shows single sample
jumps of 70 and 180 degrees.

Removing it looked correct and was worse in play. The body ducks mid turn
instead of braking and pivoting.

The reason is the whole approach. We rotate by writing the actor's angle, which
tells the animation graph nothing, so the legs keep running the previous cycle
while the body turns underneath them. When the game rotates the actor itself it
drives the animation with it, which is where the brake and pivot come from. An
instant turn gives the mismatch no time to appear on screen. Easing a half turn
over a third of a second puts it on display.

So the snap is not a shortcut to clean up later. It is hiding the ceiling of
writing the angle directly, and it stays until the rotation goes through
whatever path the game uses on itself.

## What the log carries

`dt` is the interval the step was scaled by, and `driven` is the angle the
rotator commanded as opposed to the one the actor carries. `driven` matching
`angle` on every sample is the evidence that nothing overwrites us after we
write, which is what ruled out the game fighting our writes.

Both exist because three rounds were spent inferring the frame time from angle
deltas and getting it wrong. The measured interval is around 10.7 ms, not the
16.7 that was assumed.

## Open

- Alternating between two directions quickly swings the body back and forth
  through the full arc each time, because every octant change recaptures the
  offset. Comparing that against the mod switched off is not a fair reference:
  with it off the body does not turn at all, so there is nothing there to be
  smooth. Hysteresis in time, ignoring a direction that does not hold for a few
  updates, is the shape of the fix and it has not been tried.
- `turnSmoothing` was last tuned against measurements taken while half the turns
  were still bypassing the spring, so the default of 0.1 deserves another look.
- Everything above is cosmetic next to the graph never being told the body
  turned.
